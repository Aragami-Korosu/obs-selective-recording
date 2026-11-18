// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "core/placement.hpp"

#include "core/controller.hpp"

#include <obs.hpp>

#include <vector>

namespace selective_recording {

namespace {

// The mark (saved with the scene collection) means the plugin only ever re-shows placements
// it hid itself.
bool hidden_by_plugin(obs_sceneitem_t *item)
{
	OBSDataAutoRelease data = obs_sceneitem_get_private_settings(item);
	return obs_data_get_bool(data, kHiddenByPluginKey);
}

void set_hidden_by_plugin(obs_sceneitem_t *item, bool hidden)
{
	OBSDataAutoRelease data = obs_sceneitem_get_private_settings(item);
	if (hidden)
		obs_data_set_bool(data, kHiddenByPluginKey, true);
	else
		obs_data_erase(data, kHiddenByPluginKey);
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

bool has_placement_route(obs_sceneitem_t *item)
{
	return item && !obs_sceneitem_is_group(item);
}

namespace {

// Sets one placement without notifying, so a folder can set all its sources in one change.
bool apply_placement_route(Controller &controller, const std::string &canvas, obs_sceneitem_t *item, Route route)
{
	if (!has_placement_route(item) || obs_sceneitem_locked(item))
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
	return true;
}

Route next_in_cycle(Route current)
{
	switch (current) {
	case Route::Both:
		return Route::StreamOnly;
	case Route::StreamOnly:
		return Route::RecordingOnly;
	case Route::RecordingOnly:
	case Route::Off:
		return Route::Both;
	}
	return Route::Both;
}

std::vector<OBSSceneItem> folder_items(obs_sceneitem_t *folder)
{
	std::vector<OBSSceneItem> items;
	obs_scene_t *scene = obs_sceneitem_group_get_scene(folder);
	if (scene) {
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
				static_cast<std::vector<OBSSceneItem> *>(param)->emplace_back(item);
				return true;
			},
			&items);
	}
	return items;
}

} // namespace

bool set_placement_route(Controller &controller, const std::string &canvas, obs_sceneitem_t *item, Route route)
{
	if (!apply_placement_route(controller, canvas, item, route))
		return false;
	controller.routes_changed();
	return true;
}

bool cycle_placement_route(Controller &controller, const std::string &canvas, obs_sceneitem_t *item)
{
	return set_placement_route(controller, canvas, item,
				   next_in_cycle(placement_route(controller, canvas, item).route));
}

FolderRoute folder_route(Controller &controller, const std::string &canvas, obs_sceneitem_t *folder)
{
	FolderRoute result;
	if (!folder || !obs_sceneitem_is_group(folder))
		return result;
	result.locked = obs_sceneitem_locked(folder);
	bool first = true;
	for (auto &item : folder_items(folder)) {
		if (!has_placement_route(item))
			continue;
		const Route route = placement_route(controller, canvas, item).route;
		if (first) {
			result.route = route;
			first = false;
		} else if (route != result.route) {
			result.mixed = true;
		}
	}
	result.empty = first;
	return result;
}

bool set_folder_route(Controller &controller, const std::string &canvas, obs_sceneitem_t *folder, Route route)
{
	if (!folder || !obs_sceneitem_is_group(folder) || obs_sceneitem_locked(folder))
		return false;
	bool changed = false;
	for (auto &item : folder_items(folder))
		changed = apply_placement_route(controller, canvas, item, route) || changed;
	if (changed)
		controller.routes_changed();
	return changed;
}

bool cycle_folder_route(Controller &controller, const std::string &canvas, obs_sceneitem_t *folder)
{
	const FolderRoute current = folder_route(controller, canvas, folder);
	return set_folder_route(controller, canvas, folder, current.mixed ? Route::Both : next_in_cycle(current.route));
}

} // namespace selective_recording
