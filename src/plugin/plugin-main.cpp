// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "plugin/plugin-main.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <plugin-support.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

void log_canvas(obs_canvas_t *canvas)
{
	obs_video_info ovi = {};
	if (obs_canvas_get_video_info(canvas, &ovi))
		obs_log(LOG_INFO, "canvas '%s' (%ux%u)", obs_canvas_get_name(canvas), ovi.base_width, ovi.base_height);
	else
		obs_log(LOG_INFO, "canvas '%s' (no video)", obs_canvas_get_name(canvas));
}

// Logs the main canvas plus any added by the frontend or plugins such as Aitum Vertical.
void log_canvases()
{
	OBSCanvasAutoRelease main_canvas = obs_get_main_canvas();
	log_canvas(main_canvas);

	obs_frontend_canvas_list list = {};
	obs_frontend_get_canvases(&list);
	for (size_t i = 0; i < list.canvases.num; i++)
		log_canvas(list.canvases.array[i]);
	obs_frontend_canvas_list_free(&list);
}

void on_frontend_event(enum obs_frontend_event event, void *)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		obs_log(LOG_INFO, "frontend ready");
		log_canvases();
		break;
	case OBS_FRONTEND_EVENT_CANVAS_ADDED:
	case OBS_FRONTEND_EVENT_CANVAS_REMOVED:
		log_canvases();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		obs_log(LOG_INFO, "frontend exiting");
		break;
	default:
		break;
	}
}

} // namespace

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("PluginName");
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("PluginDescription");
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_post_load(void)
{
	// Frontend callbacks are registered after all modules load so the UI is available.
	obs_frontend_add_event_callback(on_frontend_event, nullptr);
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(on_frontend_event, nullptr);
	obs_log(LOG_INFO, "unloaded");
}
