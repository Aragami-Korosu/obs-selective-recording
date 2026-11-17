// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include "core/routes.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace selective_recording {

// Routing profiles for the current scene collection. Source UUIDs belong to a scene
// collection, so profiles are saved inside it. Owned and used on the UI thread only.
class RouteStore {
public:
	RouteStore();

	void load(obs_data_t *collection_data);
	obs_data_t *save() const;
	void reset();

	const std::vector<RouteProfile> &profiles() const { return profiles_; }
	const RouteProfile &active() const { return profiles_[active_]; }
	size_t active_index() const { return active_; }
	void set_active(size_t index);

	size_t add_profile(const std::string &name, bool copy_active);
	void rename_profile(size_t index, const std::string &name);
	bool remove_profile(size_t index);
	bool import_profile(obs_data_t *data);
	obs_data_t *export_profile(size_t index) const;

	ResolvedRoute resolve_video(const std::string &canvas, obs_source_t *source, obs_sceneitem_t *item) const;
	ResolvedRoute resolve_audio(const std::string &canvas, obs_source_t *source) const;
	double gain_db(const std::string &canvas, obs_source_t *source) const;

	// An empty canvas sets the rule for every canvas. nullopt clears it.
	void set_video(const std::string &canvas, obs_source_t *source, std::optional<Route> route);
	void set_audio(const std::string &canvas, obs_source_t *source, std::optional<Route> route);
	void set_gain(const std::string &canvas, obs_source_t *source, std::optional<double> gain_db);
	void set_item_video(obs_sceneitem_t *item, std::optional<Route> route);
	bool has_item_video(obs_sceneitem_t *item) const;

	void remove_missing_sources();

	// Increments on every change so pipelines know to re-evaluate.
	uint64_t revision() const { return revision_; }

private:
	SourceRule *rule_for(const std::string &canvas, obs_source_t *source, bool create);
	const SourceRule *find_rule(const std::string &canvas, const std::string &uuid) const;
	void changed() { ++revision_; }

	std::vector<RouteProfile> profiles_;
	size_t active_ = 0;
	uint64_t revision_ = 1;
};

} // namespace selective_recording
