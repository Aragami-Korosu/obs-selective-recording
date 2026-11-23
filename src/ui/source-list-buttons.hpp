// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <obs.hpp>

#include <functional>
#include <string>
#include <vector>

class QAbstractItemView;
class QMainWindow;
class QToolButton;

namespace selective_recording {

class Controller;

// Adds a Streamlabs-style recording button to each row of OBS's own Sources list, and to
// the Sources lists of extra canvases such as Aitum Vertical's "Vertical Sources" dock.
//
// OBS has no public API for this, so the button is attached to the list's existing row
// widgets. Each row is matched to its scene item by order and checked against the source
// name; anything unexpected is skipped, so a layout change can only hide the buttons,
// never put them on the wrong source. Only the plugin's routes change; OBS state never does.
class SourceListButtons : public QObject {
	Q_OBJECT

public:
	SourceListButtons(Controller &controller, QMainWindow *main_window, std::function<void()> show_dock,
			  QObject *parent = nullptr);
	~SourceListButtons() override;

	void sync();

private:
	void find_lists();
	void remove_all();
	void sync_list(QAbstractItemView *list, const std::string &canvas_key, const std::vector<OBSSceneItem> &items);
	void show_menu(QToolButton *button, const QPoint &pos);

	Controller &controller_;
	QPointer<QMainWindow> main_window_;
	QPointer<QAbstractItemView> main_list_;
	std::vector<QPointer<QAbstractItemView>> canvas_lists_;
	std::function<void()> show_dock_;
	QTimer timer_;
	int ticks_ = 0;
};

} // namespace selective_recording
