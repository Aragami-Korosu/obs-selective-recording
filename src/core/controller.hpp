// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include "core/live-state.hpp"
#include "core/route-store.hpp"
#include "core/settings.hpp"
#include "recording/recovery.hpp"

#include <obs-frontend-api.h>

#include <QObject>
#include <QTimer>

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace selective_recording {

class Pipeline;

// Owns the plugin's state and every canvas pipeline. Lives on the UI thread.
class Controller : public QObject {
	Q_OBJECT

public:
	static Controller *instance();
	static void create();
	static void destroy();

	Settings &settings() { return settings_; }
	RouteStore &routes() { return routes_; }
	bool shutting_down() const { return shutting_down_; }

	std::vector<Pipeline *> pipelines() const;
	Pipeline *pipeline(const std::string &key) const;

	void start_all();
	void stop_all();

	// Call after changing routes or settings so pipelines, warnings and storage follow.
	void routes_changed();
	void settings_changed();
	void set_enabled(bool enabled);

	const WarningMap &warnings() const { return warnings_; }
	const std::vector<InterruptedRecording> &interrupted() const { return interrupted_; }
	void dismiss_interrupted(const std::string &id);

	void refresh_canvases();
	void pipeline_changed(const std::string &key);

signals:
	void canvasesChanged();
	void pipelineStateChanged(const QString &key);
	void routesChanged();
	void warningsChanged();
	void interruptedChanged();
	void statsUpdated();
	void currentSceneChanged();

private:
	Controller();
	~Controller() override;

	void on_frontend_event(obs_frontend_event event);
	void on_tick();
	void evaluate_warnings_now();
	void shutdown();
	void register_hotkeys(Pipeline *pipeline);
	void unregister_hotkeys(const std::string &key);
	void save_hotkeys();
	void schedule_collection_save();

	static void frontend_event(obs_frontend_event event, void *param);
	static void frontend_save(obs_data_t *data, bool saving, void *param);

	struct Hotkeys {
		obs_hotkey_id toggle = OBS_INVALID_HOTKEY_ID;
		obs_hotkey_id pause = OBS_INVALID_HOTKEY_ID;
	};

	Settings settings_;
	RouteStore routes_;
	std::map<std::string, std::unique_ptr<Pipeline>> pipelines_;
	std::map<std::string, Hotkeys> hotkeys_;
	obs_hotkey_id all_hotkeys_ = OBS_INVALID_HOTKEY_ID;
	WarningMap warnings_;
	std::vector<InterruptedRecording> interrupted_;
	QTimer timer_;
	QTimer save_timer_;
	int ticks_ = 0;
	bool loaded_ = false;
	bool shutting_down_ = false;
	bool frontend_gone_ = false;
};

} // namespace selective_recording
