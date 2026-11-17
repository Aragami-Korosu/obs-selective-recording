// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <obs.hpp>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace selective_recording {

// A private copy of one canvas's scene tree that shares the original sources.
//
// The mirror lives in a private canvas with the original canvas's base resolution, so
// nested scenes and relative transforms behave exactly like the originals. Only objects
// created here are ever modified; the original scenes are read-only (ADR-0003).
class SceneMirror {
public:
	// Decides whether a mirrored copy of an original scene item is visible. Called on the UI thread.
	using VisibilityFn = std::function<bool(obs_sceneitem_t *original)>;

	SceneMirror(obs_canvas_t *source_canvas, uint32_t output_width, uint32_t output_height,
		    VisibilityFn visibility);
	~SceneMirror();
	SceneMirror(const SceneMirror &) = delete;
	SceneMirror &operator=(const SceneMirror &) = delete;

	bool valid() const { return canvas_ != nullptr; }
	obs_canvas_t *canvas() const { return canvas_; }
	video_t *video() const;

	// UI thread only.
	void refresh_visibility();
	void reconcile();
	void suspend();
	void resume();
	void mark_all_dirty();
	std::vector<OBSSource> active_sources() const;

private:
	struct Mirrored {
		OBSSource original;
		OBSScene mirror;
		std::vector<OBSSignal> scene_signals;
		std::vector<OBSSignal> group_signals;
		std::vector<OBSSource> groups;
		bool dirty = true;
	};

	struct Link {
		obs_source_t *owner = nullptr;
		OBSSceneItem original;
		OBSSceneItem mirror;
	};

	struct PlanNode {
		OBSSceneItem original;
		OBSSource add_source;
		bool is_group = false;
		std::vector<PlanNode> children;
	};

	Mirrored *mirror_for(obs_source_t *original_scene, bool create);
	void build(Mirrored &m);
	void plan_items(obs_scene_t *scene, Mirrored &m, std::vector<PlanNode> &out);
	void apply_plan(obs_scene_t *dest, const std::vector<PlanNode> &plan, obs_source_t *owner,
			std::vector<Link> &links);
	void destroy(Mirrored &m);
	void rebuild_dirty();
	void sync_scene_list();
	void ensure_transition();
	void connect_scene_signals(obs_source_t *scene_source, std::vector<OBSSignal> &out);
	void mark_dirty_containing(obs_source_t *scene_or_group);
	void update_item_visibility(obs_sceneitem_t *original);

	static void copy_item_properties(obs_sceneitem_t *from, obs_sceneitem_t *to);
	static void on_structure_changed(void *param, calldata_t *cd);
	static void on_item_transform(void *param, calldata_t *cd);
	static void on_item_visible(void *param, calldata_t *cd);
	static void on_canvas_source_changed(void *param, calldata_t *cd);
	static void on_source_removed(void *param, calldata_t *cd);
	static void tick(void *param, float seconds);
	void tick_transition();

	// Held strongly so its signal handler outlives our connections. The pipeline drops the
	// mirror as soon as the canvas is removed.
	OBSCanvas source_canvas_;
	OBSCanvasAutoRelease canvas_;
	VisibilityFn visibility_;
	uint32_t base_width_ = 0;
	uint32_t base_height_ = 0;
	bool suspended_ = false;

	std::map<obs_source_t *, std::unique_ptr<Mirrored>> mirrors_;

	// Read by signal handlers and the graphics thread.
	mutable std::mutex map_mutex_;
	std::unordered_map<obs_source_t *, OBSSource> scene_map_;
	std::unordered_map<obs_sceneitem_t *, Link> links_;
	OBSSource transition_;
	obs_source_t *transition_original_ = nullptr;
	std::string transition_id_;
	std::string transition_settings_;

	// Graphics thread state for following the original transition.
	obs_source_t *shown_a_ = nullptr;
	obs_source_t *shown_b_ = nullptr;

	std::vector<OBSSignal> canvas_signals_;
	OBSSignal source_remove_signal_;
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace selective_recording
