// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <QDialog>

#include <string>
#include <vector>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QTabWidget;

namespace selective_recording {

class Controller;

// Global recording preferences plus one tab of settings per canvas.
class SettingsDialog : public QDialog {
	Q_OBJECT

public:
	SettingsDialog(Controller &controller, const std::string &focus_canvas, QWidget *parent = nullptr);

private:
	struct CanvasPage {
		std::string key;
		QCheckBox *enabled = nullptr;
		QComboBox *resolution = nullptr;
		QSpinBox *width = nullptr;
		QSpinBox *height = nullptr;
		QComboBox *fps = nullptr;
		QComboBox *encoder = nullptr;
		QComboBox *rate_control = nullptr;
		QSpinBox *quality = nullptr;
		QSpinBox *bitrate = nullptr;
		QLineEdit *directory = nullptr;
		QLineEdit *suffix = nullptr;
	};

	QWidget *build_general();
	QWidget *build_canvas(CanvasPage &page, uint32_t base_width, uint32_t base_height);
	void apply();

	Controller &controller_;
	QTabWidget *tabs_ = nullptr;
	QComboBox *container_ = nullptr;
	QComboBox *audio_encoder_ = nullptr;
	QSpinBox *audio_bitrate_ = nullptr;
	QLineEdit *filename_ = nullptr;
	QCheckBox *fallback_ = nullptr;
	QCheckBox *warn_video_ = nullptr;
	QCheckBox *warn_audio_ = nullptr;
	QCheckBox *recovery_ = nullptr;
	QCheckBox *source_buttons_ = nullptr;
	QCheckBox *folder_buttons_ = nullptr;
	QCheckBox *record_button_ = nullptr;
	std::vector<CanvasPage> pages_;
};

} // namespace selective_recording
