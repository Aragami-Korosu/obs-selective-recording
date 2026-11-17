// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "core/route-store.hpp"

#include <obs-module.h>
#include <obs.hpp>
#include <util/platform.h>

namespace selective_recording {

namespace {

std::string new_profile_id()
{
	char *uuid = os_generate_uuid();
	std::string id = uuid ? uuid : "profile";
	bfree(uuid);
	return id;
}

RouteProfile default_profile()
{
	RouteProfile profile;
	profile.id = "default";
	profile.name = obs_module_text("Profile.Default");
	return profile;
}

std::string scene_uuid_of(obs_sceneitem_t *item)
{
	obs_scene_t *scene = obs_sceneitem_get_scene(item);
	obs_source_t *scene_source = obs_scene_get_source(scene);
	const char *uuid = scene_source ? obs_source_get_uuid(scene_source) : nullptr;
	return uuid ? uuid : "";
}

} // namespace

RouteStore::RouteStore()
{
	reset();
}

void RouteStore::reset()
{
	profiles_.clear();
	profiles_.push_back(default_profile());
	active_ = 0;
	changed();
}

void RouteStore::load(obs_data_t *collection_data)
{
	profiles_.clear();
	active_ = 0;

	OBSDataArrayAutoRelease array = obs_data_get_array(collection_data, "profiles");
	const size_t count = obs_data_array_count(array);
	for (size_t i = 0; i < count; i++) {
		OBSDataAutoRelease entry = obs_data_array_item(array, i);
		RouteProfile profile = RouteProfile::load(entry);
		if (profile.id.empty())
			profile.id = new_profile_id();
		if (profile.name.empty())
			profile.name = profile.id;
		profiles_.push_back(std::move(profile));
	}

	if (profiles_.empty())
		profiles_.push_back(default_profile());

	const char *active_id = obs_data_get_string(collection_data, "active_profile");
	for (size_t i = 0; i < profiles_.size(); i++) {
		if (profiles_[i].id == active_id)
			active_ = i;
	}
	changed();
}

obs_data_t *RouteStore::save() const
{
	obs_data_t *data = obs_data_create();
	obs_data_set_int(data, "schema_version", 1);
	obs_data_set_string(data, "active_profile", active().id.c_str());

	OBSDataArrayAutoRelease array = obs_data_array_create();
	for (const auto &profile : profiles_) {
		OBSDataAutoRelease entry = profile.save();
		obs_data_array_push_back(array, entry);
	}
	obs_data_set_array(data, "profiles", array);
	return data;
}

void RouteStore::set_active(size_t index)
{
	if (index < profiles_.size() && index != active_) {
		active_ = index;
		changed();
	}
}

size_t RouteStore::add_profile(const std::string &name, bool copy_active)
{
	RouteProfile profile = copy_active ? active() : RouteProfile{};
	profile.id = new_profile_id();
	profile.name = name;
	profiles_.push_back(std::move(profile));
	active_ = profiles_.size() - 1;
	changed();
	return active_;
}

void RouteStore::rename_profile(size_t index, const std::string &name)
{
	if (index < profiles_.size() && !name.empty()) {
		profiles_[index].name = name;
		changed();
	}
}

bool RouteStore::remove_profile(size_t index)
{
	if (profiles_.size() <= 1 || index >= profiles_.size())
		return false;

	profiles_.erase(profiles_.begin() + (ptrdiff_t)index);
	if (active_ >= profiles_.size() || active_ == index)
		active_ = 0;
	else if (active_ > index)
		active_--;
	changed();
	return true;
}

bool RouteStore::import_profile(obs_data_t *data)
{
	if (!data)
		return false;

	RouteProfile profile = RouteProfile::load(data);
	profile.id = new_profile_id();
	if (profile.name.empty())
		profile.name = obs_module_text("Profile.Imported");
	profiles_.push_back(std::move(profile));
	active_ = profiles_.size() - 1;
	changed();
	return true;
}

obs_data_t *RouteStore::export_profile(size_t index) const
{
	return index < profiles_.size() ? profiles_[index].save() : nullptr;
}

const SourceRule *RouteStore::find_rule(const std::string &canvas, const std::string &uuid) const
{
	const RouteProfile &profile = active();
	if (canvas.empty()) {
		auto it = profile.sources.find(uuid);
		return it == profile.sources.end() ? nullptr : &it->second;
	}

	auto canvas_it = profile.canvas_sources.find(canvas);
	if (canvas_it == profile.canvas_sources.end())
		return nullptr;
	auto it = canvas_it->second.find(uuid);
	return it == canvas_it->second.end() ? nullptr : &it->second;
}

SourceRule *RouteStore::rule_for(const std::string &canvas, obs_source_t *source, bool create)
{
	const char *uuid = source ? obs_source_get_uuid(source) : nullptr;
	if (!uuid)
		return nullptr;

	RouteProfile &profile = profiles_[active_];
	auto &rules = canvas.empty() ? profile.sources : profile.canvas_sources[canvas];
	auto it = rules.find(uuid);
	if (it != rules.end())
		return &it->second;
	if (!create)
		return nullptr;
	return &rules[uuid];
}

ResolvedRoute RouteStore::resolve_video(const std::string &canvas, obs_source_t *source, obs_sceneitem_t *item) const
{
	const RouteProfile &profile = active();

	if (item) {
		auto it = profile.item_video.find(item_key(scene_uuid_of(item).c_str(), obs_sceneitem_get_id(item)));
		if (it != profile.item_video.end())
			return {it->second, RouteOrigin::Item};
	}

	const char *uuid = source ? obs_source_get_uuid(source) : nullptr;
	if (uuid) {
		const SourceRule *rule = find_rule(canvas, uuid);
		if (rule && rule->video)
			return {*rule->video, RouteOrigin::Canvas};
		rule = find_rule("", uuid);
		if (rule && rule->video)
			return {*rule->video, RouteOrigin::Source};
	}
	return {profile.default_video, RouteOrigin::ProfileDefault};
}

ResolvedRoute RouteStore::resolve_audio(const std::string &canvas, obs_source_t *source) const
{
	const RouteProfile &profile = active();
	const char *uuid = source ? obs_source_get_uuid(source) : nullptr;
	if (uuid) {
		const SourceRule *rule = find_rule(canvas, uuid);
		if (rule && rule->audio)
			return {*rule->audio, RouteOrigin::Canvas};
		rule = find_rule("", uuid);
		if (rule && rule->audio)
			return {*rule->audio, RouteOrigin::Source};
	}
	return {profile.default_audio, RouteOrigin::ProfileDefault};
}

double RouteStore::gain_db(const std::string &canvas, obs_source_t *source) const
{
	const char *uuid = source ? obs_source_get_uuid(source) : nullptr;
	if (!uuid)
		return 0.0;
	const SourceRule *rule = find_rule(canvas, uuid);
	if (rule && rule->gain_db)
		return *rule->gain_db;
	rule = find_rule("", uuid);
	return rule && rule->gain_db ? *rule->gain_db : 0.0;
}

void RouteStore::set_video(const std::string &canvas, obs_source_t *source, std::optional<Route> route)
{
	SourceRule *rule = rule_for(canvas, source, route.has_value());
	if (!rule)
		return;
	rule->video = route;
	changed();
}

void RouteStore::set_audio(const std::string &canvas, obs_source_t *source, std::optional<Route> route)
{
	SourceRule *rule = rule_for(canvas, source, route.has_value());
	if (!rule)
		return;
	rule->audio = route;
	changed();
}

void RouteStore::set_gain(const std::string &canvas, obs_source_t *source, std::optional<double> gain_db)
{
	SourceRule *rule = rule_for(canvas, source, gain_db.has_value());
	if (!rule)
		return;
	rule->gain_db = gain_db;
	changed();
}

void RouteStore::set_item_video(obs_sceneitem_t *item, std::optional<Route> route)
{
	if (!item)
		return;
	const std::string key = item_key(scene_uuid_of(item).c_str(), obs_sceneitem_get_id(item));
	auto &items = profiles_[active_].item_video;
	if (route)
		items[key] = *route;
	else
		items.erase(key);
	changed();
}

bool RouteStore::has_item_video(obs_sceneitem_t *item) const
{
	if (!item)
		return false;
	const auto &items = active().item_video;
	return items.count(item_key(scene_uuid_of(item).c_str(), obs_sceneitem_get_id(item))) > 0;
}

void RouteStore::remove_missing_sources()
{
	auto exists = [](const std::string &uuid) {
		OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid.c_str());
		return source != nullptr;
	};

	bool removed = false;
	auto prune = [&](std::map<std::string, SourceRule> &rules) {
		for (auto it = rules.begin(); it != rules.end();) {
			if (it->second.empty() || !exists(it->first)) {
				it = rules.erase(it);
				removed = true;
			} else {
				++it;
			}
		}
	};

	for (auto &profile : profiles_) {
		prune(profile.sources);
		for (auto &[canvas, rules] : profile.canvas_sources)
			prune(rules);
		for (auto it = profile.item_video.begin(); it != profile.item_video.end();) {
			const std::string scene_uuid = it->first.substr(0, it->first.find('/'));
			if (!exists(scene_uuid)) {
				it = profile.item_video.erase(it);
				removed = true;
			} else {
				++it;
			}
		}
	}

	if (removed)
		changed();
}

} // namespace selective_recording
