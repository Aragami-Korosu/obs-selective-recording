// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "ui/record-button.hpp"

#include "core/controller.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QKeyEvent>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPushButton>

namespace selective_recording {

namespace {

constexpr int kSyncMs = 300;

QString frontend_text(const char *key)
{
	return QString::fromUtf8(obs_frontend_get_locale_string(key));
}

bool is_activation_key(int key)
{
	return key == Qt::Key_Space || key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Select;
}

} // namespace

RecordButtonTakeover::RecordButtonTakeover(Controller &controller, QMainWindow *main_window, QObject *parent)
	: QObject(parent),
	  controller_(controller),
	  main_window_(main_window)
{
	connect(&timer_, &QTimer::timeout, this, &RecordButtonTakeover::sync);
	connect(&controller_, &Controller::pipelineStateChanged, this, &RecordButtonTakeover::sync);
	timer_.start(kSyncMs);
	sync();
}

RecordButtonTakeover::~RecordButtonTakeover()
{
	if (button_)
		button_->removeEventFilter(this);
	restore();
}

bool RecordButtonTakeover::taking_over() const
{
	return controller_.settings().record_button && !controller_.shutting_down() && !obs_frontend_recording_active();
}

bool RecordButtonTakeover::eventFilter(QObject *watched, QEvent *event)
{
	if (watched != button_ || !taking_over() || !button_->isEnabled())
		return QObject::eventFilter(watched, event);

	switch (event->type()) {
	case QEvent::MouseButtonPress:
	case QEvent::MouseButtonDblClick:
		if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
			pressed_ = true;
			return true;
		}
		break;
	case QEvent::MouseButtonRelease:
		if (static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
			const bool inside =
				button_->rect().contains(static_cast<QMouseEvent *>(event)->position().toPoint());
			if (pressed_ && inside)
				toggle();
			pressed_ = false;
			return true;
		}
		break;
	case QEvent::KeyPress:
		if (is_activation_key(static_cast<QKeyEvent *>(event)->key()))
			return true;
		break;
	case QEvent::KeyRelease: {
		auto *key = static_cast<QKeyEvent *>(event);
		if (is_activation_key(key->key())) {
			if (!key->isAutoRepeat())
				toggle();
			return true;
		}
		break;
	}
	default:
		break;
	}
	return QObject::eventFilter(watched, event);
}

void RecordButtonTakeover::toggle()
{
	controller_.toggle_all();
	QTimer::singleShot(0, this, &RecordButtonTakeover::sync);
}

void RecordButtonTakeover::sync()
{
	if (controller_.shutting_down()) {
		timer_.stop();
		restore();
		return;
	}
	if (!button_ && main_window_) {
		button_ = main_window_->findChild<QPushButton *>(QStringLiteral("recordButton"));
		if (button_)
			button_->installEventFilter(this);
	}
	if (!button_)
		return;

	if (!taking_over()) {
		restore();
		return;
	}

	const bool recording = controller_.any_recording();
	const QString label = frontend_text(recording ? "Basic.Main.StopRecording" : "Basic.Main.StartRecording");
	if (button_->isChecked() != recording)
		button_->setChecked(recording);
	if (button_->text() != label)
		button_->setText(label);
	button_->setToolTip(QString::fromUtf8(obs_module_text("RecordButton.Tooltip")));
	showing_ = true;
}

void RecordButtonTakeover::restore()
{
	if (!showing_ || !button_)
		return;
	showing_ = false;
	button_->setToolTip(QString());
	// When OBS's own recording is running, OBS has already set the button for it.
	if (!obs_frontend_recording_active()) {
		button_->setChecked(false);
		button_->setText(frontend_text("Basic.Main.StartRecording"));
	}
}

} // namespace selective_recording
