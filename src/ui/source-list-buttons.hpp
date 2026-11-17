// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

#include <functional>

class QAbstractItemView;
class QMainWindow;
class QToolButton;

namespace selective_recording {

class Controller;

// Adds a Streamlabs-style recording button to each row of OBS's own Sources list.
//
// OBS has no public API for this, so the button is attached to the list's existing row
// widgets. Each row is matched to its scene item by order and checked against the source
// name; anything unexpected is skipped, so an OBS layout change can only hide the buttons,
// never put them on the wrong source. Only the plugin's routes change; OBS state never does.
class SourceListButtons : public QObject {
	Q_OBJECT

public:
	SourceListButtons(Controller &controller, QMainWindow *main_window, std::function<void()> show_dock,
			  QObject *parent = nullptr);
	~SourceListButtons() override;

	void sync();

private:
	bool find_list();
	void remove_all();
	void show_menu(QToolButton *button, const QPoint &pos);

	Controller &controller_;
	QPointer<QMainWindow> main_window_;
	QPointer<QAbstractItemView> list_;
	std::function<void()> show_dock_;
	QTimer timer_;
};

} // namespace selective_recording
