// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "api/websocket-api.hpp"

#include "core/controller.hpp"
#include "core/obs-utils.hpp"
#include "core/pipeline.hpp"
#include "core/placement.hpp"

#include <obs-websocket-api.h>
#include <plugin-support.h>

#include <algorithm>
#include <functional>

namespace selective_recording::websocket {

namespace {

obs_websocket_vendor g_vendor = nullptr;

const char *state_name(PipelineState state)
{
	switch (state) {
	case PipelineState::Idle:
		return "idle";
	case PipelineState::Recording:
		return "recording";
	case PipelineState::Paused:
		return "paused";
	case PipelineState::Stopping:
		return "stopping";
	case PipelineState::Failed:
		return "failed";
	}
	return "idle";
}

obs_data_t *pipeline_status(Pipeline *p)
{
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "canvas", p->key().c_str());
	obs_data_set_string(data, "name", p->name().c_str());
	obs_data_set_string(data, "state", state_name(p->state()));
	obs_data_set_string(data, "error", p->last_error().c_str());
	obs_data_set_string(data, "file", p->file_path().c_str());
	obs_data_set_string(data, "encoder", p->encoder_name().c_str());
	obs_data_set_bool(data, "preview", p->preview());

	const RecorderStats stats = p->stats();
	obs_data_set_int(data, "duration_ms", (long long)stats.duration_ms);
	obs_data_set_int(data, "bytes", (long long)stats.bytes);
	obs_data_set_int(data, "dropped_frames", stats.dropped_frames);
	obs_data_set_int(data, "total_frames", stats.total_frames);
	obs_data_set_int(data, "audio_latency_ms", (long long)p->audio_latency_ms());
	return data;
}

// Requests arrive on the websocket thread; plugin state lives on the UI thread.
void on_ui(obs_data_t *request, obs_data_t *response,
	   const std::function<void(Controller &, obs_data_t *, obs_data_t *)> &handler)
{
	run_on_ui_blocking([&]() {
		Controller *controller = Controller::instance();
		if (!controller) {
			obs_data_set_string(response, "error", "Selective Recording is not ready.");
			return;
		}
		handler(*controller, request, response);
	});
}

std::vector<Pipeline *> targets(Controller &c, obs_data_t *request, obs_data_t *response)
{
	const char *canvas = obs_data_get_string(request, "canvas");
	if (!canvas || !*canvas)
		return c.pipelines();

	Pipeline *p = c.pipeline(canvas);
	if (!p) {
		obs_data_set_string(response, "error", "Unknown canvas.");
		return {};
	}
	return {p};
}

OBSSource find_source(obs_data_t *request)
{
	const char *uuid = obs_data_get_string(request, "source_uuid");
	if (uuid && *uuid) {
		OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid);
		return source.Get();
	}
	const char *name = obs_data_get_string(request, "source_name");
	if (name && *name) {
		OBSSourceAutoRelease source = obs_get_source_by_name(name);
		return source.Get();
	}
	return nullptr;
}

void get_status(obs_data_t *request, obs_data_t *response, void *)
{
	on_ui(request, response, [](Controller &c, obs_data_t *, obs_data_t *res) {
		obs_data_set_bool(res, "enabled", c.settings().enabled);
		obs_data_set_string(res, "profile", c.routes().active().name.c_str());
		OBSDataArrayAutoRelease canvases = obs_data_array_create();
		for (Pipeline *p : c.pipelines()) {
			OBSDataAutoRelease entry = pipeline_status(p);
			obs_data_array_push_back(canvases, entry);
		}
		obs_data_set_array(res, "canvases", canvases);
	});
}

void start_recording(obs_data_t *request, obs_data_t *response, void *)
{
	on_ui(request, response, [](Controller &c, obs_data_t *req, obs_data_t *res) {
		bool ok = true;
		for (Pipeline *p : targets(c, req, res)) {
			p->clear_error();
			ok = p->start() && ok;
		}
		obs_data_set_bool(res, "ok", ok);
	});
}

void stop_recording(obs_data_t *request, obs_data_t *response, void *)
{
	on_ui(request, response, [](Controller &c, obs_data_t *req, obs_data_t *res) {
		for (Pipeline *p : targets(c, req, res))
			p->stop();
		obs_data_set_bool(res, "ok", true);
	});
}

void set_paused(obs_data_t *request, obs_data_t *response, bool paused)
{
	on_ui(request, response, [paused](Controller &c, obs_data_t *req, obs_data_t *res) {
		bool ok = true;
		for (Pipeline *p : targets(c, req, res))
			ok = p->set_paused(paused) && ok;
		obs_data_set_bool(res, "ok", ok);
	});
}

void pause_recording(obs_data_t *request, obs_data_t *response, void *)
{
	set_paused(request, response, true);
}

void resume_recording(obs_data_t *request, obs_data_t *response, void *)
{
	set_paused(request, response, false);
}

void set_enabled(obs_data_t *request, obs_data_t *response, void *)
{
	on_ui(request, response, [](Controller &c, obs_data_t *req, obs_data_t *res) {
		c.set_enabled(obs_data_get_bool(req, "enabled"));
		obs_data_set_bool(res, "ok", true);
	});
}

void set_route(obs_data_t *request, obs_data_t *response, void *)
{
	on_ui(request, response, [](Controller &c, obs_data_t *req, obs_data_t *res) {
		OBSSource source = find_source(req);
		if (!source) {
			obs_data_set_string(res, "error", "Unknown source.");
			return;
		}

		const std::string canvas = obs_data_get_string(req, "canvas");
		RouteStore &routes = c.routes();
		bool changed = false;

		if (obs_data_has_user_value(req, "video")) {
			routes.set_video(canvas, source, optional_route_from_string(obs_data_get_string(req, "video")));
			changed = true;
		}
		if (obs_data_has_user_value(req, "audio")) {
			routes.set_audio(canvas, source, optional_route_from_string(obs_data_get_string(req, "audio")));
			changed = true;
		}
		if (obs_data_has_user_value(req, "gain_db")) {
			routes.set_gain(canvas, source, obs_data_get_double(req, "gain_db"));
			changed = true;
		}
		if (changed)
			c.routes_changed();
		obs_data_set_bool(res, "ok", changed);
	});
}

void get_routes(obs_data_t *request, obs_data_t *response, void *)
{
	on_ui(request, response, [](Controller &c, obs_data_t *req, obs_data_t *res) {
		OBSSource source = find_source(req);
		if (!source) {
			obs_data_set_string(res, "error", "Unknown source.");
			return;
		}
		const char *canvas_arg = obs_data_get_string(req, "canvas");
		const std::string canvas = canvas_arg && *canvas_arg ? canvas_arg : kMainCanvasKey;
		obs_data_set_string(res, "video",
				    route_to_string(c.routes().resolve_video(canvas, source, nullptr).route));
		obs_data_set_string(res, "audio", route_to_string(c.routes().resolve_audio(canvas, source).route));
		obs_data_set_double(res, "gain_db", c.routes().gain_db(canvas, source));
	});
}

// A placement, by scene_uuid (any canvas, or a group) or scene_name (main canvas), and item_id.
OBSSceneItem find_item(obs_data_t *request)
{
	OBSSourceAutoRelease scene_source;
	const char *uuid = obs_data_get_string(request, "scene_uuid");
	const char *name = obs_data_get_string(request, "scene_name");
	if (uuid && *uuid)
		scene_source = obs_get_source_by_uuid(uuid);
	else if (name && *name)
		scene_source = obs_get_source_by_name(name);
	obs_scene_t *scene = obs_group_or_scene_from_source(scene_source);
	if (!scene)
		return nullptr;
	return obs_scene_find_sceneitem_by_id(scene, obs_data_get_int(request, "item_id"));
}

std::string canvas_arg(obs_data_t *request)
{
	const char *canvas = obs_data_get_string(request, "canvas");
	return canvas && *canvas ? canvas : kMainCanvasKey;
}

// Works exactly like clicking a Sources-list button: Recording only also hides the placement in
// OBS so it leaves the stream.
void set_item_route(obs_data_t *request, obs_data_t *response, void *)
{
	on_ui(request, response, [](Controller &c, obs_data_t *req, obs_data_t *res) {
		OBSSceneItem item = find_item(req);
		if (!item) {
			obs_data_set_string(res, "error", "Unknown scene item.");
			return;
		}
		const char *video = obs_data_get_string(req, "video");
		if (!video || !*video) {
			obs_data_set_string(res, "error", "Missing video route.");
			return;
		}
		// A folder has no setting of its own; like a folder button, this sets every source inside.
		const Route route = route_from_string(video);
		const bool folder = obs_sceneitem_is_group(item);
		const bool ok = folder ? set_folder_route(c, canvas_arg(req), item, route)
				       : set_placement_route(c, canvas_arg(req), item, route);
		if (!ok)
			obs_data_set_string(res, "error",
					    folder ? "The folder is locked or empty." : "The source is locked.");
		obs_data_set_bool(res, "ok", ok);
	});
}

void get_item_route(obs_data_t *request, obs_data_t *response, void *)
{
	on_ui(request, response, [](Controller &c, obs_data_t *req, obs_data_t *res) {
		OBSSceneItem item = find_item(req);
		if (!item) {
			obs_data_set_string(res, "error", "Unknown scene item.");
			return;
		}
		if (obs_sceneitem_is_group(item)) {
			const FolderRoute route = folder_route(c, canvas_arg(req), item);
			obs_data_set_bool(res, "folder", true);
			obs_data_set_string(res, "video", route_to_string(route.route));
			obs_data_set_bool(res, "mixed", route.mixed);
			obs_data_set_bool(res, "locked", route.locked);
		} else {
			const PlacementRoute route = placement_route(c, canvas_arg(req), item);
			obs_data_set_bool(res, "folder", false);
			obs_data_set_string(res, "video", route_to_string(route.route));
			obs_data_set_bool(res, "locked", route.locked);
		}
		obs_data_set_bool(res, "visible", obs_sceneitem_visible(item));
	});
}

void set_canvas_settings(obs_data_t *request, obs_data_t *response, void *)
{
	on_ui(request, response, [](Controller &c, obs_data_t *req, obs_data_t *res) {
		const char *canvas = obs_data_get_string(req, "canvas");
		if (!canvas || !*canvas || !c.pipeline(canvas)) {
			obs_data_set_string(res, "error", "Unknown canvas.");
			return;
		}

		CanvasSettings &cs = c.settings().canvas(canvas);
		if (obs_data_has_user_value(req, "enabled"))
			cs.enabled = obs_data_get_bool(req, "enabled");
		if (obs_data_has_user_value(req, "output_directory"))
			cs.output_directory = obs_data_get_string(req, "output_directory");
		if (obs_data_has_user_value(req, "video_encoder"))
			cs.video_encoder = obs_data_get_string(req, "video_encoder");
		if (obs_data_has_user_value(req, "width"))
			cs.width = (uint32_t)obs_data_get_int(req, "width") & ~1u;
		if (obs_data_has_user_value(req, "height"))
			cs.height = (uint32_t)obs_data_get_int(req, "height") & ~1u;
		if (obs_data_has_user_value(req, "fps_divisor"))
			cs.fps_divisor = (uint32_t)std::max<long long>(1, obs_data_get_int(req, "fps_divisor"));
		c.settings_changed();
		obs_data_set_bool(res, "ok", true);
	});
}

// Scenes on non-main canvases can't be found by name through obs-websocket, so expose their UUIDs.
void get_canvas_scenes(obs_data_t *request, obs_data_t *response, void *)
{
	on_ui(request, response, [](Controller &c, obs_data_t *req, obs_data_t *res) {
		Pipeline *p = c.pipeline(obs_data_get_string(req, "canvas"));
		if (!p) {
			obs_data_set_string(res, "error", "Unknown canvas.");
			return;
		}
		OBSDataArrayAutoRelease list = obs_data_array_create();
		for (auto &scene : canvas_scenes(p->canvas())) {
			OBSDataAutoRelease entry = obs_data_create();
			obs_data_set_string(entry, "name", obs_source_get_name(scene));
			obs_data_set_string(entry, "uuid", obs_source_get_uuid(scene));
			obs_data_array_push_back(list, entry);
		}
		obs_data_set_array(res, "scenes", list);
	});
}

void get_warnings(obs_data_t *request, obs_data_t *response, void *)
{
	on_ui(request, response, [](Controller &c, obs_data_t *, obs_data_t *res) {
		OBSDataArrayAutoRelease list = obs_data_array_create();
		for (const auto &[canvas, sources] : c.warnings()) {
			for (const auto &[uuid, w] : sources) {
				if (!w.any())
					continue;
				OBSDataAutoRelease entry = obs_data_create();
				obs_data_set_string(entry, "canvas", canvas.c_str());
				obs_data_set_string(entry, "source_uuid", uuid.c_str());
				OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid.c_str());
				obs_data_set_string(entry, "source_name", source ? obs_source_get_name(source) : "");
				obs_data_set_bool(entry, "video_live", w.video_live);
				obs_data_set_bool(entry, "audio_live", w.audio_live);
				obs_data_set_bool(entry, "shared_track", w.shared_track);
				obs_data_set_int(entry, "tracks", w.tracks);
				obs_data_array_push_back(list, entry);
			}
		}
		obs_data_set_array(res, "warnings", list);
	});
}

void get_interrupted(obs_data_t *request, obs_data_t *response, void *)
{
	on_ui(request, response, [](Controller &c, obs_data_t *, obs_data_t *res) {
		OBSDataArrayAutoRelease list = obs_data_array_create();
		for (const auto &rec : c.interrupted()) {
			OBSDataAutoRelease entry = obs_data_create();
			obs_data_set_string(entry, "id", rec.id.c_str());
			obs_data_set_string(entry, "canvas", rec.canvas.c_str());
			obs_data_set_string(entry, "path", rec.path.c_str());
			obs_data_set_string(entry, "started", rec.started.c_str());
			obs_data_array_push_back(list, entry);
		}
		obs_data_set_array(res, "recordings", list);
	});
}

struct Request {
	const char *name;
	obs_websocket_request_callback_function fn;
};

const Request kRequests[] = {
	{"GetStatus", get_status},
	{"StartRecording", start_recording},
	{"StopRecording", stop_recording},
	{"PauseRecording", pause_recording},
	{"ResumeRecording", resume_recording},
	{"SetEnabled", set_enabled},
	{"SetCanvasSettings", set_canvas_settings},
	{"GetCanvasScenes", get_canvas_scenes},
	{"GetInterruptedRecordings", get_interrupted},
	{"SetRoute", set_route},
	{"GetRoute", get_routes},
	{"SetItemRoute", set_item_route},
	{"GetItemRoute", get_item_route},
	{"GetWarnings", get_warnings},
};

} // namespace

void register_vendor()
{
	if (g_vendor)
		return;
	g_vendor = obs_websocket_register_vendor("selective-recording");
	if (!g_vendor) {
		obs_log(LOG_INFO, "obs-websocket not available; vendor requests disabled");
		return;
	}
	for (const Request &r : kRequests)
		obs_websocket_vendor_register_request(g_vendor, r.name, r.fn, nullptr);
}

void unregister_vendor()
{
	if (!g_vendor)
		return;
	for (const Request &r : kRequests)
		obs_websocket_vendor_unregister_request(g_vendor, r.name);
	g_vendor = nullptr;
}

void emit_state_changed()
{
	if (!g_vendor)
		return;
	Controller *c = Controller::instance();
	if (!c)
		return;

	OBSDataAutoRelease data = obs_data_create();
	OBSDataArrayAutoRelease canvases = obs_data_array_create();
	for (Pipeline *p : c->pipelines()) {
		OBSDataAutoRelease entry = pipeline_status(p);
		obs_data_array_push_back(canvases, entry);
	}
	obs_data_set_array(data, "canvases", canvases);
	obs_websocket_vendor_emit_event(g_vendor, "RecordingStateChanged", data);
}

} // namespace selective_recording::websocket
