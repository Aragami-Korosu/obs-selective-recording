// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>

class QMainWindow;
class QPushButton;

namespace selective_recording {

class Controller;

// Makes OBS's Record button record with Selective Recording, as the Record button does in
// Streamlabs (ADR-0009).
//
// OBS has no API for this, so the button's clicks are caught before OBS sees them: OBS's own
// recording is never started, and its output is never touched. The button then shows the
// selective recordings' state. While OBS's own recording is running (started by a hotkey or
// automation), clicks pass through so it can be stopped as usual. If OBS's layout changes and
// the button can't be found, the Record button simply keeps OBS's normal behaviour.
class RecordButtonTakeover : public QObject {
	Q_OBJECT

public:
	RecordButtonTakeover(Controller &controller, QMainWindow *main_window, QObject *parent = nullptr);
	~RecordButtonTakeover() override;

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;

private:
	bool taking_over() const;
	void toggle();
	void sync();
	void restore();

	Controller &controller_;
	QPointer<QMainWindow> main_window_;
	QPointer<QPushButton> button_;
	QTimer timer_;
	bool showing_ = false;
	bool pressed_ = false;
};

} // namespace selective_recording
