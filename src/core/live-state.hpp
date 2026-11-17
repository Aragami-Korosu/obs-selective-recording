// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include "core/obs-utils.hpp"

#include <map>
#include <string>
#include <vector>

namespace selective_recording {

class RouteStore;

// Why a source marked Recording only or Off may still reach a stream.
struct SourceWarning {
	bool video_live = false;
	bool audio_live = false;
	bool shared_track = false;
	std::vector<std::string> scenes;
	uint32_t tracks = 0;
	bool tracks_assumed = false;

	bool any() const { return video_live || audio_live || shared_track; }
};

// Keyed by canvas key, then source UUID.
using WarningMap = std::map<std::string, std::map<std::string, SourceWarning>>;

// Reads OBS state only; nothing here changes it (ADR-0003).
WarningMap evaluate_warnings(const std::vector<CanvasInfo> &canvases, const RouteStore &routes, bool check_video,
			     bool check_audio);

// Audio tracks used by the stream(s) on a canvas, as a mixer bitmask.
uint32_t stream_tracks(obs_canvas_t *canvas, bool *assumed);

} // namespace selective_recording
