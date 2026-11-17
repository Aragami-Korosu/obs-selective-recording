// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include "core/routes.hpp"

#include <QIcon>
#include <QString>

#include <string>

namespace selective_recording {

class Controller;

// One route covering a source's video and audio together, as in Streamlabs.
struct CombinedRoute {
	Route route = Route::Both;
	bool mixed = false; // video and audio differ
	bool has_video = false;
	bool has_audio = false;
};

CombinedRoute combined_route(Controller &controller, const std::string &canvas, obs_source_t *source,
			     obs_sceneitem_t *item);

// Sets video and audio for the source on every canvas, and clears this canvas's and this
// placement's overrides so the result is exactly what the button shows.
void set_combined_route(Controller &controller, const std::string &canvas, obs_source_t *source, obs_sceneitem_t *item,
			Route route);

// Both → Stream only → Recording only → Both. Off is available from the menu.
void cycle_combined_route(Controller &controller, const std::string &canvas, obs_source_t *source,
			  obs_sceneitem_t *item);

QIcon route_icon(Route route, bool mixed);
QString combined_label(const CombinedRoute &route);
QString combined_tooltip(const CombinedRoute &route);

} // namespace selective_recording
