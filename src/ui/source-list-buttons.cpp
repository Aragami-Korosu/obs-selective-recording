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

std::vector<OBSSceneItem> listed_items()
{
	OBSSourceAutoRelease scene_source = obs_frontend_preview_program_mode_active()
						    ? obs_frontend_get_current_preview_scene()
						    : obs_frontend_get_current_scene();
	std::vector<OBSSceneItem> items;
	obs_scene_t *scene = obs_scene_from_source(scene_source);
	if (scene)
		obs_scene_enum_items(scene, enum_item, &items);
	return items;
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

bool SourceListButtons::find_list()
{
	if (list_)
		return true;
	if (!main_window_)
		return false;

	list_ = main_window_->findChild<QAbstractItemView *>(QStringLiteral("sources"));
	if (!list_) {
		for (QAbstractItemView *view : main_window_->findChildren<QAbstractItemView *>()) {
			if (qstrcmp(view->metaObject()->className(), "SourceTree") == 0) {
				list_ = view;
				break;
			}
		}
	}
	return list_ != nullptr;
}

void SourceListButtons::remove_all()
{
	if (!list_)
		return;
	for (QToolButton *button : list_->findChildren<QToolButton *>(QString::fromLatin1(kButtonName)))
		button->deleteLater();
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
	if (!find_list() || !list_->model())
		return;

	QAbstractItemModel *model = list_->model();
	const std::vector<OBSSceneItem> items = listed_items();
	if ((int)items.size() != model->rowCount())
		return; // OBS is mid-update; try again on the next tick.

	for (int row = 0; row < model->rowCount(); row++) {
		QWidget *widget = list_->indexWidget(model->index(row, 0));
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
					cycle_combined_route(controller_, kMainCanvasKey,
							     obs_sceneitem_get_source(target), target);
			});
			connect(button, &QWidget::customContextMenuRequested, this,
				[this, button](const QPoint &pos) { show_menu(button, button->mapToGlobal(pos)); });
			insert_before_visibility(widget, button);
		}

		obs_scene_t *parent = obs_sceneitem_get_scene(item);
		button->setProperty("sceneUuid", QByteArray(obs_source_get_uuid(obs_scene_get_source(parent))));
		button->setProperty("itemId", (qlonglong)obs_sceneitem_get_id(item));

		const CombinedRoute route = combined_route(controller_, kMainCanvasKey, source, item);
		button->setIcon(route_icon(route.route, route.mixed));
		button->setToolTip(QString::fromUtf8(obs_module_text("PluginName")) + QStringLiteral(": ") +
				   combined_label(route) + QStringLiteral("\n") + combined_tooltip(route));
	}
}

void SourceListButtons::show_menu(QToolButton *button, const QPoint &pos)
{
	OBSSceneItem item = item_for(button);
	if (!item)
		return;
	obs_source_t *source = obs_sceneitem_get_source(item);

	QMenu menu;
	menu.addSection(QString::fromUtf8(obs_source_get_name(source)));
	for (Route r : {Route::Both, Route::StreamOnly, Route::RecordingOnly, Route::Off}) {
		CombinedRoute shown;
		shown.route = r;
		menu.addAction(route_icon(r, false), combined_label(shown), this, [this, item, r]() {
			set_combined_route(controller_, kMainCanvasKey, obs_sceneitem_get_source(item), item, r);
		});
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
