/*
Dynamic Visibility — OBS filter that auto-manages scene-item visibility
according to a chosen rule (Exclusive / At-most-N / Always-one-visible).
Copyright (C) 2026 K_STYER, GPLv2 or later.
*/

#include <obs-module.h>
#include <plugin-support.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return "Visibility controllers for scene items and source filters.";
}

MODULE_EXPORT const char *obs_module_name(void)
{
	return "Dynamic Visibility";
}

MODULE_EXPORT const char *obs_module_author(void)
{
	return "K_STYER";
}

extern struct obs_source_info dynamic_visibility_filter_info;
extern struct obs_source_info dynamic_filter_visibility_filter_info;

bool obs_module_load(void)
{
	obs_register_source(&dynamic_visibility_filter_info);
	obs_register_source(&dynamic_filter_visibility_filter_info);
	obs_log(LOG_INFO, "plugin loaded successfully (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_unload(void)
{
	obs_log(LOG_INFO, "plugin unloaded");
}
