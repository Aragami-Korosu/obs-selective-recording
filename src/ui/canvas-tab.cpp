// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "ui/canvas-tab.hpp"

#include "core/controller.hpp"
#include "core/obs-utils.hpp"
#include "core/pipeline.hpp"
#include "ui/preview-widget.hpp"
#include "ui/route-button.hpp"
#include "ui/settings-dialog.hpp"
#include "ui/simple-routes.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QSignalBlocker>
#include <QToolButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace selective_recording {

namespace {

enum Column { ColName = 0, ColVideo, ColAudio, ColWarning };
enum Filter { FilterAll = 0, FilterExcluded, FilterRecordingOnly, FilterWarnings };
constexpr int kPathRole = Qt::UserRole + 1;
constexpr int kRowRole = Qt::UserRole + 2;

QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

std::vector<OBSSceneItem> items_top_first(obs_scene_t *scene)
{
	std::vector<OBSSceneItem> items;
	obs_scene_enum_items(
		scene,
		[](obs_scene_t *, obs_sceneitem_t *item, void *param) {
			static_cast<std::vector<OBSSceneItem> *>(param)->emplace_back(item);
			return true;
		},
		&items);
	std::reverse(items.begin(), items.end());
	return items;
}

QString format_duration(uint64_t ms)
{
	const uint64_t s = ms / 1000;
	return QStringLiteral("%1:%2:%3")
		.arg(s / 3600, 2, 10, QLatin1Char('0'))
		.arg((s / 60) % 60, 2, 10, QLatin1Char('0'))
		.arg(s % 60, 2, 10, QLatin1Char('0'));
}

QString format_bytes(uint64_t bytes)
{
	const double mb = (double)bytes / (1024.0 * 1024.0);
	if (mb >= 1024.0)
		return QStringLiteral("%1 GB").arg(mb / 1024.0, 0, 'f', 2);
	return QStringLiteral("%1 MB").arg(mb, 0, 'f', 1);
}

QString track_list(uint32_t mask)
{
	QStringList tracks;
	for (int i = 0; i < MAX_AUDIO_MIXES; i++) {
		if (mask & (1u << i))
			tracks << QString::number(i + 1);
	}
	return tracks.join(QStringLiteral(", "));
}

} // namespace

CanvasTab::CanvasTab(Controller &controller, std::string key, QWidget *parent)
	: QWidget(parent),
	  controller_(controller),
	  key_(std::move(key))
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(4);

	preview_ = new PreviewWidget(this);
	preview_->setVisible(false);
	layout->addWidget(preview_, 2);

	auto *filter_row = new QHBoxLayout();
	search_ = new QLineEdit(this);
	search_->setPlaceholderText(text("Dock.Search"));
	search_->setClearButtonEnabled(true);
	filter_ = new QComboBox(this);
	filter_->addItem(text("Filter.All"), FilterAll);
	filter_->addItem(text("Filter.Excluded"), FilterExcluded);
	filter_->addItem(text("Filter.RecordingOnly"), FilterRecordingOnly);
	filter_->addItem(text("Filter.Warnings"), FilterWarnings);
	mode_button_ = new QToolButton(this);
	mode_button_->setText(text("Simple.Advanced"));
	mode_button_->setToolTip(text("Simple.AdvancedHelp"));
	mode_button_->setCheckable(true);
	filter_row->addWidget(search_, 1);
	filter_row->addWidget(filter_);
	filter_row->addWidget(mode_button_);
	layout->addLayout(filter_row);

	scene_label_ = new QLabel(this);
	scene_label_->setStyleSheet(QStringLiteral("font-weight: bold;"));
	layout->addWidget(scene_label_);

	warning_summary_ = new QLabel(this);
	warning_summary_->setWordWrap(true);
	warning_summary_->setStyleSheet(QStringLiteral("color: #d9a441;"));
	warning_summary_->setVisible(false);
	layout->addWidget(warning_summary_);

	tree_ = new QTreeWidget(this);
	tree_->setColumnCount(4);
	tree_->setHeaderLabels({text("Dock.Source"), text("Dock.Video"), text("Dock.Audio"), QString()});
	tree_->header()->setSectionResizeMode(ColName, QHeaderView::Stretch);
	tree_->header()->setSectionResizeMode(ColVideo, QHeaderView::ResizeToContents);
	tree_->header()->setSectionResizeMode(ColAudio, QHeaderView::ResizeToContents);
	tree_->header()->setSectionResizeMode(ColWarning, QHeaderView::ResizeToContents);
	tree_->header()->setStretchLastSection(false);
	tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
	tree_->setContextMenuPolicy(Qt::CustomContextMenu);
	tree_->setUniformRowHeights(true);
	layout->addWidget(tree_, 3);

	disabled_ = new QLabel(text("Dock.CanvasDisabled"), this);
	disabled_->setWordWrap(true);
	disabled_->setVisible(false);
	layout->addWidget(disabled_);

	error_ = new QLabel(this);
	error_->setWordWrap(true);
	error_->setStyleSheet(QStringLiteral("color: #e05d5d;"));
	error_->setVisible(false);
	error_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	layout->addWidget(error_);

	auto *status_row = new QHBoxLayout();
	state_ = new QLabel(this);
	file_ = new QLabel(this);
	file_->setTextInteractionFlags(Qt::TextSelectableByMouse);
	file_->setMinimumWidth(10);
	folder_ = new QToolButton(this);
	folder_->setText(text("Dock.OpenFolder"));
	folder_->setAutoRaise(true);
	status_row->addWidget(state_);
	status_row->addWidget(file_, 1);
	status_row->addWidget(folder_);
	layout->addLayout(status_row);

	auto *buttons = new QHBoxLayout();
	record_ = new QPushButton(this);
	pause_ = new QPushButton(this);
	preview_button_ = new QToolButton(this);
	preview_button_->setText(text("Dock.Preview"));
	preview_button_->setCheckable(true);
	auto *settings = new QToolButton(this);
	settings->setText(text("Dock.Settings"));
	buttons->addWidget(record_, 2);
	buttons->addWidget(pause_, 1);
	buttons->addWidget(preview_button_);
	buttons->addWidget(settings);
	layout->addLayout(buttons);

	connect(search_, &QLineEdit::textChanged, this, &CanvasTab::apply_filter);
	connect(filter_, &QComboBox::currentIndexChanged, this, &CanvasTab::apply_filter);
	connect(tree_, &QWidget::customContextMenuRequested, this, &CanvasTab::show_tree_menu);
	connect(record_, &QPushButton::clicked, this, &CanvasTab::toggle_recording);
	connect(pause_, &QPushButton::clicked, this, &CanvasTab::toggle_pause);
	connect(preview_button_, &QToolButton::toggled, this, &CanvasTab::toggle_preview);
	connect(settings, &QToolButton::clicked, this, &CanvasTab::open_settings);
	connect(folder_, &QToolButton::clicked, this, &CanvasTab::open_folder);
	connect(mode_button_, &QToolButton::toggled, this, [this](bool advanced) {
		controller_.settings().simple_mode = !advanced;
		controller_.settings().save();
		set_simple(!advanced);
	});

	simple_ = controller_.settings().simple_mode;
	mode_button_->setChecked(!simple_);
	set_simple(simple_);
	refresh_state();
}

void CanvasTab::set_simple(bool simple)
{
	simple_ = simple;
	if (mode_button_->isChecked() == simple) {
		QSignalBlocker block(mode_button_);
		mode_button_->setChecked(!simple);
	}
	filter_->setVisible(!simple);
	scene_label_->setVisible(simple);
	tree_->setColumnHidden(ColAudio, simple);
	tree_->headerItem()->setText(ColVideo, simple ? text("Simple.Column") : text("Dock.Video"));
	rebuild_if_changed(true);
}

OBSSource CanvasTab::current_scene() const
{
	Pipeline *pipeline = controller_.pipeline(key_);
	if (!pipeline)
		return nullptr;

	// Main canvas: the scene OBS's Sources list shows (the preview scene in Studio Mode).
	if (is_main_canvas(pipeline->canvas())) {
		OBSSourceAutoRelease scene = obs_frontend_preview_program_mode_active()
						     ? obs_frontend_get_current_preview_scene()
						     : obs_frontend_get_current_scene();
		return scene.Get();
	}

	OBSSourceAutoRelease channel = obs_canvas_get_channel(pipeline->canvas(), 0);
	if (channel && obs_source_get_type(channel) == OBS_SOURCE_TYPE_TRANSITION) {
		OBSSourceAutoRelease active = obs_transition_get_active_source(channel);
		return active.Get();
	}
	return channel.Get();
}

std::string CanvasTab::structure_signature() const
{
	Pipeline *pipeline = controller_.pipeline(key_);
	if (!pipeline)
		return {};

	std::string sig = simple_ ? "simple:" + source_uuid(current_scene()) +
					    (controller_.settings().folder_buttons ? ":folders" : "")
				  : "advanced";
	std::function<void(obs_scene_t *)> walk = [&](obs_scene_t *scene) {
		for (auto &item : items_top_first(scene)) {
			obs_source_t *source = obs_sceneitem_get_source(item);
			sig += std::to_string(obs_sceneitem_get_id(item)) + ":" + source_name(source) + ";";
			if (obs_sceneitem_is_group(item)) {
				sig += "{";
				walk(obs_sceneitem_group_get_scene(item));
				sig += "}";
			}
		}
	};

	for (auto &scene : canvas_scenes(pipeline->canvas())) {
		sig += "|" + source_uuid(scene) + ":" + source_name(scene) + "=";
		walk(obs_scene_from_source(scene));
	}
	for (auto &source : global_audio_sources())
		sig += "|a:" + source_uuid(source) + ":" + source_name(source);
	return sig;
}

void CanvasTab::release()
{
	preview_->set_canvas(nullptr);
	tree_->clear();
	rows_.clear();
	signature_.clear();
}

void CanvasTab::rebuild_if_changed(bool force)
{
	std::string sig = structure_signature();
	if (!force && sig == signature_) {
		// Visibility and locks change without changing the structure.
		if (simple_)
			refresh_routes();
		return;
	}
	signature_ = std::move(sig);
	build_tree();
}

void CanvasTab::build_tree()
{
	QSet<QString> expanded;
	std::function<void(QTreeWidgetItem *)> remember = [&](QTreeWidgetItem *item) {
		if (item->isExpanded())
			expanded.insert(item->data(ColName, kPathRole).toString());
		for (int i = 0; i < item->childCount(); i++)
			remember(item->child(i));
	};
	const bool first_build = tree_->topLevelItemCount() == 0;
	for (int i = 0; i < tree_->topLevelItemCount(); i++)
		remember(tree_->topLevelItem(i));

	tree_->clear();
	rows_.clear();

	Pipeline *pipeline = controller_.pipeline(key_);
	if (!pipeline)
		return;

	if (simple_) {
		build_simple();
		return;
	}

	OBSSourceAutoRelease program = obs_canvas_get_channel(pipeline->canvas(), 0);
	OBSSourceAutoRelease current = program && obs_source_get_type(program) == OBS_SOURCE_TYPE_TRANSITION
					       ? obs_transition_get_active_source(program)
					       : obs_source_get_ref(program);

	for (auto &scene : canvas_scenes(pipeline->canvas())) {
		auto *top = new QTreeWidgetItem(tree_);
		const std::string name = source_name(scene);
		top->setText(ColName, QString::fromStdString(name));
		top->setData(ColName, kPathRole, QString::fromStdString("scene:" + source_uuid(scene)));
		QFont font = top->font(ColName);
		font.setBold(true);
		top->setFont(ColName, font);
		add_scene_items(top, obs_scene_from_source(scene), name);

		const QString path = top->data(ColName, kPathRole).toString();
		top->setExpanded(expanded.contains(path) || (first_build && scene.Get() == current.Get()));
	}

	std::vector<OBSSource> globals = global_audio_sources();
	if (!globals.empty()) {
		auto *section = new QTreeWidgetItem(tree_);
		section->setText(ColName, text("Dock.AudioDevices"));
		section->setData(ColName, kPathRole, QStringLiteral("audio-devices"));
		QFont font = section->font(ColName);
		font.setBold(true);
		section->setFont(ColName, font);
		for (auto &source : globals) {
			auto *row = new QTreeWidgetItem(section);
			row->setText(ColName, QString::fromStdString(source_name(source)));
			row->setData(ColName, kPathRole, QString::fromStdString("audio:" + source_uuid(source)));
			add_row(row, source, nullptr, {}, false, true);
		}
		section->setExpanded(first_build || expanded.contains(QStringLiteral("audio-devices")));
	}

	refresh_routes();
	refresh_warnings();
	apply_filter();
}

void CanvasTab::build_simple()
{
	OBSSource scene = current_scene();
	const std::string name = source_name(scene);
	scene_label_->setText(text("Simple.Scene").arg(QString::fromStdString(name)));

	obs_scene_t *s = obs_scene_from_source(scene);
	if (s)
		add_simple_items(nullptr, s, name);

	refresh_routes();
	refresh_warnings();
	apply_filter();
}

void CanvasTab::add_simple_items(QTreeWidgetItem *parent, obs_scene_t *scene, const std::string &scene_name)
{
	for (auto &item : items_top_first(scene)) {
		obs_source_t *source = obs_sceneitem_get_source(item);
		auto *tree_item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree_);
		tree_item->setText(ColName, QString::fromStdString(source_name(source)));

		const char *type = obs_source_get_display_name(obs_source_get_id(source));
		tree_item->setToolTip(ColName, type ? QString::fromUtf8(type) : QString());
		add_simple_row(tree_item, source, item, scene_name);

		if (obs_sceneitem_is_group(item)) {
			add_simple_items(tree_item, obs_sceneitem_group_get_scene(item), scene_name);
			tree_item->setExpanded(true);
		}
	}
}

void CanvasTab::add_simple_row(QTreeWidgetItem *tree_item, obs_source_t *source, obs_sceneitem_t *item,
			       const std::string &scene_name)
{
	Row row;
	row.source = source;
	row.item = item;
	row.uuid = source_uuid(source);
	row.scene_name = scene_name;
	row.tree_item = tree_item;
	const size_t index = rows_.size();
	tree_item->setData(ColName, kRowRole, (qulonglong)index);

	// Folders get a button only when it's turned on in Settings; it sets the sources inside.
	const bool folder = obs_sceneitem_is_group(item);
	if (!folder || controller_.settings().folder_buttons) {
		row.placement = new QToolButton(tree_);
		row.placement->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		row.placement->setIconSize(QSize(16, 16));
		row.placement->setMinimumWidth(120);
		row.placement->setCursor(Qt::PointingHandCursor);
		row.placement->setContextMenuPolicy(Qt::CustomContextMenu);
		tree_->setItemWidget(tree_item, ColVideo, row.placement);
		connect(row.placement, &QToolButton::clicked, this, [this, index, folder]() {
			if (folder)
				cycle_folder_route(controller_, key_, rows_[index].item);
			else
				cycle_placement_route(controller_, key_, rows_[index].item);
		});
		connect(row.placement, &QWidget::customContextMenuRequested, this, [this, index](const QPoint &pos) {
			show_placement_menu(rows_[index], rows_[index].placement->mapToGlobal(pos));
		});
	}

	row.warning = new QLabel(tree_);
	row.warning->setMinimumWidth(18);
	tree_->setItemWidget(tree_item, ColWarning, row.warning);
	rows_.push_back(std::move(row));
}

void CanvasTab::show_placement_menu(const Row &row, const QPoint &pos)
{
	OBSSceneItem item = row.item;
	const bool folder = obs_sceneitem_is_group(item);
	const FolderRoute folder_current = folder_route(controller_, key_, item);
	const PlacementRoute current = folder ? PlacementRoute{folder_current.route, folder_current.locked}
					      : placement_route(controller_, key_, item);

	QMenu menu(this);
	menu.addSection(QString::fromStdString(source_name(row.source)));
	for (Route r : {Route::Both, Route::StreamOnly, Route::RecordingOnly}) {
		QAction *action = menu.addAction(route_icon(r), route_label(r), this, [this, item, r, folder]() {
			if (folder)
				set_folder_route(controller_, key_, item, r);
			else
				set_placement_route(controller_, key_, item, r);
		});
		action->setCheckable(true);
		action->setChecked(r == current.route && !(folder && folder_current.mixed));
		action->setEnabled(!current.locked);
	}
	menu.addSeparator();
	menu.addAction(text("Simple.Advanced"), this, [this]() { mode_button_->setChecked(true); });
	menu.exec(pos);
}

void CanvasTab::add_scene_items(QTreeWidgetItem *parent, obs_scene_t *scene, const std::string &scene_name)
{
	for (auto &item : items_top_first(scene)) {
		obs_source_t *source = obs_sceneitem_get_source(item);
		auto *tree_item = new QTreeWidgetItem(parent);
		tree_item->setText(ColName, QString::fromStdString(source_name(source)));
		tree_item->setData(ColName, kPathRole,
				   parent->data(ColName, kPathRole).toString() + QStringLiteral("/") +
					   QString::number(obs_sceneitem_get_id(item)));

		const char *type = obs_source_get_display_name(obs_source_get_id(source));
		tree_item->setToolTip(ColName, type ? QString::fromUtf8(type) : QString());
		if (!obs_sceneitem_visible(item))
			tree_item->setForeground(ColName, palette().brush(QPalette::Disabled, QPalette::Text));

		const bool is_group = obs_sceneitem_is_group(item);
		const bool is_scene = obs_source_is_scene(source);
		// Folders follow OBS's eye icon; the sources inside them have the buttons.
		add_row(tree_item, source, item, scene_name, (has_video(source) || is_scene) && !is_group,
			has_audio(source) && !is_group && !is_scene);

		if (is_group) {
			add_scene_items(tree_item, obs_sceneitem_group_get_scene(item), scene_name);
			tree_item->setExpanded(true);
		}
	}
}

void CanvasTab::add_row(QTreeWidgetItem *tree_item, obs_source_t *source, obs_sceneitem_t *item,
			const std::string &scene_name, bool allow_video, bool allow_audio)
{
	Row row;
	row.source = source;
	row.item = item;
	row.uuid = source_uuid(source);
	row.scene_name = scene_name;
	row.tree_item = tree_item;
	const size_t index = rows_.size();
	tree_item->setData(ColName, kRowRole, (qulonglong)index);

	if (allow_video) {
		row.video = new RouteButton(RouteButton::Kind::Video, tree_);
		tree_->setItemWidget(tree_item, ColVideo, row.video);
		connect(row.video, &RouteButton::cycleRequested, this, [this, index]() { cycle_video(rows_[index]); });
		connect(row.video, &RouteButton::menuRequested, this,
			[this, index](const QPoint &pos) { show_video_menu(rows_[index], pos); });
	}
	if (allow_audio) {
		row.audio = new RouteButton(RouteButton::Kind::Audio, tree_);
		tree_->setItemWidget(tree_item, ColAudio, row.audio);
		connect(row.audio, &RouteButton::cycleRequested, this, [this, index]() { cycle_audio(rows_[index]); });
		connect(row.audio, &RouteButton::menuRequested, this,
			[this, index](const QPoint &pos) { show_audio_menu(rows_[index], pos); });
	}

	row.warning = new QLabel(tree_);
	row.warning->setMinimumWidth(18);
	tree_->setItemWidget(tree_item, ColWarning, row.warning);
	rows_.push_back(std::move(row));
}

void CanvasTab::refresh_routes()
{
	const RouteStore &routes = controller_.routes();
	for (Row &row : rows_) {
		if (row.video)
			row.video->set_route(routes.resolve_video(key_, row.source, row.item));
		if (row.audio)
			row.audio->set_route(routes.resolve_audio(key_, row.source));
		if (row.placement && obs_sceneitem_is_group(row.item)) {
			const FolderRoute route = folder_route(controller_, key_, row.item);
			row.placement->setIcon(route_icon(route.route, route.mixed));
			row.placement->setText(route.mixed ? text("Folder.Mixed") : route_label(route.route));
			row.placement->setToolTip(folder_tooltip(route));
			row.placement->setEnabled(!route.locked && !route.empty);
		} else if (row.placement) {
			const PlacementRoute route = placement_route(controller_, key_, row.item);
			row.placement->setIcon(route_icon(route.route));
			row.placement->setText(route_label(route.route));
			row.placement->setToolTip(route_tooltip(route));
			row.placement->setEnabled(!route.locked);
			const QPalette::ColorGroup group = obs_sceneitem_visible(row.item) ? QPalette::Active
											   : QPalette::Disabled;
			row.tree_item->setForeground(ColName, palette().brush(group, QPalette::Text));
		}
	}
	apply_filter();
}

void CanvasTab::refresh_warnings()
{
	const auto &all = controller_.warnings();
	auto canvas_it = all.find(key_);
	int count = 0;

	for (Row &row : rows_) {
		const SourceWarning *w = nullptr;
		if (canvas_it != all.end()) {
			auto it = canvas_it->second.find(row.uuid);
			if (it != canvas_it->second.end() && it->second.any())
				w = &it->second;
		}

		if (!w) {
			row.warning->clear();
			row.warning->setToolTip(QString());
			continue;
		}

		QStringList lines;
		if (w->video_live) {
			QStringList scenes;
			for (auto &s : w->scenes)
				scenes << QString::fromStdString(s);
			scenes.removeDuplicates();
			lines << text("Warning.VideoLive").arg(scenes.join(QStringLiteral(", ")));
		}
		if (w->audio_live) {
			lines << text("Warning.AudioLive").arg(track_list(w->tracks));
			if (w->tracks_assumed)
				lines << text("Warning.TracksAssumed");
		}
		if (w->shared_track)
			lines << text("Warning.SharedTrack").arg(track_list(w->tracks));

		row.warning->setText(QStringLiteral("⚠"));
		row.warning->setStyleSheet(QStringLiteral("color: #d9a441;"));
		row.warning->setToolTip(lines.join(QStringLiteral("\n\n")));
		count++;
	}

	warning_summary_->setVisible(count > 0);
	if (count > 0)
		warning_summary_->setText(text("Warning.Summary").arg(count));
	apply_filter();
}

void CanvasTab::refresh_state()
{
	Pipeline *pipeline = controller_.pipeline(key_);
	const bool canvas_enabled = controller_.settings().canvas(key_).enabled;
	disabled_->setVisible(!canvas_enabled);

	if (!pipeline) {
		record_->setEnabled(false);
		pause_->setEnabled(false);
		return;
	}

	const PipelineState state = pipeline->state();
	const bool recording = pipeline->recording();
	const bool stopping = state == PipelineState::Stopping;

	record_->setText(recording || stopping ? text("Dock.Stop") : text("Dock.Start"));
	record_->setEnabled(!stopping && (recording || canvas_enabled));
	pause_->setText(state == PipelineState::Paused ? text("Dock.Resume") : text("Dock.Pause"));
	pause_->setEnabled(recording);

	QString status;
	switch (state) {
	case PipelineState::Idle:
		status = text("State.Idle");
		break;
	case PipelineState::Recording:
		status = QStringLiteral("● ") + text("State.Recording");
		break;
	case PipelineState::Paused:
		status = QStringLiteral("❚❚ ") + text("State.Paused");
		break;
	case PipelineState::Stopping:
		status = text("State.Stopping");
		break;
	case PipelineState::Failed:
		status = text("State.Failed");
		break;
	}

	if (pipeline->busy()) {
		const RecorderStats stats = pipeline->stats();
		status +=
			QStringLiteral("  %1 · %2").arg(format_duration(stats.duration_ms), format_bytes(stats.bytes));
		if (stats.dropped_frames > 0)
			status += QStringLiteral(" · ") + text("State.Dropped").arg(stats.dropped_frames);
		state_->setToolTip(text("State.Details")
					   .arg(QString::fromStdString(pipeline->encoder_name()))
					   .arg(pipeline->audio_latency_ms()));
		const QString path = QString::fromStdString(pipeline->file_path());
		file_->setText(QFileInfo(path).fileName());
		file_->setToolTip(path);
	} else if (!file_->toolTip().isEmpty()) {
		file_->setText(text("State.Saved") + QStringLiteral(" ") + QFileInfo(file_->toolTip()).fileName());
	}
	state_->setText(status);
	state_->setStyleSheet(recording ? QStringLiteral("color: #e05d5d; font-weight: bold;") : QString());
	file_->setVisible(pipeline->busy() || !file_->text().isEmpty());
	folder_->setVisible(!file_->toolTip().isEmpty());

	const bool failed = state == PipelineState::Failed;
	error_->setVisible(failed);
	if (failed)
		error_->setText(QString::fromStdString(pipeline->last_error()));

	if (preview_button_->isChecked())
		preview_->set_canvas(pipeline->render_canvas());
}

void CanvasTab::cycle_video(const Row &row)
{
	RouteStore &routes = controller_.routes();
	const ResolvedRoute current = routes.resolve_video(key_, row.source, row.item);
	const Route next = next_route(current.route);
	if (current.origin == RouteOrigin::Item)
		routes.set_item_video(row.item, next);
	else if (current.origin == RouteOrigin::Canvas)
		routes.set_video(key_, row.source, next);
	else
		routes.set_video("", row.source, next);
	controller_.routes_changed();
}

void CanvasTab::cycle_audio(const Row &row)
{
	RouteStore &routes = controller_.routes();
	const ResolvedRoute current = routes.resolve_audio(key_, row.source);
	const Route next = next_route(current.route);
	routes.set_audio(current.origin == RouteOrigin::Canvas ? key_ : "", row.source, next);
	controller_.routes_changed();
}

void CanvasTab::show_video_menu(const Row &row, const QPoint &pos)
{
	RouteStore &routes = controller_.routes();
	Pipeline *pipeline = controller_.pipeline(key_);
	const QString canvas_name = pipeline ? QString::fromStdString(pipeline->name()) : QString();
	OBSSource source = row.source;
	OBSSceneItem item = row.item;

	QMenu menu(this);
	menu.addSection(text("Menu.VideoEverywhere"));
	for (Route r : {Route::Both, Route::StreamOnly, Route::RecordingOnly, Route::Off}) {
		menu.addAction(RouteButton::label(r), this, [this, &routes, source, r]() {
			routes.set_video("", source, r);
			controller_.routes_changed();
		});
	}

	QMenu *canvas_menu = menu.addMenu(text("Menu.OnlyCanvas").arg(canvas_name));
	for (Route r : {Route::Both, Route::StreamOnly, Route::RecordingOnly, Route::Off}) {
		canvas_menu->addAction(RouteButton::label(r), this, [this, &routes, source, r]() {
			routes.set_video(key_, source, r);
			controller_.routes_changed();
		});
	}
	canvas_menu->addSeparator();
	canvas_menu->addAction(text("Menu.Clear"), this, [this, &routes, source]() {
		routes.set_video(key_, source, std::nullopt);
		controller_.routes_changed();
	});

	if (item) {
		QMenu *item_menu = menu.addMenu(text("Menu.OnlyItem").arg(QString::fromStdString(row.scene_name)));
		for (Route r : {Route::Both, Route::StreamOnly, Route::RecordingOnly, Route::Off}) {
			item_menu->addAction(RouteButton::label(r), this, [this, &routes, item, r]() {
				routes.set_item_video(item, r);
				controller_.routes_changed();
			});
		}
		item_menu->addSeparator();
		item_menu->addAction(text("Menu.Clear"), this, [this, &routes, item]() {
			routes.set_item_video(item, std::nullopt);
			controller_.routes_changed();
		});
	}

	menu.addSeparator();
	menu.addAction(text("Menu.Reset"), this, [this, &routes, source]() {
		routes.set_video("", source, std::nullopt);
		controller_.routes_changed();
	});
	menu.exec(pos);
}

void CanvasTab::show_audio_menu(const Row &row, const QPoint &pos)
{
	RouteStore &routes = controller_.routes();
	Pipeline *pipeline = controller_.pipeline(key_);
	const QString canvas_name = pipeline ? QString::fromStdString(pipeline->name()) : QString();
	OBSSource source = row.source;

	QMenu menu(this);
	menu.addSection(text("Menu.AudioEverywhere"));
	for (Route r : {Route::Both, Route::StreamOnly, Route::RecordingOnly, Route::Off}) {
		menu.addAction(RouteButton::label(r), this, [this, &routes, source, r]() {
			routes.set_audio("", source, r);
			controller_.routes_changed();
		});
	}

	QMenu *canvas_menu = menu.addMenu(text("Menu.OnlyCanvas").arg(canvas_name));
	for (Route r : {Route::Both, Route::StreamOnly, Route::RecordingOnly, Route::Off}) {
		canvas_menu->addAction(RouteButton::label(r), this, [this, &routes, source, r]() {
			routes.set_audio(key_, source, r);
			controller_.routes_changed();
		});
	}
	canvas_menu->addSeparator();
	canvas_menu->addAction(text("Menu.Clear"), this, [this, &routes, source]() {
		routes.set_audio(key_, source, std::nullopt);
		controller_.routes_changed();
	});

	menu.addSeparator();
	menu.addAction(text("Menu.Gain"), this, [this, &routes, source]() {
		bool ok = false;
		const double current = routes.gain_db("", source);
		const double value = QInputDialog::getDouble(this, text("Menu.GainTitle"), text("Menu.GainLabel"),
							     current, -30.0, 20.0, 1, &ok);
		if (!ok)
			return;
		routes.set_gain("", source, value == 0.0 ? std::nullopt : std::optional<double>(value));
		controller_.routes_changed();
	});
	menu.addAction(text("Menu.Reset"), this, [this, &routes, source]() {
		routes.set_audio("", source, std::nullopt);
		routes.set_gain("", source, std::nullopt);
		controller_.routes_changed();
	});
	menu.exec(pos);
}

void CanvasTab::show_tree_menu(const QPoint &pos)
{
	std::vector<OBSSource> selected;
	std::vector<OBSSceneItem> selected_items;
	for (QTreeWidgetItem *item : tree_->selectedItems()) {
		const QVariant index = item->data(ColName, kRowRole);
		if (!index.isValid())
			continue;
		const size_t i = (size_t)index.toULongLong();
		if (i >= rows_.size())
			continue;
		if (!contains_source(selected, rows_[i].source))
			selected.push_back(rows_[i].source);
		if (rows_[i].item)
			selected_items.push_back(rows_[i].item);
	}
	if (selected.empty())
		return;

	RouteStore &routes = controller_.routes();
	QMenu menu(this);

	if (simple_) {
		menu.addSection(text("Menu.Selected").arg(selected_items.size()));
		for (Route r : {Route::Both, Route::StreamOnly, Route::RecordingOnly}) {
			menu.addAction(route_icon(r), route_label(r), this, [this, selected_items, r]() {
				for (auto &item : selected_items)
					set_placement_route(controller_, key_, item, r);
			});
		}
		menu.exec(tree_->viewport()->mapToGlobal(pos));
		return;
	}

	menu.addSection(text("Menu.Selected").arg(selected.size()));

	QMenu *video = menu.addMenu(text("Dock.Video"));
	QMenu *audio = menu.addMenu(text("Dock.Audio"));
	for (Route r : {Route::Both, Route::StreamOnly, Route::RecordingOnly, Route::Off}) {
		video->addAction(RouteButton::label(r), this, [this, &routes, selected, r]() {
			for (auto &s : selected)
				routes.set_video("", s, r);
			controller_.routes_changed();
		});
		audio->addAction(RouteButton::label(r), this, [this, &routes, selected, r]() {
			for (auto &s : selected) {
				if (has_audio(s))
					routes.set_audio("", s, r);
			}
			controller_.routes_changed();
		});
	}

	menu.addSeparator();
	menu.addAction(text("Menu.Reset"), this, [this, &routes, selected]() {
		for (auto &s : selected) {
			routes.set_video("", s, std::nullopt);
			routes.set_audio("", s, std::nullopt);
		}
		controller_.routes_changed();
	});
	menu.exec(tree_->viewport()->mapToGlobal(pos));
}

void CanvasTab::apply_filter()
{
	const QString needle = search_->text().trimmed();
	const int mode = filter_->currentData().toInt();
	const auto &all = controller_.warnings();
	auto canvas_it = all.find(key_);

	auto row_matches = [&](QTreeWidgetItem *item) {
		const QVariant index = item->data(ColName, kRowRole);
		if (!index.isValid())
			return false;
		const Row &row = rows_[(size_t)index.toULongLong()];

		if (!needle.isEmpty() && !item->text(ColName).contains(needle, Qt::CaseInsensitive))
			return false;

		Route video = row.video ? row.video->route() : Route::Both;
		Route audio = row.audio ? row.audio->route() : Route::Both;
		if (row.placement) {
			video = audio = placement_route(controller_, key_, row.item).route;
			if (mode == FilterExcluded)
				return !in_recording(video);
		}
		switch (mode) {
		case FilterExcluded:
			return (row.video && !in_recording(video)) || (row.audio && !in_recording(audio));
		case FilterRecordingOnly:
			return video == Route::RecordingOnly || audio == Route::RecordingOnly;
		case FilterWarnings:
			return canvas_it != all.end() && canvas_it->second.count(row.uuid) &&
			       canvas_it->second.at(row.uuid).any();
		default:
			return true;
		}
	};

	const bool filtering = !needle.isEmpty() || mode != FilterAll;
	std::function<bool(QTreeWidgetItem *)> visit = [&](QTreeWidgetItem *item) {
		bool any_child = false;
		for (int i = 0; i < item->childCount(); i++)
			any_child = visit(item->child(i)) || any_child;
		const bool show = !filtering || row_matches(item) || any_child;
		item->setHidden(!show);
		if (filtering && any_child)
			item->setExpanded(true);
		return show;
	};
	for (int i = 0; i < tree_->topLevelItemCount(); i++)
		visit(tree_->topLevelItem(i));
}

void CanvasTab::toggle_recording()
{
	Pipeline *pipeline = controller_.pipeline(key_);
	if (!pipeline)
		return;
	if (pipeline->recording()) {
		pipeline->stop();
	} else {
		pipeline->clear_error();
		pipeline->start();
	}
	refresh_state();
}

void CanvasTab::toggle_pause()
{
	Pipeline *pipeline = controller_.pipeline(key_);
	if (pipeline)
		pipeline->set_paused(pipeline->state() != PipelineState::Paused);
	refresh_state();
}

void CanvasTab::toggle_preview(bool enabled)
{
	Pipeline *pipeline = controller_.pipeline(key_);
	if (!pipeline)
		return;

	if (!enabled)
		preview_->set_canvas(nullptr);
	pipeline->set_preview(enabled);
	preview_->setVisible(enabled);
	if (enabled)
		preview_->set_canvas(pipeline->render_canvas());
}

void CanvasTab::open_settings()
{
	QPointer<CanvasTab> self(this);
	QPointer<SettingsDialog> dialog = new SettingsDialog(controller_, key_, this);
	dialog->setAttribute(Qt::WA_DeleteOnClose);
	dialog->exec();
	if (self)
		refresh_state();
}

void CanvasTab::open_folder()
{
	const QString path = file_->toolTip();
	if (!path.isEmpty())
		QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
}

} // namespace selective_recording
