// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "ui/dock.hpp"

#include "core/controller.hpp"
#include "core/pipeline.hpp"
#include "ui/canvas-tab.hpp"

#include <obs-module.h>
#include <obs.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QToolButton>
#include <QUrl>
#include <QVBoxLayout>

namespace selective_recording {

namespace {

QString text(const char *key)
{
	return QString::fromUtf8(obs_module_text(key));
}

} // namespace

Dock::Dock(Controller &controller, QWidget *parent) : QWidget(parent), controller_(controller)
{
	auto *layout = new QVBoxLayout(this);
	layout->setContentsMargins(4, 4, 4, 4);
	layout->setSpacing(4);

	auto *top = new QHBoxLayout();
	enabled_ = new QCheckBox(text("Dock.Enabled"), this);
	enabled_->setToolTip(text("Dock.EnabledHelp"));
	profiles_ = new QComboBox(this);
	profiles_->setToolTip(text("Dock.ProfileHelp"));
	profile_menu_ = new QToolButton(this);
	profile_menu_->setText(QStringLiteral("⋯"));
	profile_menu_->setToolTip(text("Dock.ProfileMenu"));
	top->addWidget(enabled_);
	top->addStretch(1);
	top->addWidget(new QLabel(text("Dock.Profile"), this));
	top->addWidget(profiles_, 1);
	top->addWidget(profile_menu_);
	layout->addLayout(top);

	recovery_ = new QFrame(this);
	recovery_->setFrameShape(QFrame::StyledPanel);
	recovery_->setStyleSheet(QStringLiteral("QFrame { border: 1px solid #d9a441; border-radius: 3px; }"));
	auto *recovery_layout = new QVBoxLayout(recovery_);
	recovery_text_ = new QLabel(recovery_);
	recovery_text_->setWordWrap(true);
	recovery_text_->setStyleSheet(QStringLiteral("border: none;"));
	auto *recovery_buttons = new QHBoxLayout();
	auto *show_files = new QPushButton(text("Recovery.Show"), recovery_);
	auto *dismiss = new QPushButton(text("Recovery.Dismiss"), recovery_);
	recovery_buttons->addStretch(1);
	recovery_buttons->addWidget(show_files);
	recovery_buttons->addWidget(dismiss);
	recovery_layout->addWidget(recovery_text_);
	recovery_layout->addLayout(recovery_buttons);
	recovery_->setVisible(false);
	layout->addWidget(recovery_);

	tabs_ = new QTabWidget(this);
	tabs_->setDocumentMode(true);
	layout->addWidget(tabs_, 1);

	empty_ = new QLabel(text("Dock.NoCanvas"), this);
	empty_->setAlignment(Qt::AlignCenter);
	empty_->setWordWrap(true);
	layout->addWidget(empty_);

	all_ = new QPushButton(this);
	layout->addWidget(all_);

	connect(enabled_, &QCheckBox::toggled, this, [this](bool checked) { controller_.set_enabled(checked); });
	connect(profiles_, &QComboBox::currentIndexChanged, this, [this](int index) {
		if (updating_profiles_ || index < 0)
			return;
		controller_.routes().set_active((size_t)index);
		controller_.routes_changed();
	});
	connect(profile_menu_, &QToolButton::clicked, this, &Dock::show_profile_menu);
	connect(all_, &QPushButton::clicked, this, [this]() {
		bool any = false;
		for (Pipeline *p : controller_.pipelines())
			any = any || p->recording();
		if (any)
			controller_.stop_all();
		else
			controller_.start_all();
		refresh_all_button();
	});

	connect(show_files, &QPushButton::clicked, this, [this]() {
		const auto &items = controller_.interrupted();
		if (items.empty())
			return;
		const QString path = QString::fromStdString(items.front().path);
		QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
	});
	connect(dismiss, &QPushButton::clicked, this, [this]() {
		const auto items = controller_.interrupted();
		for (const auto &rec : items)
			controller_.dismiss_interrupted(rec.id);
	});

	connect(&controller_, &Controller::canvasesChanged, this, &Dock::rebuild_tabs);
	connect(&controller_, &Controller::routesChanged, this, [this]() {
		refresh_profiles();
		for (int i = 0; i < tabs_->count(); i++)
			static_cast<CanvasTab *>(tabs_->widget(i))->refresh_routes();
	});
	connect(&controller_, &Controller::warningsChanged, this, [this]() {
		for (int i = 0; i < tabs_->count(); i++)
			static_cast<CanvasTab *>(tabs_->widget(i))->refresh_warnings();
	});
	connect(&controller_, &Controller::pipelineStateChanged, this, [this](const QString &key) {
		if (CanvasTab *t = tab(key))
			t->refresh_state();
		refresh_all_button();
	});
	connect(&controller_, &Controller::statsUpdated, this, [this]() {
		for (int i = 0; i < tabs_->count(); i++)
			static_cast<CanvasTab *>(tabs_->widget(i))->refresh_state();
	});
	connect(&controller_, &Controller::interruptedChanged, this, &Dock::refresh_recovery);

	// Scene edits change the tree; check the visible tab for structural changes once a second.
	connect(&structure_timer_, &QTimer::timeout, this, [this]() {
		if (!isVisible())
			return;
		if (auto *t = static_cast<CanvasTab *>(tabs_->currentWidget()))
			t->rebuild_if_changed();
	});
	structure_timer_.start(1000);
	connect(tabs_, &QTabWidget::currentChanged, this, [this](int) {
		if (auto *t = static_cast<CanvasTab *>(tabs_->currentWidget()))
			t->rebuild_if_changed();
	});

	rebuild_tabs();
	refresh_profiles();
	refresh_recovery();
}

CanvasTab *Dock::tab(const QString &key) const
{
	for (int i = 0; i < tabs_->count(); i++) {
		auto *t = static_cast<CanvasTab *>(tabs_->widget(i));
		if (QString::fromStdString(t->key()) == key)
			return t;
	}
	return nullptr;
}

void Dock::rebuild_tabs()
{
	const QString current =
		tabs_->currentWidget() ? QString::fromStdString(static_cast<CanvasTab *>(tabs_->currentWidget())->key())
				       : QString();

	// deleteLater: a tab may be running a dialog or menu when canvases change (for example on exit).
	while (tabs_->count() > 0) {
		QWidget *w = tabs_->widget(0);
		tabs_->removeTab(0);
		w->hide();
		w->deleteLater();
	}

	for (Pipeline *p : controller_.pipelines()) {
		auto *t = new CanvasTab(controller_, p->key(), tabs_);
		const int index = tabs_->addTab(t, QString::fromStdString(p->name()));
		if (QString::fromStdString(p->key()) == current)
			tabs_->setCurrentIndex(index);
	}

	empty_->setVisible(tabs_->count() == 0);
	tabs_->setVisible(tabs_->count() > 0);
	enabled_->blockSignals(true);
	enabled_->setChecked(controller_.settings().enabled);
	enabled_->blockSignals(false);
	refresh_all_button();
}

void Dock::refresh_all_button()
{
	const auto pipelines = controller_.pipelines();
	bool any = false;
	for (Pipeline *p : pipelines)
		any = any || p->recording();
	all_->setVisible(pipelines.size() > 1);
	all_->setText(any ? text("Dock.StopAll") : text("Dock.StartAll"));
}

void Dock::refresh_profiles()
{
	updating_profiles_ = true;
	profiles_->clear();
	const RouteStore &routes = controller_.routes();
	for (const auto &profile : routes.profiles())
		profiles_->addItem(QString::fromStdString(profile.name));
	profiles_->setCurrentIndex((int)routes.active_index());
	updating_profiles_ = false;

	enabled_->blockSignals(true);
	enabled_->setChecked(controller_.settings().enabled);
	enabled_->blockSignals(false);
}

void Dock::refresh_recovery()
{
	const auto &items = controller_.interrupted();
	recovery_->setVisible(!items.empty());
	if (items.empty())
		return;

	QStringList files;
	for (const auto &rec : items)
		files << QString::fromStdString(rec.path);
	recovery_text_->setText(text("Recovery.Message").arg(items.size()).arg(files.join(QStringLiteral("\n"))));
}

void Dock::show_profile_menu()
{
	RouteStore &routes = controller_.routes();
	QMenu menu(this);

	menu.addAction(text("Profile.New"), this, [this, &routes]() {
		bool ok = false;
		const QString name = QInputDialog::getText(this, text("Profile.New"), text("Profile.Name"),
							   QLineEdit::Normal, QString(), &ok);
		if (ok && !name.trimmed().isEmpty()) {
			routes.add_profile(name.trimmed().toStdString(), false);
			controller_.routes_changed();
		}
	});
	menu.addAction(text("Profile.Duplicate"), this, [this, &routes]() {
		bool ok = false;
		const QString name =
			QInputDialog::getText(this, text("Profile.Duplicate"), text("Profile.Name"), QLineEdit::Normal,
					      QString::fromStdString(routes.active().name) + QStringLiteral(" 2"), &ok);
		if (ok && !name.trimmed().isEmpty()) {
			routes.add_profile(name.trimmed().toStdString(), true);
			controller_.routes_changed();
		}
	});
	menu.addAction(text("Profile.Rename"), this, [this, &routes]() {
		bool ok = false;
		const QString name = QInputDialog::getText(this, text("Profile.Rename"), text("Profile.Name"),
							   QLineEdit::Normal,
							   QString::fromStdString(routes.active().name), &ok);
		if (ok && !name.trimmed().isEmpty()) {
			routes.rename_profile(routes.active_index(), name.trimmed().toStdString());
			controller_.routes_changed();
		}
	});
	QAction *remove = menu.addAction(text("Profile.Delete"), this, [this, &routes]() {
		if (QMessageBox::question(
			    this, text("Profile.Delete"),
			    text("Profile.DeleteConfirm").arg(QString::fromStdString(routes.active().name))) ==
		    QMessageBox::Yes) {
			routes.remove_profile(routes.active_index());
			controller_.routes_changed();
		}
	});
	remove->setEnabled(routes.profiles().size() > 1);

	menu.addSeparator();
	menu.addAction(text("Profile.Import"), this, [this, &routes]() {
		const QString path = QFileDialog::getOpenFileName(this, text("Profile.Import"), QString(),
								  QStringLiteral("JSON (*.json)"));
		if (path.isEmpty())
			return;
		OBSDataAutoRelease data = obs_data_create_from_json_file(path.toUtf8().constData());
		if (!data || !routes.import_profile(data)) {
			QMessageBox::warning(this, text("Profile.Import"), text("Profile.ImportFailed"));
			return;
		}
		controller_.routes_changed();
	});
	menu.addAction(text("Profile.Export"), this, [this, &routes]() {
		const QString path = QFileDialog::getSaveFileName(this, text("Profile.Export"),
								  QString::fromStdString(routes.active().name) +
									  QStringLiteral(".json"),
								  QStringLiteral("JSON (*.json)"));
		if (path.isEmpty())
			return;
		OBSDataAutoRelease data = routes.export_profile(routes.active_index());
		if (!data || !obs_data_save_json(data, path.toUtf8().constData()))
			QMessageBox::warning(this, text("Profile.Export"), text("Profile.ExportFailed"));
	});

	menu.exec(profile_menu_->mapToGlobal(QPoint(0, profile_menu_->height())));
}

} // namespace selective_recording
