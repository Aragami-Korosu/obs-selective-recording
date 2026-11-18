// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "video/scene-mirror.hpp"

#include "core/obs-utils.hpp"

#include <plugin-support.h>

#include <algorithm>

namespace selective_recording {

namespace {

constexpr const char *kCutTransition = "cut_transition";

uint32_t even(uint32_t value)
{
	return std::max<uint32_t>(2, value & ~1u);
}

std::vector<OBSSceneItem> scene_items(obs_scene_t *scene)
{
	std::vector<OBSSceneItem> items;
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			static_cast<std::vector<OBSSceneItem> *>(param)->emplace_back(item);
			return true;
		},
		&items);
	return items;
}

std::string settings_json(obs_source_t *source)
{
	OBSDataAutoRelease settings = obs_source_get_settings(source);
	const char *json = settings ? obs_data_get_json(settings) : nullptr;
	return json ? json : "";
}

} // namespace

SceneMirror::SceneMirror(obs_canvas_t *source_canvas, uint32_t output_width, uint32_t output_height,
			 VisibilityFn visibility)
	: visibility_(std::move(visibility))
{
	obs_video_info ovi = {};
	if (!obs_get_video_info(&ovi))
		return;

	obs_video_info canvas_ovi = {};
	if (obs_canvas_get_video_info(source_canvas, &canvas_ovi)) {
		ovi.base_width = canvas_ovi.base_width;
		ovi.base_height = canvas_ovi.base_height;
	}

	base_width_ = ovi.base_width;
	base_height_ = ovi.base_height;
	ovi.output_width = even(output_width ? output_width : ovi.base_width);
	ovi.output_height = even(output_height ? output_height : ovi.base_height);

	source_canvas_ = source_canvas;

	// ACTIVATE makes recording-only sources render even when hidden in the live scene.
	// MIX_AUDIO is deliberately absent so nothing here is mixed into OBS's audio tracks.
	canvas_ = obs_canvas_create_private("selective-recording", &ovi, ACTIVATE | EPHEMERAL);
	if (!canvas_) {
		obs_log(LOG_ERROR, "could not create the recording canvas");
		return;
	}

	signal_handler_t *sh = obs_canvas_get_signal_handler(source_canvas);
	canvas_signals_.emplace_back(sh, "source_add", on_canvas_source_changed, this);
	canvas_signals_.emplace_back(sh, "source_remove", on_canvas_source_changed, this);
	source_remove_signal_.Connect(obs_get_signal_handler(), "source_remove", on_source_removed, this);

	sync_scene_list();
	rebuild_dirty();
	sync_on_air_scenes();
	ensure_transition();

	obs_add_tick_callback(tick, this);
}

SceneMirror::~SceneMirror()
{
	*alive_ = false;

	if (!canvas_)
		return;

	obs_remove_tick_callback(tick, this);
	obs_canvas_set_channel(canvas_, 0, nullptr);

	canvas_signals_.clear();
	source_remove_signal_.Disconnect();

	// Scenes made in a canvas keep a pointer to the canvas's lock even after they're removed
	// from it, and libobs destroys them later on its own thread. So the canvas must outlive
	// them: release them, let libobs finish destroying them, and only then release the canvas.
	std::vector<OBSWeakSource> scenes;
	for (auto &[original, m] : mirrors_) {
		if (m->mirror)
			scenes.emplace_back(OBSGetWeakRef(obs_scene_get_source(m->mirror)));
		destroy(*m);
	}
	mirrors_.clear();

	{
		std::lock_guard lock(map_mutex_);
		links_.clear();
		scene_map_.clear();
		scene_names_.clear();
		transition_ = nullptr;
		transition_original_ = nullptr;
	}

	obs_wait_for_destroy_queue();
	const bool all_gone = std::none_of(scenes.begin(), scenes.end(), [](const OBSWeakSource &weak) {
		OBSSourceAutoRelease scene = obs_weak_source_get_source(weak);
		return scene != nullptr;
	});

	obs_canvas_remove(canvas_);
	if (!all_gone) {
		// Something still holds a mirror scene; keep its canvas alive rather than risk a crash.
		obs_log(LOG_WARNING, "a recording scene outlived its canvas; keeping the canvas");
		obs_canvas_get_ref(canvas_);
	}
}

video_t *SceneMirror::video() const
{
	return canvas_ ? obs_canvas_get_video(canvas_) : nullptr;
}

SceneMirror::Mirrored *SceneMirror::mirror_for(obs_source_t *original_scene, bool create)
{
	auto it = mirrors_.find(original_scene);
	if (it != mirrors_.end())
		return it->second.get();
	if (!create || suspended_)
		return nullptr;

	auto m = std::make_unique<Mirrored>();
	m->original = original_scene;

	const std::string name = "selective-recording:" + source_uuid(original_scene);
	OBSSceneAutoRelease scene = obs_scene_create_private(name.c_str());
	if (!scene)
		return nullptr;

	// Join the private canvas before adding items so relative transforms use the right size.
	obs_canvas_move_scene(scene, canvas_);
	m->mirror = scene.Get();
	connect_scene_signals(original_scene, m->scene_signals);

	Mirrored *raw = m.get();
	mirrors_[original_scene] = std::move(m);
	{
		std::lock_guard lock(map_mutex_);
		scene_map_[original_scene] = obs_scene_get_source(raw->mirror);
	}
	build(*raw);
	return raw;
}

void SceneMirror::connect_scene_signals(obs_source_t *scene_source, std::vector<OBSSignal> &out)
{
	signal_handler_t *sh = obs_source_get_signal_handler(scene_source);
	out.emplace_back(sh, "item_add", on_structure_changed, this);
	out.emplace_back(sh, "item_remove", on_structure_changed, this);
	out.emplace_back(sh, "reorder", on_structure_changed, this);
	out.emplace_back(sh, "refresh", on_structure_changed, this);
	out.emplace_back(sh, "item_transform", on_item_transform, this);
	out.emplace_back(sh, "item_visible", on_item_visible, this);
}

void SceneMirror::plan_items(obs_scene_t *scene, Mirrored &m, std::vector<PlanNode> &out)
{
	for (auto &item : scene_items(scene)) {
		PlanNode node;
		node.original = item.Get();
		obs_source_t *source = obs_sceneitem_get_source(item);

		if (obs_sceneitem_is_group(item)) {
			node.is_group = true;
			obs_scene_t *group_scene = obs_sceneitem_group_get_scene(item);
			obs_source_t *group_source = obs_scene_get_source(group_scene);
			m.groups.emplace_back(group_source);
			connect_scene_signals(group_source, m.group_signals);
			plan_items(group_scene, m, node.children);
		} else if (obs_source_is_scene(source)) {
			// Nested scenes point at their own mirror so routing applies inside them too.
			Mirrored *child = mirror_for(source, true);
			if (!child)
				continue;
			node.add_source = obs_scene_get_source(child->mirror);
		} else {
			node.add_source = source;
		}
		out.push_back(std::move(node));
	}
}

void SceneMirror::apply_plan(obs_scene_t *dest, const std::vector<PlanNode> &plan, obs_source_t *owner,
			     std::vector<Link> &links)
{
	for (const PlanNode &node : plan) {
		obs_sceneitem_t *added = nullptr;

		if (node.is_group) {
			OBSSourceAutoRelease group =
				obs_source_create_private("group", "selective-recording group", nullptr);
			added = obs_scene_add(dest, group);
			if (!added)
				continue;
			obs_sceneitem_defer_group_resize_begin(added);
			apply_plan(obs_group_from_source(group), node.children, owner, links);
			obs_sceneitem_defer_group_resize_end(added);
		} else {
			added = obs_scene_add(dest, node.add_source);
			if (!added)
				continue;
		}

		copy_item_properties(node.original, added);
		obs_sceneitem_set_visible(added, visibility_ ? visibility_(node.original) : true);
		links.push_back({owner, node.original, added});
	}
}

void SceneMirror::build(Mirrored &m)
{
	obs_scene_t *original = obs_scene_from_source(m.original);
	if (!original || !m.mirror)
		return;

	// Read the original tree first, then lock only the mirror, so lock order never inverts.
	m.group_signals.clear();
	m.groups.clear();
	std::vector<PlanNode> plan;
	plan_items(original, m, plan);

	std::vector<Link> links;
	struct Context {
		SceneMirror *self;
		const std::vector<PlanNode> *plan;
		obs_source_t *owner;
		std::vector<Link> *links;
	} ctx{this, &plan, m.original, &links};

	obs_scene_atomic_update(
		m.mirror,
		[](void *param, obs_scene_t *scene) {
			auto *c = static_cast<Context *>(param);
			for (auto &item : scene_items(scene))
				obs_sceneitem_remove(item);
			c->self->apply_plan(scene, *c->plan, c->owner, *c->links);
		},
		&ctx);

	{
		std::lock_guard lock(map_mutex_);
		for (auto it = links_.begin(); it != links_.end();) {
			if (it->second.owner == m.original)
				it = links_.erase(it);
			else
				++it;
		}
		for (auto &link : links)
			links_[link.original.Get()] = link;
	}
	m.dirty = false;
}

void SceneMirror::destroy(Mirrored &m)
{
	m.scene_signals.clear();
	m.group_signals.clear();
	m.groups.clear();
	{
		std::lock_guard lock(map_mutex_);
		scene_map_.erase(m.original.Get());
		for (auto it = scene_names_.begin(); it != scene_names_.end();) {
			if (m.mirror && it->second.Get() == obs_scene_get_source(m.mirror))
				it = scene_names_.erase(it);
			else
				++it;
		}
		for (auto it = links_.begin(); it != links_.end();) {
			if (it->second.owner == m.original)
				it = links_.erase(it);
			else
				++it;
		}
	}
	if (m.mirror) {
		// Private scenes are not removed from their canvas automatically.
		obs_canvas_scene_remove(m.mirror);
		m.mirror = nullptr;
	}
}

void SceneMirror::sync_scene_list()
{
	if (suspended_)
		return;

	for (auto &scene : canvas_scenes(source_canvas_))
		mirror_for(scene, true);

	// Scenes nested from another canvas stay mirrored until they are deleted.
	for (auto it = mirrors_.begin(); it != mirrors_.end();) {
		if (obs_source_removed(it->first)) {
			destroy(*it->second);
			it = mirrors_.erase(it);
		} else {
			++it;
		}
	}
}

void SceneMirror::rebuild_dirty()
{
	// Copy first: building can add nested mirrors to the map.
	std::vector<Mirrored *> dirty;
	for (auto &[original, m] : mirrors_) {
		if (m->dirty)
			dirty.push_back(m.get());
	}
	for (Mirrored *m : dirty)
		build(*m);
}

void SceneMirror::mark_all_dirty()
{
	for (auto &[original, m] : mirrors_)
		m->dirty = true;
}

void SceneMirror::mark_dirty_containing(obs_source_t *scene_or_group)
{
	auto it = mirrors_.find(scene_or_group);
	if (it != mirrors_.end()) {
		it->second->dirty = true;
		return;
	}
	for (auto &[original, m] : mirrors_) {
		for (auto &group : m->groups) {
			if (group.Get() == scene_or_group)
				m->dirty = true;
		}
	}
}

void SceneMirror::copy_item_properties(obs_sceneitem_t *from, obs_sceneitem_t *to)
{
	obs_transform_info info = {};
	obs_sceneitem_get_info2(from, &info);
	obs_sceneitem_set_info2(to, &info);

	obs_sceneitem_crop crop = {};
	obs_sceneitem_get_crop(from, &crop);
	obs_sceneitem_set_crop(to, &crop);

	obs_sceneitem_set_scale_filter(to, obs_sceneitem_get_scale_filter(from));
	obs_sceneitem_set_blending_method(to, obs_sceneitem_get_blending_method(from));
	obs_sceneitem_set_blending_mode(to, obs_sceneitem_get_blending_mode(from));
}

void SceneMirror::update_item_visibility(obs_sceneitem_t *original)
{
	OBSSceneItem mirror;
	{
		std::lock_guard lock(map_mutex_);
		auto it = links_.find(original);
		if (it == links_.end())
			return;
		mirror = it->second.mirror;
	}
	const bool visible = visibility_ ? visibility_(original) : true;
	if (obs_sceneitem_visible(mirror) != visible)
		obs_sceneitem_set_visible(mirror, visible);
}

void SceneMirror::refresh_visibility()
{
	std::vector<std::pair<OBSSceneItem, OBSSceneItem>> pairs;
	{
		std::lock_guard lock(map_mutex_);
		pairs.reserve(links_.size());
		for (auto &[original, link] : links_)
			pairs.emplace_back(link.original, link.mirror);
	}
	for (auto &[original, mirror] : pairs) {
		const bool visible = visibility_ ? visibility_(original) : true;
		if (obs_sceneitem_visible(mirror) != visible)
			obs_sceneitem_set_visible(mirror, visible);
	}
}

void SceneMirror::sync_on_air_scenes()
{
	std::vector<OBSSource> on_air;
	OBSSourceAutoRelease channel = obs_canvas_get_channel(source_canvas_, 0);
	if (channel && obs_source_get_type(channel) == OBS_SOURCE_TYPE_TRANSITION) {
		for (auto target : {OBS_TRANSITION_SOURCE_A, OBS_TRANSITION_SOURCE_B}) {
			OBSSourceAutoRelease source = obs_transition_get_source(channel, target);
			if (source)
				on_air.emplace_back(source.Get());
		}
	} else if (channel) {
		on_air.emplace_back(channel.Get());
	}

	// Studio Mode can put a private duplicate of a scene on air; mirror it like any other.
	const std::vector<OBSSource> listed = canvas_scenes(source_canvas_);
	for (auto &source : on_air) {
		if (!obs_source_is_scene(source) || mirrors_.count(source))
			continue;
		Mirrored *m = mirror_for(source, true);
		if (m && !contains_source(listed, source))
			m->ephemeral = true;
	}

	for (auto it = mirrors_.begin(); it != mirrors_.end();) {
		Mirrored &m = *it->second;
		if (!m.ephemeral || contains_source(on_air, it->first)) {
			m.unused_checks = 0;
			++it;
			continue;
		}
		if (++m.unused_checks < 8) {
			++it;
			continue;
		}
		destroy(m);
		it = mirrors_.erase(it);
	}

	std::lock_guard lock(map_mutex_);
	scene_names_.clear();
	for (auto &[original, m] : mirrors_) {
		if (!m->ephemeral)
			scene_names_[source_name(original)] = obs_scene_get_source(m->mirror);
	}
}

void SceneMirror::reconcile()
{
	if (!canvas_ || suspended_)
		return;
	rebuild_dirty();
	sync_on_air_scenes();
	ensure_transition();
}

void SceneMirror::suspend()
{
	if (suspended_ || !canvas_)
		return;

	suspended_ = true;
	obs_canvas_set_channel(canvas_, 0, nullptr);
	{
		std::lock_guard lock(map_mutex_);
		transition_ = nullptr;
		transition_original_ = nullptr;
	}
	for (auto &[original, m] : mirrors_)
		destroy(*m);
	mirrors_.clear();
}

void SceneMirror::resume()
{
	if (!suspended_ || !canvas_)
		return;

	suspended_ = false;
	sync_scene_list();
	rebuild_dirty();
	sync_on_air_scenes();
	ensure_transition();
}

std::vector<OBSSource> SceneMirror::active_sources() const
{
	if (!canvas_)
		return {};
	OBSSourceAutoRelease root = obs_canvas_get_channel(canvas_, 0);
	return active_tree(root);
}

void SceneMirror::ensure_transition()
{
	OBSSourceAutoRelease channel = obs_canvas_get_channel(source_canvas_, 0);
	const bool is_transition = channel && obs_source_get_type(channel) == OBS_SOURCE_TYPE_TRANSITION;
	const std::string wanted_id = is_transition ? obs_source_get_id(channel) : kCutTransition;
	const std::string settings = is_transition ? settings_json(channel) : "";

	bool recreate;
	{
		std::lock_guard lock(map_mutex_);
		recreate = !transition_ || transition_id_ != wanted_id || transition_original_ != channel.Get();
	}

	if (!recreate) {
		if (settings != transition_settings_) {
			OBSDataAutoRelease data = obs_data_create_from_json(settings.c_str());
			obs_source_update(transition_, data);
			transition_settings_ = settings;
		}
		return;
	}

	OBSDataAutoRelease data = is_transition ? obs_source_get_settings(channel) : nullptr;
	OBSSourceAutoRelease transition =
		obs_source_create_private(wanted_id.c_str(), "selective-recording transition", data);
	if (!transition && wanted_id != kCutTransition)
		transition = obs_source_create_private(kCutTransition, "selective-recording transition", nullptr);
	if (!transition)
		return;

	obs_transition_set_size(transition, base_width_, base_height_);
	if (is_transition) {
		obs_transition_set_scale_type(transition, obs_transition_get_scale_type(channel));
		obs_transition_set_alignment(transition, obs_transition_get_alignment(channel));
	}

	{
		std::lock_guard lock(map_mutex_);
		transition_ = transition.Get();
		transition_original_ = channel.Get();
		transition_id_ = wanted_id;
		shown_a_ = nullptr;
		shown_b_ = nullptr;
	}
	transition_settings_ = settings;
	obs_canvas_set_channel(canvas_, 0, transition);
}

void SceneMirror::tick(void *param, float)
{
	static_cast<SceneMirror *>(param)->tick_transition();
}

void SceneMirror::tick_transition()
{
	OBSSourceAutoRelease channel = obs_canvas_get_channel(source_canvas_, 0);
	if (!channel)
		return;

	OBSSourceAutoRelease original_a;
	OBSSourceAutoRelease original_b;
	float time = 0.0f;
	if (obs_source_get_type(channel) == OBS_SOURCE_TYPE_TRANSITION) {
		original_a = obs_transition_get_source(channel, OBS_TRANSITION_SOURCE_A);
		original_b = obs_transition_get_source(channel, OBS_TRANSITION_SOURCE_B);
		time = obs_transition_get_time(channel);
	} else {
		original_a = obs_source_get_ref(channel);
	}

	OBSSource transition;
	OBSSource mirror_a;
	OBSSource mirror_b;
	{
		std::lock_guard lock(map_mutex_);
		transition = transition_;

		// A scene we haven't mirrored yet (a fresh Studio Mode duplicate) falls back to the
		// mirror of the scene it was copied from until its own mirror is ready.
		auto lookup = [this](obs_source_t *original) -> OBSSource {
			if (!original)
				return nullptr;
			auto it = scene_map_.find(original);
			if (it != scene_map_.end())
				return it->second;
			const char *name = obs_source_get_name(original);
			auto named = name ? scene_names_.find(name) : scene_names_.end();
			return named != scene_names_.end() ? named->second : nullptr;
		};
		mirror_a = lookup(original_a);
		mirror_b = lookup(original_b);
	}
	if (!transition)
		return;

	if (original_b && mirror_b) {
		// Follow the original transition's progress frame by frame, including the Studio Mode T-bar.
		if (shown_a_ != mirror_a.Get() || shown_b_ != mirror_b.Get()) {
			obs_transition_set(transition, mirror_a);
			obs_transition_start(transition, OBS_TRANSITION_MODE_MANUAL, 0, mirror_b);
			obs_transition_set_manual_torque(transition, 0.0f, 0.0f);
			shown_a_ = mirror_a.Get();
			shown_b_ = mirror_b.Get();
		}
		obs_transition_set_manual_time(transition, time);
		return;
	}

	if (shown_a_ != mirror_a.Get() || shown_b_) {
		obs_transition_set(transition, mirror_a);
		shown_a_ = mirror_a.Get();
		shown_b_ = nullptr;
	}
}

void SceneMirror::on_structure_changed(void *param, calldata_t *cd)
{
	auto *self = static_cast<SceneMirror *>(param);
	obs_scene_t *scene = static_cast<obs_scene_t *>(calldata_ptr(cd, "scene"));
	obs_source_t *scene_source = scene ? obs_scene_get_source(scene) : nullptr;
	std::weak_ptr<bool> alive = self->alive_;

	post_to_ui([self, alive, scene_source]() {
		if (alive.expired() || !*alive.lock())
			return;
		self->mark_dirty_containing(scene_source);
		self->rebuild_dirty();
	});
}

void SceneMirror::on_item_transform(void *param, calldata_t *cd)
{
	auto *self = static_cast<SceneMirror *>(param);
	obs_sceneitem_t *item = static_cast<obs_sceneitem_t *>(calldata_ptr(cd, "item"));
	if (!item)
		return;

	// Applied immediately so animated transforms stay frame-accurate.
	OBSSceneItem mirror;
	{
		std::lock_guard lock(self->map_mutex_);
		auto it = self->links_.find(item);
		if (it == self->links_.end())
			return;
		mirror = it->second.mirror;
	}
	copy_item_properties(item, mirror);
}

void SceneMirror::on_item_visible(void *param, calldata_t *cd)
{
	auto *self = static_cast<SceneMirror *>(param);
	OBSSceneItem item = static_cast<obs_sceneitem_t *>(calldata_ptr(cd, "item"));
	std::weak_ptr<bool> alive = self->alive_;

	post_to_ui([self, alive, item]() {
		if (alive.expired() || !*alive.lock())
			return;
		self->update_item_visibility(item);
	});
}

void SceneMirror::on_canvas_source_changed(void *param, calldata_t *cd)
{
	auto *self = static_cast<SceneMirror *>(param);
	obs_source_t *source = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	if (!source || !obs_source_is_scene(source))
		return;

	std::weak_ptr<bool> alive = self->alive_;
	post_to_ui([self, alive]() {
		if (alive.expired() || !*alive.lock())
			return;
		self->sync_scene_list();
		self->rebuild_dirty();
	});
}

void SceneMirror::on_source_removed(void *param, calldata_t *cd)
{
	auto *self = static_cast<SceneMirror *>(param);
	OBSSource source = static_cast<obs_source_t *>(calldata_ptr(cd, "source"));
	std::weak_ptr<bool> alive = self->alive_;

	post_to_ui([self, alive, source]() {
		if (alive.expired() || !*alive.lock())
			return;

		// Drop every mirrored reference to a deleted source right away (NFR-009).
		std::vector<obs_source_t *> owners;
		{
			std::lock_guard lock(self->map_mutex_);
			for (auto &[original, link] : self->links_) {
				if (obs_sceneitem_get_source(link.original) == source.Get())
					owners.push_back(link.owner);
			}
		}
		for (obs_source_t *owner : owners)
			self->mark_dirty_containing(owner);

		if (obs_source_is_scene(source))
			self->sync_scene_list();
		self->rebuild_dirty();
	});
}

} // namespace selective_recording
