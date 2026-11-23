// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <obs.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace selective_recording {

// What a source contributes to the recording, video and audio together (ADR-0010). The stream
// half is the user's intent: choosing Recording only hides the placement in OBS so it leaves the
// stream (ADR-0008), and warnings flag anything still live that shouldn't be (ADR-0003).
enum class Route { Both, StreamOnly, RecordingOnly, Off };

// Set in a scene item's private settings while the plugin is hiding it for Recording only.
inline constexpr const char *kHiddenByPluginKey = "selective_recording_hidden";

const char *route_to_string(Route route);
Route route_from_string(const char *value, Route fallback = Route::Both);
std::optional<Route> optional_route_from_string(const char *value);

inline bool in_recording(Route route)
{
	return route == Route::Both || route == Route::RecordingOnly;
}

// True when the user intends the source to be hidden from the stream.
inline bool hidden_from_stream(Route route)
{
	return route == Route::RecordingOnly || route == Route::Off;
}

Route next_route(Route route);

struct SourceRule {
	std::optional<Route> video;
	// Only read from collections saved by earlier builds; RouteStore folds it into video on load.
	std::optional<Route> audio;
	std::optional<double> gain_db;

	bool empty() const { return !video && !audio && !gain_db; }
};

struct RouteProfile {
	std::string id;
	std::string name;
	Route default_video = Route::Both;

	// Keyed by source UUID.
	std::map<std::string, SourceRule> sources;
	// Keyed by canvas key, then source UUID.
	std::map<std::string, std::map<std::string, SourceRule>> canvas_sources;
	// Keyed by "<scene uuid>/<item id>". The source's audio follows its placements.
	std::map<std::string, Route> item_video;

	obs_data_t *save() const;
	static RouteProfile load(obs_data_t *data);
};

// Where an effective route came from, so the dock can show inherited values differently.
enum class RouteOrigin { Item, Canvas, Source, ProfileDefault };

struct ResolvedRoute {
	Route route = Route::Both;
	RouteOrigin origin = RouteOrigin::ProfileDefault;
};

std::string item_key(const char *scene_uuid, int64_t item_id);

} // namespace selective_recording
