// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "ui/source-list-buttons.hpp"

#include "core/controller.hpp"
#include "core/obs-utils.hpp"
#include "ui/simple-routes.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QAbstractItemView>
#include <QHBoxLayout>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QToolButton>

#include <algorithm>
#include <vector>

namespace selective_recording {

namespace {

constexpr const char *kButtonName = "selectiveRecordingButton";
constexpr int kSyncMs = 400;
constexpr int kRescanTicks = 5;

// Same order as OBS's Sources list: top item first, expanded group children under their group.
bool enum_item(obs_scene_t *, obs_sceneitem_t *item, void *param)
{
	auto *items = static_cast<std::vector<OBSSceneItem> *>(param);
	if (obs_source_removed(obs_sceneitem_get_source(item)))
		return true;

	if (obs_sceneitem_is_group(item)) {
		OBSDataAutoRelease data = obs_sceneitem_get_private_settings(item);
		if (!obs_data_get_bool(data, "collapsed"))
			obs_scene_enum_items(obs_sceneitem_group_get_scene(item), enum_item, items);
	}
	items->insert(items->begin(), OBSSceneItem(item));
	return true;
}

std::vector<OBSSceneItem> scene_items(obs_scene_t *scene)
{
	std::vector<OBSSceneItem> items;
	if (scene)
		obs_scene_enum_items(scene, enum_item, &items);
	return items;
}

// The scene OBS's own Sources list shows.
std::vector<OBSSceneItem> main_list_items()
{
	OBSSourceAutoRelease scene_source = obs_frontend_preview_program_mode_active()
						    ? obs_frontend_get_current_preview_scene()
						    : obs_frontend_get_current_scene();
	return scene_items(obs_scene_from_source(scene_source));
}

bool row_shows(QWidget *row, const char *name)
{
	if (!name)
		return false;
	const QString expected = QString::fromUtf8(name);
	for (QLabel *label : row->findChildren<QLabel *>()) {
		if (label->text() == expected)
			return true;
	}
	return false;
}

// True when every row the list has built shows the matching item's source name.
bool rows_match(QAbstractItemView *list, const std::vector<OBSSceneItem> &items)
{
	QAbstractItemModel *model = list->model();
	if (!model || (int)items.size() != model->rowCount())
		return false;
	for (int row = 0; row < model->rowCount(); row++) {
		QWidget *widget = list->indexWidget(model->index(row, 0));
		if (widget && !row_shows(widget, obs_source_get_name(obs_sceneitem_get_source(items[(size_t)row]))))
			return false;
	}
	return true;
}

// Finds which canvas and scene another canvas's Sources list (such as Aitum Vertical's)
// is showing. That list belongs to another plugin, so it is identified by its rows: the
// canvas's on-air scene is tried first (both sides while a transition runs), then every
// other scene of the canvas. Only extra canvases are searched; the main list is separate.
bool match_canvas_list(QAbstractItemView *list, std::string &canvas_key, std::vector<OBSSceneItem> &items)
{
	const std::vector<CanvasInfo> canvases = list_canvases();

	auto try_scene = [&](obs_source_t *source, const CanvasInfo &info) {
		obs_scene_t *scene = obs_scene_from_source(source);
		if (!scene)
			return false;
		std::vector<OBSSceneItem> candidate = scene_items(scene);
		if (!rows_match(list, candidate))
			return false;
		canvas_key = info.key;
		items = std::move(candidate);
		return true;
	};

	for (const CanvasInfo &info : canvases) {
		if (info.is_main)
			continue;
		OBSSourceAutoRelease channel = obs_canvas_get_channel(info.canvas, 0);
		if (!channel)
			continue;
		if (obs_source_get_type(channel) == OBS_SOURCE_TYPE_TRANSITION) {
			for (auto side : {OBS_TRANSITION_SOURCE_B, OBS_TRANSITION_SOURCE_A}) {
				OBSSourceAutoRelease scene = obs_transition_get_source(channel, side);
				if (try_scene(scene, info))
					return true;
			}
		} else if (try_scene(channel, info)) {
			return true;
		}
	}

	for (const CanvasInfo &info : canvases) {
		if (info.is_main)
			continue;
		for (const OBSSource &scene : canvas_scenes(info.canvas)) {
			if (try_scene(scene, info))
				return true;
		}
	}
	return false;
}

std::string canvas_key_of(QToolButton *button)
{
	return button->property("canvasKey").toByteArray().toStdString();
}

OBSSceneItem item_for(QToolButton *button)
{
	const QByteArray scene_uuid = button->property("sceneUuid").toByteArray();
	const int64_t id = button->property("itemId").toLongLong();
	OBSSourceAutoRelease scene_source = obs_get_source_by_uuid(scene_uuid.constData());
	obs_scene_t *scene = obs_group_or_scene_from_source(scene_source);
	return scene ? OBSSceneItem(obs_scene_find_sceneitem_by_id(scene, id)) : OBSSceneItem();
}

void insert_before_visibility(QWidget *row, QToolButton *button)
{
	auto *layout = qobject_cast<QHBoxLayout *>(row->layout());
	if (!layout) {
		delete button;
		return;
	}

	int index = -1;
	for (int i = 0; i < layout->count(); i++) {
		QWidget *w = layout->itemAt(i)->widget();
		if (w && w->property("class").toString().contains(QStringLiteral("indicator-visibility"))) {
			index = i;
			break;
		}
	}
	if (index < 0)
		index = std::max(0, layout->count() - 2);
	layout->insertWidget(index, button);
}

} // namespace

SourceListButtons::SourceListButtons(Controller &controller, QMainWindow *main_window, std::function<void()> show_dock,
				     QObject *parent)
	: QObject(parent),
	  controller_(controller),
	  main_window_(main_window),
	  show_dock_(std::move(show_dock))
{
	connect(&timer_, &QTimer::timeout, this, &SourceListButtons::sync);
	connect(&controller_, &Controller::routesChanged, this, &SourceListButtons::sync);
	connect(&controller_, &Controller::currentSceneChanged, this, [this]() {
		// OBS rebuilds its rows after this event; attach once they exist.
		QTimer::singleShot(0, this, &SourceListButtons::sync);
	});
	timer_.start(kSyncMs);
}

SourceListButtons::~SourceListButtons()
{
	remove_all();
}

void SourceListButtons::find_lists()
{
	if (!main_window_)
		return;
	if (!main_list_)
		main_list_ = main_window_->findChild<QAbstractItemView *>(QStringLiteral("sources"));

	// Other canvases' docks (Aitum Vertical's "Vertical Sources") can appear at any time, so
	// look for them every few ticks. They use OBS's list class but not its object name.
	if (ticks_++ % kRescanTicks != 0)
		return;
	canvas_lists_.erase(std::remove_if(canvas_lists_.begin(), canvas_lists_.end(),
					   [](const QPointer<QAbstractItemView> &list) { return list.isNull(); }),
			    canvas_lists_.end());
	for (QAbstractItemView *view : main_window_->findChildren<QAbstractItemView *>()) {
		if (view == main_list_ || qstrcmp(view->metaObject()->className(), "SourceTree") != 0)
			continue;
		if (std::find(canvas_lists_.begin(), canvas_lists_.end(), view) == canvas_lists_.end())
			canvas_lists_.emplace_back(view);
	}
}

void SourceListButtons::remove_all()
{
	std::vector<QAbstractItemView *> lists;
	if (main_list_)
		lists.push_back(main_list_);
	for (const auto &list : canvas_lists_) {
		if (list)
			lists.push_back(list);
	}
	for (QAbstractItemView *list : lists) {
		for (QToolButton *button : list->findChildren<QToolButton *>(QString::fromLatin1(kButtonName)))
			button->deleteLater();
	}
}

void SourceListButtons::sync()
{
	if (controller_.shutting_down()) {
		timer_.stop();
		remove_all();
		return;
	}
	if (!controller_.settings().source_list_buttons) {
		remove_all();
		return;
	}
	find_lists();

	if (main_list_ && main_list_->model())
		sync_list(main_list_, kMainCanvasKey, main_list_items());

	for (const auto &list : canvas_lists_) {
		if (!list || !list->model())
			continue;
		std::string key;
		std::vector<OBSSceneItem> items;
		if (match_canvas_list(list, key, items))
			sync_list(list, key, items);
	}
}

void SourceListButtons::sync_list(QAbstractItemView *list, const std::string &canvas_key,
				  const std::vector<OBSSceneItem> &items)
{
	QAbstractItemModel *model = list->model();
	if ((int)items.size() != model->rowCount())
		return; // The list is mid-update; try again on the next tick.

	for (int row = 0; row < model->rowCount(); row++) {
		QWidget *widget = list->indexWidget(model->index(row, 0));
		if (!widget)
			continue;

		obs_sceneitem_t *item = items[(size_t)row];
		obs_source_t *source = obs_sceneitem_get_source(item);
		if (!row_shows(widget, obs_source_get_name(source)))
			continue;

		auto *button = widget->findChild<QToolButton *>(QString::fromLatin1(kButtonName));
		if (!button) {
			button = new QToolButton(widget);
			button->setObjectName(QString::fromLatin1(kButtonName));
			button->setAutoRaise(true);
			button->setFocusPolicy(Qt::NoFocus);
			button->setIconSize(QSize(16, 16));
			button->setFixedSize(24, 22);
			button->setCursor(Qt::PointingHandCursor);
			button->setContextMenuPolicy(Qt::CustomContextMenu);
			button->setStyleSheet(QStringLiteral("QToolButton { border: none; background: transparent; }"));
			connect(button, &QToolButton::clicked, this, [this, button]() {
				OBSSceneItem target = item_for(button);
				if (target)
					cycle_placement_route(controller_, canvas_key_of(button), target);
			});
			connect(button, &QWidget::customContextMenuRequested, this,
				[this, button](const QPoint &pos) { show_menu(button, button->mapToGlobal(pos)); });
			insert_before_visibility(widget, button);
		}

		obs_scene_t *parent = obs_sceneitem_get_scene(item);
		button->setProperty("sceneUuid", QByteArray(obs_source_get_uuid(obs_scene_get_source(parent))));
		button->setProperty("itemId", (qlonglong)obs_sceneitem_get_id(item));
		button->setProperty("canvasKey", QByteArray::fromStdString(canvas_key));

		// Like Streamlabs, a locked source's button can't be changed.
		const PlacementRoute route = placement_route(controller_, canvas_key, item);
		button->setIcon(route_icon(route.route));
		button->setEnabled(!route.locked);
		button->setToolTip(QString::fromUtf8(obs_module_text("PluginName")) + QStringLiteral(": ") +
				   route_tooltip(route));
	}
}

void SourceListButtons::show_menu(QToolButton *button, const QPoint &pos)
{
	OBSSceneItem item = item_for(button);
	if (!item)
		return;
	obs_source_t *source = obs_sceneitem_get_source(item);
	const std::string canvas_key = canvas_key_of(button);

	const Route current = placement_route(controller_, canvas_key, item).route;

	QMenu menu;
	menu.addSection(QString::fromUtf8(obs_source_get_name(source)));
	for (Route r : {Route::Both, Route::StreamOnly, Route::RecordingOnly}) {
		QAction *action = menu.addAction(route_icon(r), route_label(r), this, [this, item, r, canvas_key]() {
			set_placement_route(controller_, canvas_key, item, r);
		});
		action->setCheckable(true);
		action->setChecked(r == current);
	}
	menu.addSeparator();
	menu.addAction(QString::fromUtf8(obs_module_text("Simple.OpenDock")), this, [this]() {
		if (show_dock_)
			show_dock_();
	});
	menu.addAction(QString::fromUtf8(obs_module_text("Simple.HideButtons")), this, [this]() {
		controller_.settings().source_list_buttons = false;
		controller_.settings_changed();
		sync();
	});
	menu.exec(pos);
}

} // namespace selective_recording
