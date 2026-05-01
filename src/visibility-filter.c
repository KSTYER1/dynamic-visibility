/*
 * Dynamic Visibility filter — auto-manages scene-item visibility.
 *
 * Modes:
 *   - exclusive: at most 1 item visible at a time. Activating one auto-hides others.
 *   - max_n:     at most N items visible. Activating an (N+1)th auto-hides the
 *                oldest visible one.
 * Plus optional "always at least one visible" safety lock.
 *
 * Parent must be a scene or a group; otherwise the filter shows a warning and
 * stays inert.
 */

#include <obs-module.h>
#include <plugin-support.h>
#include <util/bmem.h>
#include <util/platform.h>

#include <stdlib.h>
#include <string.h>

#define MODE_EXCLUSIVE  "exclusive"
#define MODE_MAX_N      "max_n"

#define MAX_TRACKED     128

struct visible_entry {
	int64_t  sceneitem_id;
	uint64_t since_ns;
};

struct vis_filter {
	obs_source_t *self;

	/* settings */
	char *mode;
	int   max_n;
	bool  always_one;

	/* parent type */
	bool  parent_valid;
	obs_source_t *parent_src;
	bool  signal_connected;

	/* activation tracking for max_n eviction */
	struct visible_entry order[MAX_TRACKED];
	size_t order_count;

	/* always-one rescue */
	int64_t pending_restore_id;

	/* reentrancy guard */
	bool applying;
};

/* ---------- helpers ---------- */

static bool parent_is_scene_or_group(obs_source_t *parent)
{
	if (!parent)
		return false;
	const char *id = obs_source_get_unversioned_id(parent);
	if (!id)
		return false;
	return strcmp(id, "scene") == 0 || strcmp(id, "group") == 0;
}

static obs_scene_t *parent_as_scene(obs_source_t *parent)
{
	if (!parent)
		return NULL;
	obs_scene_t *s = obs_scene_from_source(parent);
	if (s)
		return s;
	return obs_group_from_source(parent);
}

static int find_order_index(struct vis_filter *f, int64_t id)
{
	for (size_t i = 0; i < f->order_count; i++) {
		if (f->order[i].sceneitem_id == id)
			return (int)i;
	}
	return -1;
}

static void order_touch(struct vis_filter *f, int64_t id)
{
	int idx = find_order_index(f, id);
	uint64_t now = os_gettime_ns();
	if (idx >= 0) {
		f->order[idx].since_ns = now;
		return;
	}
	if (f->order_count < MAX_TRACKED) {
		f->order[f->order_count].sceneitem_id = id;
		f->order[f->order_count].since_ns = now;
		f->order_count++;
	} else {
		/* table full — overwrite oldest */
		size_t oldest = 0;
		for (size_t i = 1; i < f->order_count; i++) {
			if (f->order[i].since_ns < f->order[oldest].since_ns)
				oldest = i;
		}
		f->order[oldest].sceneitem_id = id;
		f->order[oldest].since_ns = now;
	}
}

static void order_remove(struct vis_filter *f, int64_t id)
{
	int idx = find_order_index(f, id);
	if (idx < 0)
		return;
	for (size_t i = (size_t)idx; i + 1 < f->order_count; i++)
		f->order[i] = f->order[i + 1];
	f->order_count--;
}

/* Enumeration helpers using obs_scene_enum_items. */

struct count_visible_ctx {
	int count;
	int64_t exclude_id;
};

static bool count_visible_cb(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	(void)scene;
	struct count_visible_ctx *c = param;
	int64_t id = obs_sceneitem_get_id(item);
	if (id == c->exclude_id)
		return true;
	if (obs_sceneitem_visible(item))
		c->count++;
	return true;
}

static int count_visible(obs_scene_t *scene, int64_t exclude_id)
{
	struct count_visible_ctx c = { 0, exclude_id };
	obs_scene_enum_items(scene, count_visible_cb, &c);
	return c.count;
}

struct hide_others_ctx {
	struct vis_filter *f;
	int64_t keep_id;
};

static bool hide_others_cb(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	(void)scene;
	struct hide_others_ctx *c = param;
	int64_t id = obs_sceneitem_get_id(item);
	if (id == c->keep_id)
		return true;
	if (obs_sceneitem_visible(item)) {
		obs_sceneitem_set_visible(item, false);
		order_remove(c->f, id);
	}
	return true;
}

struct find_oldest_visible_ctx {
	struct vis_filter *f;
	int64_t keep_id;          /* don't pick this one */
	int64_t found_id;
	uint64_t found_since;
};

static bool find_oldest_cb(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	(void)scene;
	struct find_oldest_visible_ctx *c = param;
	int64_t id = obs_sceneitem_get_id(item);
	if (id == c->keep_id)
		return true;
	if (!obs_sceneitem_visible(item))
		return true;
	int idx = find_order_index(c->f, id);
	uint64_t since = (idx >= 0) ? c->f->order[idx].since_ns : 0;
	if (c->found_id == 0 || since < c->found_since) {
		c->found_id = id;
		c->found_since = since;
	}
	return true;
}

struct hide_by_id_ctx {
	int64_t target_id;
};

static bool hide_by_id_cb(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	(void)scene;
	struct hide_by_id_ctx *c = param;
	if (obs_sceneitem_get_id(item) == c->target_id) {
		obs_sceneitem_set_visible(item, false);
		return false; /* stop */
	}
	return true;
}

struct show_by_id_ctx {
	int64_t target_id;
};

static bool show_by_id_cb(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	(void)scene;
	struct show_by_id_ctx *c = param;
	if (obs_sceneitem_get_id(item) == c->target_id) {
		obs_sceneitem_set_visible(item, true);
		return false;
	}
	return true;
}

/* Snapshot all currently-visible items into the order table. */
static bool snapshot_cb(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
	(void)scene;
	struct vis_filter *f = param;
	if (obs_sceneitem_visible(item))
		order_touch(f, obs_sceneitem_get_id(item));
	return true;
}

/* ---------- mode logic ---------- */

static void apply_on_show(struct vis_filter *f, int64_t shown_id)
{
	obs_source_t *parent = f->parent_src;
	if (!parent)
		return;
	obs_scene_t *scene = parent_as_scene(parent);
	if (!scene)
		return;

	order_touch(f, shown_id);

	if (strcmp(f->mode, MODE_EXCLUSIVE) == 0) {
		f->applying = true;
		struct hide_others_ctx hc = { f, shown_id };
		obs_scene_enum_items(scene, hide_others_cb, &hc);
		f->applying = false;
	} else if (strcmp(f->mode, MODE_MAX_N) == 0) {
		int max_n = f->max_n < 1 ? 1 : f->max_n;
		/* Count how many visible, evict oldest until count <= max_n. */
		f->applying = true;
		while (1) {
			int visible_count = count_visible(scene, -1);
			if (visible_count <= max_n)
				break;
			struct find_oldest_visible_ctx fc = { f, shown_id, 0, 0 };
			obs_scene_enum_items(scene, find_oldest_cb, &fc);
			if (fc.found_id == 0)
				break; /* nothing else to hide */
			struct hide_by_id_ctx hc = { fc.found_id };
			obs_scene_enum_items(scene, hide_by_id_cb, &hc);
			order_remove(f, fc.found_id);
		}
		f->applying = false;
	}
}

static void apply_on_hide(struct vis_filter *f, int64_t hidden_id)
{
	order_remove(f, hidden_id);

	if (!f->always_one)
		return;

	obs_source_t *parent = f->parent_src;
	if (!parent)
		return;
	obs_scene_t *scene = parent_as_scene(parent);
	if (!scene)
		return;

	int visible_count = count_visible(scene, -1);
	if (visible_count == 0) {
		/* defer to next tick to avoid signal recursion */
		f->pending_restore_id = hidden_id;
	}
}

/* ---------- signal handler ---------- */

static void on_item_visible_cb(void *data, calldata_t *cd)
{
	struct vis_filter *f = data;
	if (f->applying)
		return;
	if (!obs_source_enabled(f->self))
		return;

	obs_sceneitem_t *item = NULL;
	bool visible = false;
	calldata_get_ptr(cd, "item", (void **)&item);
	calldata_get_bool(cd, "visible", &visible);
	if (!item)
		return;

	int64_t id = obs_sceneitem_get_id(item);
	if (visible)
		apply_on_show(f, id);
	else
		apply_on_hide(f, id);
}

static void connect_signal(struct vis_filter *f)
{
	if (f->signal_connected || !f->parent_src)
		return;
	signal_handler_t *sh = obs_source_get_signal_handler(f->parent_src);
	if (!sh)
		return;
	signal_handler_connect(sh, "item_visible", on_item_visible_cb, f);
	f->signal_connected = true;
}

static void disconnect_signal(struct vis_filter *f)
{
	if (!f->signal_connected || !f->parent_src)
		return;
	signal_handler_t *sh = obs_source_get_signal_handler(f->parent_src);
	if (sh)
		signal_handler_disconnect(sh, "item_visible", on_item_visible_cb, f);
	f->signal_connected = false;
}

/* ---------- OBS API callbacks ---------- */

static const char *vis_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("SourceVisibilityName");
}

static void vis_update(void *data, obs_data_t *settings)
{
	struct vis_filter *f = data;

	bfree(f->mode);
	f->mode = bstrdup(obs_data_get_string(settings, "mode"));
	if (!f->mode || !*f->mode) {
		bfree(f->mode);
		f->mode = bstrdup(MODE_EXCLUSIVE);
	}
	f->max_n = (int)obs_data_get_int(settings, "max_n");
	if (f->max_n < 1)
		f->max_n = 1;
	f->always_one = obs_data_get_bool(settings, "always_one");

	/* Re-evaluate visibility after a setting change so the new rule sticks
	 * immediately. We use the most-recently-touched item in order[] as the
	 * "anchor" for apply_on_show — that's whatever the user last activated. */
	if (f->parent_valid && f->order_count > 0) {
		int64_t newest_id = f->order[0].sceneitem_id;
		uint64_t newest_since = f->order[0].since_ns;
		for (size_t i = 1; i < f->order_count; i++) {
			if (f->order[i].since_ns > newest_since) {
				newest_since = f->order[i].since_ns;
				newest_id = f->order[i].sceneitem_id;
			}
		}
		apply_on_show(f, newest_id);
	}
}

static void *vis_create(obs_data_t *settings, obs_source_t *source)
{
	struct vis_filter *f = bzalloc(sizeof(struct vis_filter));
	f->self = source;
	f->mode = bstrdup(MODE_EXCLUSIVE);
	f->max_n = 2;

	/* Note: parent is NOT yet attached at create time. We set parent_valid
	 * and connect the signal in vis_filter_add (called by OBS once attached). */

	vis_update(f, settings);
	return f;
}

/* Called by OBS after the filter has been attached to a source.
 * This is where we can finally inspect and use the parent. */
static void vis_filter_add(void *data, obs_source_t *parent)
{
	struct vis_filter *f = data;
	f->parent_src = parent;
	f->parent_valid = parent_is_scene_or_group(parent);

	if (f->parent_valid) {
		connect_signal(f);
		obs_scene_t *scene = parent_as_scene(parent);
		if (scene)
			obs_scene_enum_items(scene, snapshot_cb, f);
	}
}

static void vis_filter_remove(void *data, obs_source_t *parent)
{
	UNUSED_PARAMETER(parent);
	struct vis_filter *f = data;
	disconnect_signal(f);
	f->parent_valid = false;
	f->parent_src = NULL;
	f->order_count = 0;
}

static void vis_destroy(void *data)
{
	struct vis_filter *f = data;
	if (!f)
		return;
	disconnect_signal(f);
	bfree(f->mode);
	bfree(f);
}

static void vis_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "mode", MODE_EXCLUSIVE);
	obs_data_set_default_int(settings, "max_n", 2);
	obs_data_set_default_bool(settings, "always_one", false);
}

/* ---------- properties ---------- */

static bool mode_modified(obs_properties_t *props, obs_property_t *prop, obs_data_t *settings)
{
	UNUSED_PARAMETER(prop);
	const char *m = obs_data_get_string(settings, "mode");
	bool show_n = m && strcmp(m, MODE_MAX_N) == 0;
	obs_property_set_visible(obs_properties_get(props, "max_n"), show_n);
	return true;
}

static obs_properties_t *vis_properties(void *data)
{
	struct vis_filter *f = data;
	obs_properties_t *props = obs_properties_create();

	if (!f || !f->parent_valid) {
		/* Warning-only UI for misapplied filter. */
		obs_property_t *warn = obs_properties_add_text(props, "warning",
			obs_module_text("WarningTitle"), OBS_TEXT_INFO);
		obs_property_set_long_description(warn, obs_module_text("WarningHelp"));
		return props;
	}

	obs_property_t *p_mode = obs_properties_add_list(props, "mode",
		obs_module_text("Mode"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p_mode, obs_module_text("ModeExclusive"), MODE_EXCLUSIVE);
	obs_property_list_add_string(p_mode, obs_module_text("ModeMaxN"), MODE_MAX_N);
	obs_property_set_modified_callback(p_mode, mode_modified);

	obs_property_t *p_max_n =
		obs_properties_add_int(props, "max_n", obs_module_text("MaxN"), 1, 20, 1);
	obs_property_set_visible(p_max_n, f && f->mode && strcmp(f->mode, MODE_MAX_N) == 0);
	obs_properties_add_bool(props, "always_one", obs_module_text("AlwaysOne"));

	return props;
}

/* ---------- tick + render ---------- */

static void vis_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	struct vis_filter *f = data;
	if (!f->parent_valid)
		return;
	if (f->pending_restore_id != 0) {
		obs_scene_t *scene = parent_as_scene(f->parent_src);
		if (scene) {
			f->applying = true;
			struct show_by_id_ctx c = { f->pending_restore_id };
			obs_scene_enum_items(scene, show_by_id_cb, &c);
			f->applying = false;
			order_touch(f, f->pending_restore_id);
		}
		f->pending_restore_id = 0;
	}
}

static void vis_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct vis_filter *f = data;
	obs_source_skip_video_filter(f->self);
}

struct obs_source_info dynamic_visibility_filter_info = {
	.id             = "dynamic_visibility_filter",
	.type           = OBS_SOURCE_TYPE_FILTER,
	.output_flags   = OBS_SOURCE_VIDEO,
	.get_name       = vis_get_name,
	.create         = vis_create,
	.destroy        = vis_destroy,
	.update         = vis_update,
	.get_defaults   = vis_defaults,
	.get_properties = vis_properties,
	.filter_add     = vis_filter_add,
	.filter_remove  = vis_filter_remove,
	.video_tick     = vis_video_tick,
	.video_render   = vis_video_render,
};
