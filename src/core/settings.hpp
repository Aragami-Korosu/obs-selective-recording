// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <obs.h>

#include <map>
#include <string>

namespace selective_recording {

enum class RateControl { Quality, Bitrate };

// Recording preferences shared by every canvas.
struct RecordingDefaults {
	std::string container = "mkv";
	std::string audio_encoder = "ffmpeg_aac";
	int audio_bitrate = 192;
	std::string filename_format = "%CCYY-%MM-%DD %hh-%mm-%ss";
	bool fallback_to_software = true;
};

// Recording settings specific to one canvas. Empty or zero values use the canvas or global default.
struct CanvasSettings {
	bool enabled = true;
	uint32_t width = 0;
	uint32_t height = 0;
	uint32_t fps_divisor = 1;
	std::string video_encoder = "auto";
	RateControl rate_control = RateControl::Quality;
	int quality = 20;
	int bitrate = 12000;
	std::string output_directory;
	std::string filename_suffix;
	obs_data_t *hotkeys = nullptr;
};

struct WarningSettings {
	bool live_visibility = true;
	bool audio_tracks = true;
};

// Global plugin settings, stored in the plugin's config directory.
class Settings {
public:
	Settings();
	~Settings();
	Settings(const Settings &) = delete;
	Settings &operator=(const Settings &) = delete;

	void load();
	void save() const;

	bool enabled = true;
	bool offer_recovery = true;
	RecordingDefaults recording;
	WarningSettings warnings;

	CanvasSettings &canvas(const std::string &key);
	const std::map<std::string, CanvasSettings> &canvases() const { return canvases_; }

	static std::string config_path(const char *file);
	static void ensure_config_dir();

private:
	std::map<std::string, CanvasSettings> canvases_;
};

} // namespace selective_recording
