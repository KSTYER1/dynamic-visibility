/*
 * Filter Visibility filter - auto-manages sibling filter enabled states.
 */

#include <obs-module.h>
#include <plugin-support.h>
#include <util/bmem.h>
#include <util/platform.h>
#include <util/threading.h>

#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define MODE_EXCLUSIVE "exclusive"
#define MODE_MAX_N "max_n"

#define CONTROL_ALL_EXCEPT_SELECTED "all_except_selected"
#define CONTROL_ONLY_SELECTED "only_selected"

#define SOURCE_VISIBILITY_ID "dynamic_visibility_filter"
#define FILTER_VISIBILITY_ID "dynamic_filter_visibility_filter"

#define MAX_TRACKED 128

struct tracked_filter {
	char *name;
	uint64_t since_ns;
	bool last_enabled;
};

struct filter_vis {
	obs_source_t *self;
	pthread_mutex_t state_mutex;
	obs_weak_source_t *parent_src;

	char *mode;
	char *control_mode;
	int max_n;
	bool always_one;

	char *selected[MAX_TRACKED];
	size_t selected_count;

	struct tracked_filter order[MAX_TRACKED];
	size_t order_count;

	char *pending_restore_name;
	bool applying;
};

static bool init_recursive_mutex(pthread_mutex_t *mutex)
{
	pthread_mutexattr_t attr;
	if (pthread_mutexattr_init(&attr) != 0)
		return false;
	pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
	int ret = pthread_mutex_init(mutex, &attr);
	pthread_mutexattr_destroy(&attr);
	return ret == 0;
}

struct name_list {
	char *names[MAX_TRACKED];
	size_t count;
};

static uint64_t fnv1a64(const char *s)
{
	uint64_t h = UINT64_C(14695981039346656037);
	while (s && *s) {
		h ^= (uint8_t)*s++;
		h *= UINT64_C(1099511628211);
	}
	return h;
}

static void make_select_key(const char *name, char *out, size_t out_size)
{
	snprintf(out, out_size, "filter_sel_%016" PRIx64, fnv1a64(name));
}

static bool streq(const char *a, const char *b)
{
	return a && b && strcmp(a, b) == 0;
}

static obs_source_t *get_parent_ref(struct filter_vis *f)
{
	return f && f->parent_src ? obs_weak_source_get_source(f->parent_src) : NULL;
}

static void name_list_clear(struct name_list *list)
{
	for (size_t i = 0; i < list->count; i++)
		bfree(list->names[i]);
	list->count = 0;
}

static void name_list_add(struct name_list *list, const char *name)
{
	if (!list || !name || !*name || list->count >= MAX_TRACKED)
		return;
	list->names[list->count++] = bstrdup(name);
}

static bool is_visibility_filter(obs_source_t *source)
{
	const char *id = obs_source_get_unversioned_id(source);
	if (!id)
		id = obs_source_get_id(source);
	return streq(id, SOURCE_VISIBILITY_ID) || streq(id, FILTER_VISIBILITY_ID);
}

static void selected_clear(struct filter_vis *f)
{
	for (size_t i = 0; i < f->selected_count; i++)
		bfree(f->selected[i]);
	f->selected_count = 0;
}

static void selected_add(struct filter_vis *f, const char *name)
{
	if (!name || !*name || f->selected_count >= MAX_TRACKED)
		return;
	f->selected[f->selected_count++] = bstrdup(name);
}

static bool selected_contains(struct filter_vis *f, const char *name)
{
	for (size_t i = 0; i < f->selected_count; i++) {
		if (streq(f->selected[i], name))
			return true;
	}
	return false;
}

static void order_clear(struct filter_vis *f)
{
	for (size_t i = 0; i < f->order_count; i++)
		bfree(f->order[i].name);
	f->order_count = 0;
}

static int order_find(struct filter_vis *f, const char *name)
{
	for (size_t i = 0; i < f->order_count; i++) {
		if (streq(f->order[i].name, name))
			return (int)i;
	}
	return -1;
}

static void order_remove(struct filter_vis *f, const char *name)
{
	int idx = order_find(f, name);
	if (idx < 0)
		return;

	bfree(f->order[idx].name);
	for (size_t i = (size_t)idx; i + 1 < f->order_count; i++)
		f->order[i] = f->order[i + 1];
	f->order_count--;
}

static void order_touch(struct filter_vis *f, const char *name)
{
	int idx = order_find(f, name);
	uint64_t now = os_gettime_ns();

	if (idx >= 0) {
		f->order[idx].since_ns = now;
		f->order[idx].last_enabled = true;
		return;
	}

	if (f->order_count < MAX_TRACKED) {
		f->order[f->order_count].name = bstrdup(name);
		f->order[f->order_count].since_ns = now;
		f->order[f->order_count].last_enabled = true;
		f->order_count++;
		return;
	}

	size_t oldest = 0;
	for (size_t i = 1; i < f->order_count; i++) {
		if (f->order[i].since_ns < f->order[oldest].since_ns)
			oldest = i;
	}

	bfree(f->order[oldest].name);
	f->order[oldest].name = bstrdup(name);
	f->order[oldest].since_ns = now;
	f->order[oldest].last_enabled = true;
}

static void order_set_enabled(struct filter_vis *f, const char *name, bool enabled)
{
	int idx = order_find(f, name);
	if (idx >= 0) {
		f->order[idx].last_enabled = enabled;
		if (enabled)
			f->order[idx].since_ns = os_gettime_ns();
		return;
	}

	if (f->order_count >= MAX_TRACKED)
		return;

	f->order[f->order_count].name = bstrdup(name);
	f->order[f->order_count].since_ns = enabled ? os_gettime_ns() : 0;
	f->order[f->order_count].last_enabled = enabled;
	f->order_count++;
}

static bool is_controlled_filter(struct filter_vis *f, obs_source_t *child)
{
	if (!f || !child || child == f->self)
		return false;
	if (is_visibility_filter(child))
		return false;

	const char *name = obs_source_get_name(child);
	if (!name || !*name)
		return false;

	bool selected = selected_contains(f, name);
	if (streq(f->control_mode, CONTROL_ONLY_SELECTED))
		return selected;
	return !selected;
}

struct selection_ctx {
	struct filter_vis *f;
	obs_data_t *settings;
};

static void collect_selected_cb(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);

	struct selection_ctx *ctx = param;
	struct filter_vis *f = ctx->f;
	if (!child || child == f->self || is_visibility_filter(child))
		return;

	const char *name = obs_source_get_name(child);
	if (!name || !*name)
		return;

	char key[64];
	make_select_key(name, key, sizeof(key));
	if (obs_data_get_bool(ctx->settings, key))
		selected_add(f, name);
}

static void refresh_selected_from_settings(struct filter_vis *f, obs_data_t *settings)
{
	selected_clear(f);
	if (!settings)
		return;

	obs_source_t *parent = get_parent_ref(f);
	if (!parent)
		return;

	struct selection_ctx ctx = { f, settings };
	obs_source_enum_filters(parent, collect_selected_cb, &ctx);
	obs_source_release(parent);
}

struct snapshot_ctx {
	struct filter_vis *f;
};

static void snapshot_cb(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);

	struct snapshot_ctx *ctx = param;
	struct filter_vis *f = ctx->f;
	if (!is_controlled_filter(f, child))
		return;

	const char *name = obs_source_get_name(child);
	bool enabled = obs_source_enabled(child);
	order_set_enabled(f, name, enabled);
}

static void rebuild_snapshot(struct filter_vis *f)
{
	order_clear(f);
	obs_source_t *parent = get_parent_ref(f);
	if (!parent)
		return;

	struct snapshot_ctx ctx = { f };
	obs_source_enum_filters(parent, snapshot_cb, &ctx);
	obs_source_release(parent);
}

struct count_ctx {
	struct filter_vis *f;
	int count;
};

static void count_active_cb(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);

	struct count_ctx *ctx = param;
	if (is_controlled_filter(ctx->f, child) && obs_source_enabled(child))
		ctx->count++;
}

static int count_active(struct filter_vis *f)
{
	struct count_ctx ctx = { f, 0 };
	obs_source_t *parent = get_parent_ref(f);
	if (parent) {
		obs_source_enum_filters(parent, count_active_cb, &ctx);
		obs_source_release(parent);
	}
	return ctx.count;
}

static void set_filter_enabled_by_name(struct filter_vis *f, const char *name, bool enabled)
{
	if (!name || !*name)
		return;

	obs_source_t *parent = get_parent_ref(f);
	if (!parent)
		return;

	obs_source_t *child = obs_source_get_filter_by_name(parent, name);
	obs_source_release(parent);
	if (!child)
		return;

	if (is_controlled_filter(f, child)) {
		const char *child_name = obs_source_get_name(child);
		obs_source_set_enabled(child, enabled);
		order_set_enabled(f, child_name, enabled);
	}

	obs_source_release(child);
}

struct hide_others_ctx {
	struct filter_vis *f;
	const char *keep_name;
	struct name_list *to_hide;
};

static void hide_others_cb(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);

	struct hide_others_ctx *ctx = param;
	if (!is_controlled_filter(ctx->f, child))
		return;

	const char *name = obs_source_get_name(child);
	if (streq(name, ctx->keep_name))
		return;

	if (obs_source_enabled(child))
		name_list_add(ctx->to_hide, name);
}

struct oldest_ctx {
	struct filter_vis *f;
	const char *keep_name;
	char *found_name;
	uint64_t found_since;
};

static void find_oldest_cb(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);

	struct oldest_ctx *ctx = param;
	if (!is_controlled_filter(ctx->f, child) || !obs_source_enabled(child))
		return;

	const char *name = obs_source_get_name(child);
	if (!name || streq(name, ctx->keep_name))
		return;

	int idx = order_find(ctx->f, name);
	uint64_t since = idx >= 0 ? ctx->f->order[idx].since_ns : 0;
	if (!ctx->found_name || since < ctx->found_since) {
		bfree(ctx->found_name);
		ctx->found_name = bstrdup(name);
		ctx->found_since = since;
	}
}

static void apply_on_enable(struct filter_vis *f, const char *enabled_name)
{
	if (!enabled_name)
		return;

	obs_source_t *parent = get_parent_ref(f);
	if (!parent)
		return;

	order_touch(f, enabled_name);
	f->applying = true;

	if (streq(f->mode, MODE_EXCLUSIVE)) {
		struct name_list to_hide = { 0 };
		struct hide_others_ctx ctx = { f, enabled_name, &to_hide };
		obs_source_enum_filters(parent, hide_others_cb, &ctx);
		for (size_t i = 0; i < to_hide.count; i++) {
			set_filter_enabled_by_name(f, to_hide.names[i], false);
			order_remove(f, to_hide.names[i]);
		}
		name_list_clear(&to_hide);
	} else if (streq(f->mode, MODE_MAX_N)) {
		int max_n = f->max_n < 1 ? 1 : f->max_n;
		while (count_active(f) > max_n) {
			struct oldest_ctx ctx = { f, enabled_name, NULL, 0 };
			obs_source_enum_filters(parent, find_oldest_cb, &ctx);
			if (!ctx.found_name)
				break;
			set_filter_enabled_by_name(f, ctx.found_name, false);
			order_remove(f, ctx.found_name);
			bfree(ctx.found_name);
		}
	}

	obs_source_release(parent);
	f->applying = false;
}

static void apply_current_policy(struct filter_vis *f)
{
	if (!f || !obs_source_enabled(f->self))
		return;

	int limit = 0;
	if (streq(f->mode, MODE_EXCLUSIVE))
		limit = 1;
	else if (streq(f->mode, MODE_MAX_N))
		limit = f->max_n < 1 ? 1 : f->max_n;
	else
		return;

	obs_source_t *parent = get_parent_ref(f);
	if (!parent)
		return;

	f->applying = true;
	while (count_active(f) > limit) {
		struct oldest_ctx ctx = { f, NULL, NULL, 0 };
		obs_source_enum_filters(parent, find_oldest_cb, &ctx);
		if (!ctx.found_name)
			break;
		set_filter_enabled_by_name(f, ctx.found_name, false);
		order_remove(f, ctx.found_name);
		bfree(ctx.found_name);
	}
	f->applying = false;

	obs_source_release(parent);
}

static void apply_on_disable(struct filter_vis *f, const char *disabled_name)
{
	order_remove(f, disabled_name);

	if (!f->always_one || count_active(f) > 0)
		return;

	bfree(f->pending_restore_name);
	f->pending_restore_name = bstrdup(disabled_name);
}

struct detect_ctx {
	struct filter_vis *f;
	char *changed_name;
	bool changed_enabled;
};

static void detect_change_cb(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);

	struct detect_ctx *ctx = param;
	struct filter_vis *f = ctx->f;
	if (ctx->changed_name || !is_controlled_filter(f, child))
		return;

	const char *name = obs_source_get_name(child);
	bool enabled = obs_source_enabled(child);
	int idx = order_find(f, name);

	if (idx < 0) {
		order_set_enabled(f, name, enabled);
		if (enabled) {
			order_touch(f, name);
			ctx->changed_name = bstrdup(name);
			ctx->changed_enabled = true;
		}
		return;
	}

	if (f->order[idx].last_enabled != enabled) {
		ctx->changed_name = bstrdup(name);
		ctx->changed_enabled = enabled;
	}
}

static const char *fv_get_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("FilterVisibilityName");
}

static void fv_update(void *data, obs_data_t *settings)
{
	struct filter_vis *f = data;
	pthread_mutex_lock(&f->state_mutex);

	bfree(f->mode);
	f->mode = bstrdup(obs_data_get_string(settings, "mode"));
	if (!f->mode || !*f->mode) {
		bfree(f->mode);
		f->mode = bstrdup(MODE_EXCLUSIVE);
	}

	bfree(f->control_mode);
	f->control_mode = bstrdup(obs_data_get_string(settings, "control_mode"));
	if (!f->control_mode || !*f->control_mode) {
		bfree(f->control_mode);
		f->control_mode = bstrdup(CONTROL_ALL_EXCEPT_SELECTED);
	}

	f->max_n = (int)obs_data_get_int(settings, "max_n");
	if (f->max_n < 1)
		f->max_n = 1;
	f->always_one = obs_data_get_bool(settings, "always_one");

	refresh_selected_from_settings(f, settings);
	rebuild_snapshot(f);
	apply_current_policy(f);
	pthread_mutex_unlock(&f->state_mutex);
}

static void *fv_create(obs_data_t *settings, obs_source_t *source)
{
	struct filter_vis *f = bzalloc(sizeof(struct filter_vis));
	if (!init_recursive_mutex(&f->state_mutex)) {
		bfree(f);
		return NULL;
	}
	f->self = source;
	f->mode = bstrdup(MODE_EXCLUSIVE);
	f->control_mode = bstrdup(CONTROL_ALL_EXCEPT_SELECTED);
	f->max_n = 2;

	fv_update(f, settings);
	return f;
}

static void fv_filter_add(void *data, obs_source_t *parent)
{
	struct filter_vis *f = data;
	pthread_mutex_lock(&f->state_mutex);
	if (f->parent_src)
		obs_weak_source_release(f->parent_src);
	f->parent_src = obs_source_get_weak_source(parent);

	obs_data_t *settings = obs_source_get_settings(f->self);
	if (settings) {
		fv_update(f, settings);
		obs_data_release(settings);
	} else {
		rebuild_snapshot(f);
		apply_current_policy(f);
	}
	pthread_mutex_unlock(&f->state_mutex);
}

static void fv_filter_remove(void *data, obs_source_t *parent)
{
	UNUSED_PARAMETER(parent);

	struct filter_vis *f = data;
	pthread_mutex_lock(&f->state_mutex);
	if (f->parent_src) {
		obs_weak_source_release(f->parent_src);
		f->parent_src = NULL;
	}
	selected_clear(f);
	order_clear(f);
	bfree(f->pending_restore_name);
	f->pending_restore_name = NULL;
	pthread_mutex_unlock(&f->state_mutex);
}

static void fv_destroy(void *data)
{
	struct filter_vis *f = data;
	if (!f)
		return;

	pthread_mutex_lock(&f->state_mutex);
	selected_clear(f);
	order_clear(f);
	if (f->parent_src)
		obs_weak_source_release(f->parent_src);
	bfree(f->pending_restore_name);
	bfree(f->mode);
	bfree(f->control_mode);
	pthread_mutex_unlock(&f->state_mutex);
	pthread_mutex_destroy(&f->state_mutex);
	bfree(f);
}

static void fv_defaults(obs_data_t *settings)
{
	obs_data_set_default_string(settings, "mode", MODE_EXCLUSIVE);
	obs_data_set_default_string(settings, "control_mode", CONTROL_ALL_EXCEPT_SELECTED);
	obs_data_set_default_int(settings, "max_n", 2);
	obs_data_set_default_bool(settings, "always_one", false);
}

static bool fv_mode_modified(obs_properties_t *props, obs_property_t *prop,
			     obs_data_t *settings)
{
	UNUSED_PARAMETER(prop);

	const char *m = obs_data_get_string(settings, "mode");
	bool show_n = m && strcmp(m, MODE_MAX_N) == 0;
	obs_property_t *p = obs_properties_get(props, "max_n");
	if (p)
		obs_property_set_visible(p, show_n);
	return true;
}

struct props_ctx {
	struct filter_vis *f;
	obs_properties_t *group;
};

static void add_filter_checkbox_cb(obs_source_t *parent, obs_source_t *child, void *param)
{
	UNUSED_PARAMETER(parent);

	struct props_ctx *ctx = param;
	struct filter_vis *f = ctx->f;
	if (!child || child == f->self || is_visibility_filter(child))
		return;

	const char *name = obs_source_get_name(child);
	if (!name || !*name)
		return;

	char key[64];
	make_select_key(name, key, sizeof(key));
	obs_properties_add_bool(ctx->group, key, name);
}

static obs_properties_t *fv_properties(void *data)
{
	struct filter_vis *f = data;
	obs_properties_t *props = obs_properties_create();
	if (f)
		pthread_mutex_lock(&f->state_mutex);

	obs_properties_add_text(props, "info", obs_module_text("FilterVisibilityInfo"),
				OBS_TEXT_INFO);

	obs_property_t *p_mode = obs_properties_add_list(
		props, "mode", obs_module_text("Mode"), OBS_COMBO_TYPE_LIST,
		OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p_mode, obs_module_text("ModeExclusive"), MODE_EXCLUSIVE);
	obs_property_list_add_string(p_mode, obs_module_text("ModeMaxN"), MODE_MAX_N);
	obs_property_set_modified_callback(p_mode, fv_mode_modified);

	obs_property_t *p_max_n =
		obs_properties_add_int(props, "max_n", obs_module_text("MaxN"), 1, 20, 1);
	obs_property_set_visible(p_max_n, f && f->mode && strcmp(f->mode, MODE_MAX_N) == 0);
	obs_properties_add_bool(props, "always_one", obs_module_text("AlwaysOne"));

	obs_property_t *p_control = obs_properties_add_list(
		props, "control_mode", obs_module_text("ControlledFilters"),
		OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p_control, obs_module_text("ControlAllExceptSelected"),
				     CONTROL_ALL_EXCEPT_SELECTED);
	obs_property_list_add_string(p_control, obs_module_text("ControlOnlySelected"),
				     CONTROL_ONLY_SELECTED);

	obs_properties_t *selection = obs_properties_create();
	obs_source_t *parent = get_parent_ref(f);
	if (parent) {
		struct props_ctx ctx = { f, selection };
		obs_source_enum_filters(parent, add_filter_checkbox_cb, &ctx);
		obs_source_release(parent);
	} else {
		obs_properties_add_text(selection, "no_parent",
					obs_module_text("FilterVisibilityNoParent"),
					OBS_TEXT_INFO);
	}
	obs_properties_add_group(props, "filter_selection", obs_module_text("FilterSelection"),
				 OBS_GROUP_NORMAL, selection);

	if (f)
		pthread_mutex_unlock(&f->state_mutex);
	return props;
}

static void fv_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	struct filter_vis *f = data;
	if (f)
		pthread_mutex_lock(&f->state_mutex);
	if (!f || !f->parent_src || !obs_source_enabled(f->self))
		goto done;

	if (f->pending_restore_name) {
		f->applying = true;
		set_filter_enabled_by_name(f, f->pending_restore_name, true);
		order_touch(f, f->pending_restore_name);
		f->applying = false;

		bfree(f->pending_restore_name);
		f->pending_restore_name = NULL;
		rebuild_snapshot(f);
		goto done;
	}

	if (f->applying)
		goto done;

	struct detect_ctx ctx = { f, NULL, false };
	obs_source_t *parent = get_parent_ref(f);
	if (!parent)
		goto done;
	obs_source_enum_filters(parent, detect_change_cb, &ctx);
	obs_source_release(parent);
	if (!ctx.changed_name)
		goto done;

	if (ctx.changed_enabled)
		apply_on_enable(f, ctx.changed_name);
	else
		apply_on_disable(f, ctx.changed_name);

	bfree(ctx.changed_name);
	rebuild_snapshot(f);

done:
	if (f)
		pthread_mutex_unlock(&f->state_mutex);
}

static void fv_video_render(void *data, gs_effect_t *effect)
{
	UNUSED_PARAMETER(effect);
	struct filter_vis *f = data;
	obs_source_skip_video_filter(f->self);
}

struct obs_source_info dynamic_filter_visibility_filter_info = {
	.id = FILTER_VISIBILITY_ID,
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = fv_get_name,
	.create = fv_create,
	.destroy = fv_destroy,
	.update = fv_update,
	.get_defaults = fv_defaults,
	.get_properties = fv_properties,
	.filter_add = fv_filter_add,
	.filter_remove = fv_filter_remove,
	.video_tick = fv_video_tick,
	.video_render = fv_video_render,
};
