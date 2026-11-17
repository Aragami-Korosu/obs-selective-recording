// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "core/controller.hpp"

#include "core/obs-utils.hpp"
#include "core/pipeline.hpp"

#include <obs-module.h>
#include <plugin-support.h>

#include <algorithm>

namespace selective_recording {

namespace {

constexpr const char *kSaveKey = "selective-recording";
constexpr int kTickMs = 250;
constexpr int kWarningTicks = 4;
Controller *g_instance = nullptr;

struct HotkeyTarget {
	std::string key;
	bool pause;
};

void hotkey_pressed(void *data, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	auto *target = static_cast<HotkeyTarget *>(data);
	const std::string key = target->key;
	const bool pause = target->pause;

	post_to_ui([key, pause]() {
		Controller *controller = Controller::instance();
		Pipeline *pipeline = controller ? controller->pipeline(key) : nullptr;
		if (!pipeline)
			return;
		if (pause)
			pipeline->set_paused(pipeline->state() != PipelineState::Paused);
		else if (pipeline->recording())
			pipeline->stop();
		else
			pipeline->start();
	});
}

void all_hotkey_pressed(void *, obs_hotkey_id, obs_hotkey_t *, bool pressed)
{
	if (!pressed)
		return;
	post_to_ui([]() {
		Controller *controller = Controller::instance();
		if (!controller)
			return;
		bool any = false;
		for (Pipeline *p : controller->pipelines())
			any = any || p->recording();
		if (any)
			controller->stop_all();
		else
			controller->start_all();
	});
}

// Hotkey callbacks need a stable pointer; these live until the plugin unloads.
std::map<std::string, std::unique_ptr<HotkeyTarget>> g_hotkey_targets;

HotkeyTarget *hotkey_target(const std::string &key, bool pause)
{
	const std::string id = key + (pause ? "#pause" : "#toggle");
	auto &slot = g_hotkey_targets[id];
	if (!slot)
		slot = std::make_unique<HotkeyTarget>(HotkeyTarget{key, pause});
	return slot.get();
}

} // namespace

Controller *Controller::instance()
{
	return g_instance;
}

void Controller::create()
{
	if (!g_instance)
		g_instance = new Controller();
}

void Controller::destroy()
{
	delete g_instance;
	g_instance = nullptr;
	g_hotkey_targets.clear();
}

Controller::Controller()
{
	settings_.load();

	obs_frontend_add_event_callback(frontend_event, this);
	obs_frontend_add_save_callback(frontend_save, this);

	all_hotkeys_ = obs_hotkey_register_frontend("selective_recording.toggle_all",
						    obs_module_text("Hotkey.ToggleAll"), all_hotkey_pressed, nullptr);
	OBSDataAutoRelease all =
		obs_data_create_from_json_file_safe(Settings::config_path("hotkeys.json").c_str(), "bak");
	if (all) {
		OBSDataArrayAutoRelease array = obs_data_get_array(all, "toggle_all");
		obs_hotkey_load(all_hotkeys_, array);
	}

	connect(&timer_, &QTimer::timeout, this, &Controller::on_tick);
	timer_.start(kTickMs);

	save_timer_.setSingleShot(true);
	save_timer_.setInterval(3000);
	connect(&save_timer_, &QTimer::timeout, this, []() { obs_frontend_save(); });
}

Controller::~Controller()
{
	shutdown();
	obs_frontend_remove_save_callback(frontend_save, this);
	obs_frontend_remove_event_callback(frontend_event, this);
}

void Controller::frontend_event(obs_frontend_event event, void *param)
{
	static_cast<Controller *>(param)->on_frontend_event(event);
}

void Controller::frontend_save(obs_data_t *data, bool saving, void *param)
{
	auto *self = static_cast<Controller *>(param);
	if (saving) {
		OBSDataAutoRelease routes = self->routes_.save();
		obs_data_set_obj(data, kSaveKey, routes);
		return;
	}

	OBSDataAutoRelease routes = obs_data_get_obj(data, kSaveKey);
	if (routes)
		self->routes_.load(routes);
	else
		self->routes_.reset();
	emit self->routesChanged();
}

void Controller::on_frontend_event(obs_frontend_event event)
{
	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		loaded_ = true;
		refresh_canvases();
		routes_.remove_missing_sources();
		if (settings_.offer_recovery) {
			interrupted_ = recovery::find_interrupted();
			if (!interrupted_.empty())
				emit interruptedChanged();
		}
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGING:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		// Recordings keep running through a collection switch; the mirror shows black until it returns.
		for (auto &[key, p] : pipelines_)
			p->suspend();
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		refresh_canvases();
		for (auto &[key, p] : pipelines_)
			p->resume();
		routes_.remove_missing_sources();
		emit routesChanged();
		break;
	case OBS_FRONTEND_EVENT_CANVAS_ADDED:
	case OBS_FRONTEND_EVENT_CANVAS_REMOVED:
		refresh_canvases();
		break;
	case OBS_FRONTEND_EVENT_EXIT:
		shutdown();
		break;
	default:
		break;
	}
}

std::vector<Pipeline *> Controller::pipelines() const
{
	std::vector<Pipeline *> result;
	// Main canvas first, then the rest in name order.
	auto main = pipelines_.find(kMainCanvasKey);
	if (main != pipelines_.end())
		result.push_back(main->second.get());
	for (auto &[key, p] : pipelines_) {
		if (key != kMainCanvasKey)
			result.push_back(p.get());
	}
	return result;
}

Pipeline *Controller::pipeline(const std::string &key) const
{
	auto it = pipelines_.find(key);
	return it == pipelines_.end() ? nullptr : it->second.get();
}

void Controller::refresh_canvases()
{
	if (!loaded_ || shutting_down_)
		return;

	const std::vector<CanvasInfo> canvases = list_canvases();
	bool changed = false;

	for (auto it = pipelines_.begin(); it != pipelines_.end();) {
		Pipeline *p = it->second.get();
		const bool still_there = std::any_of(canvases.begin(), canvases.end(), [&](const CanvasInfo &info) {
			return info.key == p->key() && info.canvas.Get() == p->canvas();
		});
		if (still_there && !obs_canvas_removed(p->canvas())) {
			++it;
			continue;
		}

		// The canvas is gone: finish this recording cleanly and keep the file (NFR-009).
		obs_log(LOG_INFO, "canvas '%s' removed", p->key().c_str());
		p->stop_and_wait(5000);
		unregister_hotkeys(p->key());
		it = pipelines_.erase(it);
		changed = true;
	}

	for (const CanvasInfo &info : canvases) {
		if (pipelines_.count(info.key))
			continue;
		obs_log(LOG_INFO, "canvas '%s' (%ux%u) available", info.key.c_str(), info.width, info.height);
		auto pipeline = std::make_unique<Pipeline>(*this, info.canvas, info.key,
							   info.is_main ? obs_module_text("Canvas.Main") : info.name);
		register_hotkeys(pipeline.get());
		pipelines_[info.key] = std::move(pipeline);
		changed = true;
	}

	if (changed) {
		emit canvasesChanged();
		evaluate_warnings_now();
	}
}

void Controller::start_all()
{
	for (Pipeline *p : pipelines()) {
		if (settings_.canvas(p->key()).enabled)
			p->start();
	}
}

void Controller::stop_all()
{
	for (Pipeline *p : pipelines())
		p->stop();
}

void Controller::routes_changed()
{
	for (auto &[key, p] : pipelines_)
		p->routes_changed();
	evaluate_warnings_now();
	emit routesChanged();
	schedule_collection_save();
}

void Controller::settings_changed()
{
	save_hotkeys();
	settings_.save();
	for (auto &[key, p] : pipelines_)
		p->routes_changed();
	evaluate_warnings_now();
	emit routesChanged();
}

void Controller::set_enabled(bool enabled)
{
	if (settings_.enabled == enabled)
		return;
	settings_.enabled = enabled;
	settings_changed();
}

void Controller::dismiss_interrupted(const std::string &id)
{
	recovery::dismiss(id);
	interrupted_.erase(std::remove_if(interrupted_.begin(), interrupted_.end(),
					  [&](const InterruptedRecording &r) { return r.id == id; }),
			   interrupted_.end());
	emit interruptedChanged();
}

void Controller::pipeline_changed(const std::string &key)
{
	emit pipelineStateChanged(QString::fromStdString(key));
}

void Controller::schedule_collection_save()
{
	// Persist routes soon after a change so a crash doesn't lose them.
	save_timer_.start();
}

void Controller::on_tick()
{
	if (shutting_down_)
		return;

	bool any_active = false;
	for (auto &[key, p] : pipelines_) {
		try {
			p->update();
		} catch (const std::exception &e) {
			obs_log(LOG_ERROR, "[%s] update failed: %s", key.c_str(), e.what());
		}
		any_active = any_active || p->busy();
	}

	if (any_active)
		emit statsUpdated();

	if (++ticks_ % kWarningTicks == 0)
		evaluate_warnings_now();
}

void Controller::evaluate_warnings_now()
{
	if (!loaded_ || shutting_down_)
		return;

	const bool enabled = settings_.enabled;
	warnings_ = evaluate_warnings(list_canvases(), routes_, enabled && settings_.warnings.live_visibility,
				      enabled && settings_.warnings.audio_tracks);
	emit warningsChanged();
}

void Controller::register_hotkeys(Pipeline *pipeline)
{
	const std::string &key = pipeline->key();
	const std::string name = pipeline->name();
	Hotkeys hk;

	const std::string toggle_id = "selective_recording.toggle." + key;
	const std::string toggle_desc = std::string(obs_module_text("Hotkey.Toggle")) + " (" + name + ")";
	hk.toggle = obs_hotkey_register_frontend(toggle_id.c_str(), toggle_desc.c_str(), hotkey_pressed,
						 hotkey_target(key, false));

	const std::string pause_id = "selective_recording.pause." + key;
	const std::string pause_desc = std::string(obs_module_text("Hotkey.Pause")) + " (" + name + ")";
	hk.pause = obs_hotkey_register_frontend(pause_id.c_str(), pause_desc.c_str(), hotkey_pressed,
						hotkey_target(key, true));

	obs_data_t *saved = settings_.canvas(key).hotkeys;
	if (saved) {
		OBSDataArrayAutoRelease toggle = obs_data_get_array(saved, "toggle");
		OBSDataArrayAutoRelease pause = obs_data_get_array(saved, "pause");
		obs_hotkey_load(hk.toggle, toggle);
		obs_hotkey_load(hk.pause, pause);
	}
	hotkeys_[key] = hk;
}

void Controller::unregister_hotkeys(const std::string &key)
{
	save_hotkeys();
	auto it = hotkeys_.find(key);
	if (it == hotkeys_.end())
		return;
	obs_hotkey_unregister(it->second.toggle);
	obs_hotkey_unregister(it->second.pause);
	hotkeys_.erase(it);
}

void Controller::save_hotkeys()
{
	for (auto &[key, hk] : hotkeys_) {
		OBSDataArrayAutoRelease toggle = obs_hotkey_save(hk.toggle);
		OBSDataArrayAutoRelease pause = obs_hotkey_save(hk.pause);
		CanvasSettings &cs = settings_.canvas(key);
		if (!cs.hotkeys)
			cs.hotkeys = obs_data_create();
		obs_data_set_array(cs.hotkeys, "toggle", toggle);
		obs_data_set_array(cs.hotkeys, "pause", pause);
	}

	if (all_hotkeys_ != OBS_INVALID_HOTKEY_ID) {
		OBSDataArrayAutoRelease array = obs_hotkey_save(all_hotkeys_);
		OBSDataAutoRelease data = obs_data_create();
		obs_data_set_array(data, "toggle_all", array);
		obs_data_save_json_safe(data, Settings::config_path("hotkeys.json").c_str(), "tmp", "bak");
	}
}

void Controller::shutdown()
{
	if (shutting_down_)
		return;
	shutting_down_ = true;
	timer_.stop();
	save_timer_.stop();

	save_hotkeys();
	settings_.save();

	// Finish every recording before OBS tears down, so files are finalised.
	for (auto &[key, p] : pipelines_)
		p->stop_and_wait(8000);
	pipelines_.clear();

	for (auto &[key, hk] : hotkeys_) {
		obs_hotkey_unregister(hk.toggle);
		obs_hotkey_unregister(hk.pause);
	}
	hotkeys_.clear();
	if (all_hotkeys_ != OBS_INVALID_HOTKEY_ID) {
		obs_hotkey_unregister(all_hotkeys_);
		all_hotkeys_ = OBS_INVALID_HOTKEY_ID;
	}
	emit canvasesChanged();
}

} // namespace selective_recording
