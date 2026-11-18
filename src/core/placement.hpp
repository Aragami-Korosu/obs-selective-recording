// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include "core/routes.hpp"

#include <string>

namespace selective_recording {

class Controller;

// Placement routes work like Streamlabs Desktop's selective recording: one setting for the
// video of one placement (a source in one scene), and the eye icon is never needed. They're
// what the Sources-list buttons, the dock's simple view and SetItemRoute change. Audio is set
// per source.
struct PlacementRoute {
	Route route = Route::Both;
	bool locked = false;
};

PlacementRoute placement_route(Controller &controller, const std::string &canvas, obs_sceneitem_t *item);

// Sets one placement's route. As in Streamlabs, Recording only takes the placement off the
// stream: the plugin hides it in OBS, and shows it again when it leaves Recording only (only
// if the plugin was the one that hid it). Locked placements are left alone; returns false.
bool set_placement_route(Controller &controller, const std::string &canvas, obs_sceneitem_t *item, Route route);

// Both → Stream only → Recording only → Both, as in Streamlabs.
bool cycle_placement_route(Controller &controller, const std::string &canvas, obs_sceneitem_t *item);

} // namespace selective_recording
