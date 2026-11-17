// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "ui/settings-dialog.hpp"

#include "core/controller.hpp"
#include "core/obs-utils.hpp"
#include "core/pipeline.hpp"
#include "recording/encoders.hpp"

#include <obs-module.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QVBoxLayout>

namespace selective_recording {

namespace {

QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

void select_data(QComboBox *combo, const QVariant &value)
{
	const int index = combo->findData(value);
	combo->setCurrentIndex(index >= 0 ? index : 0);
}

} // namespace

SettingsDialog::SettingsDialog(Controller &controller, const std::string &focus_canvas, QWidget *parent)
	: QDialog(parent),
	  controller_(controller)
{
	setWindowTitle(text("Settings.Title"));
	setMinimumWidth(520);

	tabs_ = new QTabWidget(this);
	tabs_->addTab(build_general(), text("Settings.General"));

	for (const CanvasInfo &info : list_canvases()) {
		CanvasPage page;
		page.key = info.key;
		const QString title = info.is_main ? text("Canvas.Main") : QString::fromStdString(info.name);
		QWidget *widget = build_canvas(page, info.width, info.height);
		const int index = tabs_->addTab(widget, title);
		if (info.key == focus_canvas)
			tabs_->setCurrentIndex(index);
		pages_.push_back(page);
	}

	auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
	connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
		apply();
		accept();
	});
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	auto *layout = new QVBoxLayout(this);
	layout->addWidget(tabs_);
	layout->addWidget(buttons);
}

QWidget *SettingsDialog::build_general()
{
	const Settings &s = controller_.settings();
	auto *widget = new QWidget(this);
	auto *form = new QFormLayout(widget);

	container_ = new QComboBox(widget);
	container_->addItem(text("Container.MKV"), "mkv");
	container_->addItem(text("Container.MP4"), "mp4");
	container_->addItem(text("Container.MOV"), "mov");
	select_data(container_, QString::fromStdString(s.recording.container));
	form->addRow(text("Settings.Container"), container_);

	audio_encoder_ = new QComboBox(widget);
	audio_encoder_->addItem(QStringLiteral("AAC"), "ffmpeg_aac");
	audio_encoder_->addItem(QStringLiteral("Opus"), "ffmpeg_opus");
	select_data(audio_encoder_, QString::fromStdString(s.recording.audio_encoder));
	form->addRow(text("Settings.AudioEncoder"), audio_encoder_);

	audio_bitrate_ = new QSpinBox(widget);
	audio_bitrate_->setRange(64, 512);
	audio_bitrate_->setSingleStep(32);
	audio_bitrate_->setSuffix(QStringLiteral(" kbps"));
	audio_bitrate_->setValue(s.recording.audio_bitrate);
	form->addRow(text("Settings.AudioBitrate"), audio_bitrate_);

	filename_ = new QLineEdit(QString::fromStdString(s.recording.filename_format), widget);
	filename_->setToolTip(text("Settings.FilenameHelp"));
	form->addRow(text("Settings.Filename"), filename_);

	fallback_ = new QCheckBox(text("Settings.Fallback"), widget);
	fallback_->setChecked(s.recording.fallback_to_software);
	form->addRow(QString(), fallback_);

	warn_video_ = new QCheckBox(text("Settings.WarnVideo"), widget);
	warn_video_->setChecked(s.warnings.live_visibility);
	form->addRow(text("Settings.Warnings"), warn_video_);

	warn_audio_ = new QCheckBox(text("Settings.WarnAudio"), widget);
	warn_audio_->setChecked(s.warnings.audio_tracks);
	form->addRow(QString(), warn_audio_);

	recovery_ = new QCheckBox(text("Settings.Recovery"), widget);
	recovery_->setChecked(s.offer_recovery);
	form->addRow(QString(), recovery_);

	source_buttons_ = new QCheckBox(text("Settings.SourceButtons"), widget);
	source_buttons_->setChecked(s.source_list_buttons);
	source_buttons_->setToolTip(text("Settings.SourceButtonsHelp"));
	form->addRow(text("Settings.Interface"), source_buttons_);

	if (s.recording.container == "mp4") {
		auto *note = new QLabel(text("Settings.MP4Note"), widget);
		note->setWordWrap(true);
		form->addRow(QString(), note);
	}
	return widget;
}

QWidget *SettingsDialog::build_canvas(CanvasPage &page, uint32_t base_width, uint32_t base_height)
{
	const CanvasSettings &cs = controller_.settings().canvas(page.key);
	auto *widget = new QWidget(this);
	auto *form = new QFormLayout(widget);

	page.enabled = new QCheckBox(text("Settings.CanvasEnabled"), widget);
	page.enabled->setChecked(cs.enabled);
	form->addRow(QString(), page.enabled);

	page.resolution = new QComboBox(widget);
	page.resolution->addItem(text("Settings.CanvasSize").arg(base_width).arg(base_height), 0);
	page.resolution->addItem(text("Settings.CustomSize"), 1);
	page.resolution->setCurrentIndex(cs.width && cs.height ? 1 : 0);
	form->addRow(text("Settings.Resolution"), page.resolution);

	auto *size_row = new QWidget(widget);
	auto *size_layout = new QHBoxLayout(size_row);
	size_layout->setContentsMargins(0, 0, 0, 0);
	page.width = new QSpinBox(size_row);
	page.height = new QSpinBox(size_row);
	for (QSpinBox *box : {page.width, page.height}) {
		box->setRange(16, 8192);
		box->setSingleStep(2);
	}
	page.width->setValue(cs.width ? (int)cs.width : (int)base_width);
	page.height->setValue(cs.height ? (int)cs.height : (int)base_height);
	size_layout->addWidget(page.width);
	size_layout->addWidget(new QLabel(QStringLiteral("×"), size_row));
	size_layout->addWidget(page.height);
	form->addRow(QString(), size_row);

	auto sync_size = [page]() {
		const bool custom = page.resolution->currentData().toInt() == 1;
		page.width->setEnabled(custom);
		page.height->setEnabled(custom);
	};
	connect(page.resolution, &QComboBox::currentIndexChanged, widget, sync_size);
	sync_size();

	page.fps = new QComboBox(widget);
	page.fps->addItem(text("Settings.FpsFull"), 1);
	page.fps->addItem(text("Settings.FpsHalf"), 2);
	page.fps->addItem(text("Settings.FpsThird"), 3);
	page.fps->addItem(text("Settings.FpsQuarter"), 4);
	select_data(page.fps, (int)cs.fps_divisor);
	form->addRow(text("Settings.FrameRate"), page.fps);

	page.encoder = new QComboBox(widget);
	page.encoder->addItem(text("Settings.EncoderAuto"), QStringLiteral("auto"));
	for (const EncoderOption &option : video_encoders())
		page.encoder->addItem(QString::fromStdString(option.name), QString::fromStdString(option.id));
	select_data(page.encoder, QString::fromStdString(cs.video_encoder));
	form->addRow(text("Settings.Encoder"), page.encoder);

	page.rate_control = new QComboBox(widget);
	page.rate_control->addItem(text("Settings.RateQuality"), 0);
	page.rate_control->addItem(text("Settings.RateBitrate"), 1);
	page.rate_control->setCurrentIndex(cs.rate_control == RateControl::Bitrate ? 1 : 0);
	form->addRow(text("Settings.RateControl"), page.rate_control);

	page.quality = new QSpinBox(widget);
	page.quality->setRange(1, 51);
	page.quality->setValue(cs.quality);
	page.quality->setToolTip(text("Settings.QualityHelp"));
	form->addRow(text("Settings.Quality"), page.quality);

	page.bitrate = new QSpinBox(widget);
	page.bitrate->setRange(500, 200000);
	page.bitrate->setSingleStep(500);
	page.bitrate->setSuffix(QStringLiteral(" kbps"));
	page.bitrate->setValue(cs.bitrate);
	form->addRow(text("Settings.Bitrate"), page.bitrate);

	auto sync_rate = [page]() {
		const bool bitrate = page.rate_control->currentData().toInt() == 1;
		page.quality->setEnabled(!bitrate);
		page.bitrate->setEnabled(bitrate);
	};
	connect(page.rate_control, &QComboBox::currentIndexChanged, widget, sync_rate);
	sync_rate();

	auto *dir_row = new QWidget(widget);
	auto *dir_layout = new QHBoxLayout(dir_row);
	dir_layout->setContentsMargins(0, 0, 0, 0);
	page.directory = new QLineEdit(QString::fromStdString(cs.output_directory), dir_row);
	page.directory->setPlaceholderText(text("Settings.DirectoryDefault"));
	auto *browse = new QPushButton(text("Settings.Browse"), dir_row);
	dir_layout->addWidget(page.directory);
	dir_layout->addWidget(browse);
	QLineEdit *directory = page.directory;
	connect(browse, &QPushButton::clicked, widget, [this, directory]() {
		const QString dir =
			QFileDialog::getExistingDirectory(this, text("Settings.Directory"), directory->text());
		if (!dir.isEmpty())
			directory->setText(dir);
	});
	form->addRow(text("Settings.Directory"), dir_row);

	page.suffix = new QLineEdit(QString::fromStdString(cs.filename_suffix), widget);
	page.suffix->setPlaceholderText(page.key == kMainCanvasKey ? QString() : text("Settings.SuffixDefault"));
	form->addRow(text("Settings.Suffix"), page.suffix);

	Pipeline *pipeline = controller_.pipeline(page.key);
	if (pipeline && pipeline->busy()) {
		auto *note = new QLabel(text("Settings.AppliesNext"), widget);
		note->setWordWrap(true);
		form->addRow(QString(), note);
	}
	return widget;
}

void SettingsDialog::apply()
{
	Settings &s = controller_.settings();
	s.recording.container = container_->currentData().toString().toStdString();
	s.recording.audio_encoder = audio_encoder_->currentData().toString().toStdString();
	s.recording.audio_bitrate = audio_bitrate_->value();
	if (!filename_->text().trimmed().isEmpty())
		s.recording.filename_format = filename_->text().trimmed().toStdString();
	s.recording.fallback_to_software = fallback_->isChecked();
	s.warnings.live_visibility = warn_video_->isChecked();
	s.warnings.audio_tracks = warn_audio_->isChecked();
	s.offer_recovery = recovery_->isChecked();
	s.source_list_buttons = source_buttons_->isChecked();

	for (const CanvasPage &page : pages_) {
		CanvasSettings &cs = s.canvas(page.key);
		cs.enabled = page.enabled->isChecked();
		const bool custom = page.resolution->currentData().toInt() == 1;
		cs.width = custom ? (uint32_t)page.width->value() & ~1u : 0;
		cs.height = custom ? (uint32_t)page.height->value() & ~1u : 0;
		cs.fps_divisor = (uint32_t)page.fps->currentData().toInt();
		cs.video_encoder = page.encoder->currentData().toString().toStdString();
		cs.rate_control = page.rate_control->currentData().toInt() == 1 ? RateControl::Bitrate
										: RateControl::Quality;
		cs.quality = page.quality->value();
		cs.bitrate = page.bitrate->value();
		cs.output_directory = page.directory->text().trimmed().toStdString();
		cs.filename_suffix = page.suffix->text().toStdString();
	}
	controller_.settings_changed();
}

} // namespace selective_recording
