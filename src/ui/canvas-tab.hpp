// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include "core/routes.hpp"

#include <obs.hpp>

#include <QWidget>

#include <string>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace selective_recording {

class Controller;
class PreviewWidget;
class RouteButton;

// Routing and recording controls for one canvas.
class CanvasTab : public QWidget {
	Q_OBJECT

public:
	CanvasTab(Controller &controller, std::string key, QWidget *parent = nullptr);

	const std::string &key() const { return key_; }

	void refresh_routes();
	void refresh_state();
	void refresh_warnings();
	void rebuild_if_changed(bool force = false);

	// Drops every source reference now, before the tab itself is deleted later.
	void release();

private:
	struct Row {
		OBSSource source;
		OBSSceneItem item;
		std::string uuid;
		std::string scene_name;
		QTreeWidgetItem *tree_item = nullptr;
		RouteButton *route = nullptr;     // All scenes view: the source's route
		RouteButton *placement = nullptr; // current-scene view: the placement's route
		QLabel *warning = nullptr;
	};

	void build_tree();
	void build_simple();
	void add_simple_items(QTreeWidgetItem *parent, obs_scene_t *scene, const std::string &scene_name);
	void add_simple_row(QTreeWidgetItem *tree_item, obs_source_t *source, obs_sceneitem_t *item,
			    const std::string &scene_name);
	void show_placement_menu(const Row &row, const QPoint &pos);
	void set_simple(bool simple);
	OBSSource current_scene() const;
	void add_scene_items(QTreeWidgetItem *parent, obs_scene_t *scene, const std::string &scene_name);
	void add_row(QTreeWidgetItem *tree_item, obs_source_t *source, obs_sceneitem_t *item,
		     const std::string &scene_name, bool has_route);
	std::string structure_signature() const;

	void cycle_route(const Row &row);
	void show_route_menu(const Row &row, const QPoint &pos);
	void show_tree_menu(const QPoint &pos);
	void apply_filter();

	void toggle_recording();
	void toggle_pause();
	void toggle_preview(bool enabled);
	void open_settings();
	void open_folder();

	Controller &controller_;
	std::string key_;
	std::vector<Row> rows_;
	std::string signature_;
	bool simple_ = true;
	QLabel *scene_label_ = nullptr;
	QToolButton *mode_button_ = nullptr;

	QLineEdit *search_ = nullptr;
	QComboBox *filter_ = nullptr;
	QTreeWidget *tree_ = nullptr;
	QLabel *warning_summary_ = nullptr;
	PreviewWidget *preview_ = nullptr;
	QLabel *state_ = nullptr;
	QLabel *file_ = nullptr;
	QLabel *error_ = nullptr;
	QLabel *disabled_ = nullptr;
	QPushButton *record_ = nullptr;
	QPushButton *pause_ = nullptr;
	QToolButton *preview_button_ = nullptr;
	QToolButton *folder_ = nullptr;
};

} // namespace selective_recording
