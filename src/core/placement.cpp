// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "core/placement.hpp"

#include "core/controller.hpp"

#include <obs.hpp>

namespace selective_recording {

namespace {

// Set in a scene item's private settings (saved with the scene collection) while the plugin
// is hiding it for Recording only, so it only ever re-shows placements it hid itself.
constexpr const char *kHiddenByPlugin = "selective_recording_hidden";

bool hidden_by_plugin(obs_sceneitem_t *item)
{
	OBSDataAutoRelease data = obs_sceneitem_get_private_settings(item);
	return obs_data_get_bool(data, kHiddenByPlugin);
}

void set_hidden_by_plugin(obs_sceneitem_t *item, bool hidden)
{
	OBSDataAutoRelease data = obs_sceneitem_get_private_settings(item);
	if (hidden)
		obs_data_set_bool(data, kHiddenByPlugin, true);
	else
		obs_data_erase(data, kHiddenByPlugin);
}

} // namespace

PlacementRoute placement_route(Controller &controller, const std::string &canvas, obs_sceneitem_t *item)
{
	PlacementRoute result;
	if (!item)
		return result;
	result.route = controller.routes().resolve_video(canvas, obs_sceneitem_get_source(item), item).route;
	result.locked = obs_sceneitem_locked(item);
	return result;
}

bool set_placement_route(Controller &controller, const std::string &canvas, obs_sceneitem_t *item, Route route)
{
	if (!item || obs_sceneitem_locked(item))
		return false;

	RouteStore &routes = controller.routes();
	obs_source_t *source = obs_sceneitem_get_source(item);
	const Route before = routes.resolve_video(canvas, source, item).route;

	// Only keep an override where the placement differs from what its source gets anyway.
	const Route inherited = routes.resolve_video(canvas, source, nullptr).route;
	routes.set_item_video(item, route == inherited ? std::nullopt : std::optional<Route>(route));

	if (route == Route::RecordingOnly) {
		if (obs_sceneitem_visible(item)) {
			set_hidden_by_plugin(item, true);
			obs_sceneitem_set_visible(item, false);
		}
	} else if (before == Route::RecordingOnly && hidden_by_plugin(item)) {
		set_hidden_by_plugin(item, false);
		if (route != Route::Off)
			obs_sceneitem_set_visible(item, true);
	}
	controller.routes_changed();
	return true;
}

bool cycle_placement_route(Controller &controller, const std::string &canvas, obs_sceneitem_t *item)
{
	Route next = Route::Both;
	switch (placement_route(controller, canvas, item).route) {
	case Route::Both:
		next = Route::StreamOnly;
		break;
	case Route::StreamOnly:
		next = Route::RecordingOnly;
		break;
	case Route::RecordingOnly:
	case Route::Off:
		next = Route::Both;
		break;
	}
	return set_placement_route(controller, canvas, item, next);
}

} // namespace selective_recording
