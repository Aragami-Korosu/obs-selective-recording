// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "core/obs-utils.hpp"

#include <obs-frontend-api.h>

#include <QCoreApplication>
#include <QThread>

#include <algorithm>

namespace selective_recording {

namespace {

CanvasInfo make_info(obs_canvas_t *canvas)
{
	CanvasInfo info;
	info.canvas = canvas;
	info.is_main = is_main_canvas(canvas);
	info.key = canvas_key(canvas);
	const char *name = obs_canvas_get_name(canvas);
	info.name = name ? name : "";

	obs_video_info ovi = {};
	if (obs_canvas_get_video_info(canvas, &ovi)) {
		info.width = ovi.base_width;
		info.height = ovi.base_height;
	}
	return info;
}

} // namespace

bool is_main_canvas(obs_canvas_t *canvas)
{
	return canvas && (obs_canvas_get_flags(canvas) & MAIN) != 0;
}

std::string canvas_key(obs_canvas_t *canvas)
{
	if (!canvas)
		return "";
	if (is_main_canvas(canvas))
		return kMainCanvasKey;
	const char *name = obs_canvas_get_name(canvas);
	return name ? name : "";
}

std::vector<CanvasInfo> list_canvases()
{
	std::vector<CanvasInfo> result;

	OBSCanvasAutoRelease main_canvas = obs_get_main_canvas();
	if (main_canvas)
		result.push_back(make_info(main_canvas));

	obs_frontend_canvas_list list = {};
	obs_frontend_get_canvases(&list);
	for (size_t i = 0; i < list.canvases.num; i++) {
		obs_canvas_t *canvas = list.canvases.array[i];
		if (!canvas || obs_canvas_removed(canvas) || !obs_canvas_has_video(canvas))
			continue;
		if (is_main_canvas(canvas))
			continue;
		result.push_back(make_info(canvas));
	}
	obs_frontend_canvas_list_free(&list);
	return result;
}

OBSCanvas find_canvas(const std::string &key)
{
	for (auto &info : list_canvases()) {
		if (info.key == key)
			return info.canvas;
	}
	return nullptr;
}

std::vector<OBSSource> canvas_scenes(obs_canvas_t *canvas)
{
	std::vector<OBSSource> scenes;
	if (!canvas)
		return scenes;

	if (is_main_canvas(canvas)) {
		obs_frontend_source_list list = {};
		obs_frontend_get_scenes(&list);
		for (size_t i = 0; i < list.sources.num; i++)
			scenes.emplace_back(list.sources.array[i]);
		obs_frontend_source_list_free(&list);
		return scenes;
	}

	obs_canvas_enum_scenes(
		canvas,
		[](void *param, obs_source_t *source) {
			auto *out = static_cast<std::vector<OBSSource> *>(param);
			if (obs_source_is_scene(source))
				out->emplace_back(source);
			return true;
		},
		&scenes);
	return scenes;
}

std::vector<OBSSource> global_audio_sources()
{
	std::vector<OBSSource> sources;
	for (uint32_t channel = 1; channel <= 6; channel++) {
		OBSSourceAutoRelease source = obs_get_output_source(channel);
		if (source)
			sources.emplace_back(source.Get());
	}
	return sources;
}

bool has_audio(obs_source_t *source)
{
	return source && (obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO) != 0;
}

bool is_monitor_only(obs_source_t *source)
{
	static const bool legacy_monitoring = obs_get_version() < MAKE_SEMANTIC_VERSION(33, 0, 0);
	return legacy_monitoring && source &&
	       obs_source_get_monitoring_type(source) == OBS_MONITORING_TYPE_MONITOR_ONLY;
}

bool has_video(obs_source_t *source)
{
	return source && (obs_source_get_output_flags(source) & OBS_SOURCE_VIDEO) != 0;
}

std::string source_uuid(obs_source_t *source)
{
	const char *uuid = source ? obs_source_get_uuid(source) : nullptr;
	return uuid ? uuid : "";
}

std::string source_name(obs_source_t *source)
{
	const char *name = source ? obs_source_get_name(source) : nullptr;
	return name ? name : "";
}

bool contains_source(const std::vector<OBSSource> &sources, obs_source_t *source)
{
	return std::any_of(sources.begin(), sources.end(), [&](const OBSSource &s) { return s.Get() == source; });
}

std::vector<OBSSource> active_tree(obs_source_t *root)
{
	std::vector<OBSSource> sources;
	if (!root)
		return sources;

	obs_source_enum_active_tree(
		root,
		[](obs_source_t *, obs_source_t *child, void *param) {
			auto *out = static_cast<std::vector<OBSSource> *>(param);
			if (!contains_source(*out, child))
				out->emplace_back(child);
		},
		&sources);
	sources.emplace_back(root);
	return sources;
}

bool on_ui_thread()
{
	QCoreApplication *app = QCoreApplication::instance();
	return app && QThread::currentThread() == app->thread();
}

void post_to_ui(std::function<void()> fn)
{
	QCoreApplication *app = QCoreApplication::instance();
	if (!app)
		return;
	QMetaObject::invokeMethod(app, std::move(fn), Qt::QueuedConnection);
}

void run_on_ui_blocking(std::function<void()> fn)
{
	if (on_ui_thread()) {
		fn();
		return;
	}
	QCoreApplication *app = QCoreApplication::instance();
	if (app)
		QMetaObject::invokeMethod(app, std::move(fn), Qt::BlockingQueuedConnection);
}

} // namespace selective_recording
