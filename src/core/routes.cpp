// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "core/routes.hpp"

#include <obs.hpp>

#include <cstring>

namespace selective_recording {

const char *route_to_string(Route route)
{
	switch (route) {
	case Route::Both:
		return "both";
	case Route::StreamOnly:
		return "stream_only";
	case Route::RecordingOnly:
		return "recording_only";
	case Route::Off:
		return "off";
	}
	return "both";
}

std::optional<Route> optional_route_from_string(const char *value)
{
	if (!value || !*value)
		return std::nullopt;
	if (strcmp(value, "both") == 0)
		return Route::Both;
	if (strcmp(value, "stream_only") == 0)
		return Route::StreamOnly;
	if (strcmp(value, "recording_only") == 0)
		return Route::RecordingOnly;
	if (strcmp(value, "off") == 0)
		return Route::Off;
	return std::nullopt;
}

Route route_from_string(const char *value, Route fallback)
{
	return optional_route_from_string(value).value_or(fallback);
}

Route next_route(Route route)
{
	switch (route) {
	case Route::Both:
		return Route::StreamOnly;
	case Route::StreamOnly:
		return Route::RecordingOnly;
	case Route::RecordingOnly:
		return Route::Off;
	case Route::Off:
		return Route::Both;
	}
	return Route::Both;
}

std::string item_key(const char *scene_uuid, int64_t item_id)
{
	return std::string(scene_uuid ? scene_uuid : "") + "/" + std::to_string(item_id);
}

namespace {

obs_data_t *save_rule(const SourceRule &rule)
{
	obs_data_t *data = obs_data_create();
	if (rule.video)
		obs_data_set_string(data, "video", route_to_string(*rule.video));
	if (rule.audio)
		obs_data_set_string(data, "audio", route_to_string(*rule.audio));
	if (rule.gain_db)
		obs_data_set_double(data, "gain_db", *rule.gain_db);
	return data;
}

SourceRule load_rule(obs_data_t *data)
{
	SourceRule rule;
	rule.video = optional_route_from_string(obs_data_get_string(data, "video"));
	rule.audio = optional_route_from_string(obs_data_get_string(data, "audio"));
	if (obs_data_has_user_value(data, "gain_db"))
		rule.gain_db = obs_data_get_double(data, "gain_db");
	return rule;
}

obs_data_t *save_rules(const std::map<std::string, SourceRule> &rules)
{
	obs_data_t *data = obs_data_create();
	for (const auto &[uuid, rule] : rules) {
		if (rule.empty())
			continue;
		OBSDataAutoRelease entry = save_rule(rule);
		obs_data_set_obj(data, uuid.c_str(), entry);
	}
	return data;
}

std::map<std::string, SourceRule> load_rules(obs_data_t *data)
{
	std::map<std::string, SourceRule> rules;
	for (obs_data_item_t *item = obs_data_first(data); item; obs_data_item_next(&item)) {
		OBSDataAutoRelease entry = obs_data_item_get_obj(item);
		if (!entry)
			continue;
		SourceRule rule = load_rule(entry);
		if (!rule.empty())
			rules[obs_data_item_get_name(item)] = rule;
	}
	return rules;
}

} // namespace

obs_data_t *RouteProfile::save() const
{
	obs_data_t *data = obs_data_create();
	obs_data_set_string(data, "id", id.c_str());
	obs_data_set_string(data, "name", name.c_str());
	obs_data_set_string(data, "default_video_route", route_to_string(default_video));

	OBSDataAutoRelease source_data = save_rules(sources);
	obs_data_set_obj(data, "source_routes", source_data);

	OBSDataAutoRelease canvas_data = obs_data_create();
	for (const auto &[canvas, rules] : canvas_sources) {
		OBSDataAutoRelease rules_data = save_rules(rules);
		OBSDataAutoRelease entry = obs_data_create();
		obs_data_set_obj(entry, "source_routes", rules_data);
		obs_data_set_obj(canvas_data, canvas.c_str(), entry);
	}
	obs_data_set_obj(data, "canvases", canvas_data);

	OBSDataAutoRelease item_data = obs_data_create();
	for (const auto &[key, route] : item_video) {
		OBSDataAutoRelease entry = obs_data_create();
		obs_data_set_string(entry, "video", route_to_string(route));
		obs_data_set_obj(item_data, key.c_str(), entry);
	}
	obs_data_set_obj(data, "item_overrides", item_data);
	return data;
}

RouteProfile RouteProfile::load(obs_data_t *data)
{
	RouteProfile profile;
	profile.id = obs_data_get_string(data, "id");
	profile.name = obs_data_get_string(data, "name");
	profile.default_video = route_from_string(obs_data_get_string(data, "default_video_route"));

	OBSDataAutoRelease source_data = obs_data_get_obj(data, "source_routes");
	profile.sources = load_rules(source_data);

	OBSDataAutoRelease canvas_data = obs_data_get_obj(data, "canvases");
	for (obs_data_item_t *item = obs_data_first(canvas_data); item; obs_data_item_next(&item)) {
		OBSDataAutoRelease entry = obs_data_item_get_obj(item);
		OBSDataAutoRelease rules_data = obs_data_get_obj(entry, "source_routes");
		auto rules = load_rules(rules_data);
		if (!rules.empty())
			profile.canvas_sources[obs_data_item_get_name(item)] = std::move(rules);
	}

	OBSDataAutoRelease item_data = obs_data_get_obj(data, "item_overrides");
	for (obs_data_item_t *item = obs_data_first(item_data); item; obs_data_item_next(&item)) {
		OBSDataAutoRelease entry = obs_data_item_get_obj(item);
		auto route = optional_route_from_string(obs_data_get_string(entry, "video"));
		if (route)
			profile.item_video[obs_data_item_get_name(item)] = *route;
	}
	return profile;
}

} // namespace selective_recording
