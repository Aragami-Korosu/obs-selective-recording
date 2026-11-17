// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "core/live-state.hpp"

#include "core/route-store.hpp"

#include <obs-frontend-api.h>
#include <util/config-file.h>

#include <cstring>

namespace selective_recording {

namespace {

uint32_t output_tracks(obs_output_t *output)
{
	uint32_t mask = 0;
	for (size_t i = 0; i < MAX_AUDIO_MIXES; i++) {
		obs_encoder_t *encoder = obs_output_get_audio_encoder(output, i);
		if (encoder)
			mask |= 1u << obs_encoder_get_mixer_index(encoder);
	}
	return mask;
}

uint32_t configured_main_stream_tracks()
{
	config_t *config = obs_frontend_get_profile_config();
	if (!config)
		return 1;
	const char *mode = config_get_string(config, "Output", "Mode");
	if (mode && strcmp(mode, "Advanced") == 0) {
		const int64_t track = config_get_int(config, "AdvOut", "TrackIndex");
		if (track >= 1 && track <= MAX_AUDIO_MIXES)
			return 1u << (track - 1);
	}
	return 1;
}

// Walks the scene on air and records visible items whose route asks to be hidden from the stream.
void walk_visible(obs_scene_t *scene, const std::string &canvas, const RouteStore &routes,
		  std::map<std::string, SourceWarning> &out, int depth)
{
	if (!scene || depth > 16)
		return;

	struct Context {
		const std::string *canvas;
		const RouteStore *routes;
		std::map<std::string, SourceWarning> *out;
		int depth;
		const char *scene_name;
	} ctx{&canvas, &routes, &out, depth, obs_source_get_name(obs_scene_get_source(scene))};

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			auto *c = static_cast<Context *>(param);
			if (!obs_sceneitem_visible(item))
				return true;

			obs_source_t *source = obs_sceneitem_get_source(item);
			const ResolvedRoute route = c->routes->resolve_video(*c->canvas, source, item);
			if (hidden_from_stream(route.route) && has_video(source)) {
				SourceWarning &w = (*c->out)[source_uuid(source)];
				w.video_live = true;
				if (c->scene_name)
					w.scenes.emplace_back(c->scene_name);
			}

			if (obs_sceneitem_is_group(item))
				walk_visible(obs_sceneitem_group_get_scene(item), *c->canvas, *c->routes, *c->out,
					     c->depth + 1);
			else if (obs_source_is_scene(source))
				walk_visible(obs_scene_from_source(source), *c->canvas, *c->routes, *c->out,
					     c->depth + 1);
			return true;
		},
		&ctx);
}

obs_source_t *on_air_scene(obs_canvas_t *canvas)
{
	OBSSourceAutoRelease channel = obs_canvas_get_channel(canvas, 0);
	if (!channel)
		return nullptr;
	if (obs_source_get_type(channel) == OBS_SOURCE_TYPE_TRANSITION)
		return obs_transition_get_active_source(channel);
	return obs_source_get_ref(channel);
}

} // namespace

uint32_t stream_tracks(obs_canvas_t *canvas, bool *assumed)
{
	if (assumed)
		*assumed = false;

	if (is_main_canvas(canvas)) {
		OBSOutputAutoRelease output = obs_frontend_get_streaming_output();
		const uint32_t mask = output ? output_tracks(output) : 0;
		return mask ? mask : configured_main_stream_tracks();
	}

	// Streams on another canvas are outputs whose video encoder reads that canvas's video.
	struct Context {
		video_t *video;
		uint32_t mask;
	} ctx{obs_canvas_get_video(canvas), 0};

	obs_enum_outputs(
		[](void *param, obs_output_t *output) {
			auto *c = static_cast<Context *>(param);
			if (!(obs_output_get_flags(output) & OBS_OUTPUT_SERVICE))
				return true;
			obs_encoder_t *encoder = obs_output_get_video_encoder(output);
			if (encoder && obs_encoder_video(encoder) == c->video)
				c->mask |= output_tracks(output);
			return true;
		},
		&ctx);

	if (ctx.mask)
		return ctx.mask;
	if (assumed)
		*assumed = true;
	return 1;
}

WarningMap evaluate_warnings(const std::vector<CanvasInfo> &canvases, const RouteStore &routes, bool check_video,
			     bool check_audio)
{
	WarningMap result;
	std::map<std::string, uint32_t> tracks;
	std::map<std::string, bool> assumed;

	for (const CanvasInfo &info : canvases) {
		bool was_assumed = false;
		tracks[info.key] = stream_tracks(info.canvas, &was_assumed);
		assumed[info.key] = was_assumed;
	}

	const std::vector<OBSSource> globals = global_audio_sources();

	for (const CanvasInfo &info : canvases) {
		auto &warnings = result[info.key];

		OBSSourceAutoRelease scene_source = on_air_scene(info.canvas);
		if (check_video && scene_source)
			walk_visible(obs_scene_from_source(scene_source), info.key, routes, warnings, 0);

		if (!check_audio)
			continue;

		std::vector<OBSSource> audible = active_tree(scene_source);
		for (auto &g : globals) {
			if (!contains_source(audible, g))
				audible.push_back(g);
		}

		for (auto &source : audible) {
			if (!has_audio(source) || obs_source_muted(source))
				continue;
			if (is_monitor_only(source))
				continue;

			const ResolvedRoute route = routes.resolve_audio(info.key, source);
			if (!hidden_from_stream(route.route))
				continue;

			const uint32_t mixers = obs_source_get_audio_mixers(source);
			const uint32_t on_stream = mixers & tracks[info.key];
			if (!on_stream)
				continue;

			SourceWarning &w = warnings[source_uuid(source)];
			w.audio_live = true;
			w.tracks = on_stream;
			w.tracks_assumed = assumed[info.key];

			// Removing the source from a track shared with another canvas's stream would
			// also remove it there, where the user wants it.
			for (const CanvasInfo &other : canvases) {
				if (other.key == info.key)
					continue;
				const ResolvedRoute other_route = routes.resolve_audio(other.key, source);
				if (!hidden_from_stream(other_route.route) && (on_stream & tracks[other.key]))
					w.shared_track = true;
			}
		}
	}
	return result;
}

} // namespace selective_recording
