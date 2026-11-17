// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <QTimer>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QPushButton;
class QTabWidget;
class QToolButton;

namespace selective_recording {

class CanvasTab;
class Controller;

// The Selective Recording dock.
class Dock : public QWidget {
	Q_OBJECT

public:
	explicit Dock(Controller &controller, QWidget *parent = nullptr);

private:
	void rebuild_tabs();
	void refresh_profiles();
	void refresh_recovery();
	void refresh_all_button();
	void show_profile_menu();
	CanvasTab *tab(const QString &key) const;

	Controller &controller_;
	QCheckBox *enabled_ = nullptr;
	QComboBox *profiles_ = nullptr;
	QToolButton *profile_menu_ = nullptr;
	QFrame *recovery_ = nullptr;
	QLabel *recovery_text_ = nullptr;
	QTabWidget *tabs_ = nullptr;
	QLabel *empty_ = nullptr;
	QPushButton *all_ = nullptr;
	QTimer structure_timer_;
	bool updating_profiles_ = false;
};

} // namespace selective_recording
