// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <obs.hpp>

#include <functional>
#include <string>
#include <vector>

namespace selective_recording {

inline constexpr const char *kMainCanvasKey = "main";

struct CanvasInfo {
	OBSCanvas canvas;
	std::string key;
	std::string name;
	bool is_main = false;
	uint32_t width = 0;
	uint32_t height = 0;
};

// The main canvas plus every frontend canvas with video, such as Aitum Vertical.
std::vector<CanvasInfo> list_canvases();
bool is_main_canvas(obs_canvas_t *canvas);
std::string canvas_key(obs_canvas_t *canvas);
OBSCanvas find_canvas(const std::string &key);

// Scenes belonging to a canvas, in the order the user sees them where possible.
std::vector<OBSSource> canvas_scenes(obs_canvas_t *canvas);

// Desktop audio, mic and other global audio devices (main canvas channels 1-6).
std::vector<OBSSource> global_audio_sources();

bool has_audio(obs_source_t *source);
bool has_video(obs_source_t *source);
std::string source_uuid(obs_source_t *source);
std::string source_name(obs_source_t *source);

// Sources visible on air under a root, including nested scenes and groups.
std::vector<OBSSource> active_tree(obs_source_t *root);
bool contains_source(const std::vector<OBSSource> &sources, obs_source_t *source);

// Runs work on the Qt UI thread. Plugin state is owned there.
void post_to_ui(std::function<void()> fn);
void run_on_ui_blocking(std::function<void()> fn);
bool on_ui_thread();

} // namespace selective_recording
