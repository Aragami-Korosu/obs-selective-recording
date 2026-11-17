// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "plugin/plugin-main.hpp"

#include "api/websocket-api.hpp"
#include "core/controller.hpp"
#include "ui/dock.hpp"
#include "ui/source-list-buttons.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>

#include <QDockWidget>
#include <QMainWindow>
#include <QPointer>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

using namespace selective_recording;

namespace {

constexpr const char *kDockId = "selective-recording-dock";
QPointer<Dock> g_dock;
bool g_frontend_gone = false;

void show_dock()
{
	if (!g_dock)
		return;
	if (auto *dock_widget = qobject_cast<QDockWidget *>(g_dock->parentWidget())) {
		dock_widget->setVisible(true);
		dock_widget->raise();
		dock_widget->activateWindow();
	}
}

// OBS hides new plugin docks; show ours once so new users can find it.
void introduce_dock()
{
	Controller *controller = Controller::instance();
	if (!controller || controller->settings().dock_introduced || !g_dock)
		return;

	if (auto *dock_widget = qobject_cast<QDockWidget *>(g_dock->parentWidget())) {
		dock_widget->setVisible(true);
		if (dock_widget->isFloating())
			dock_widget->resize(g_dock->sizeHint());
		dock_widget->raise();
	}
	controller->settings().dock_introduced = true;
	controller->settings().save();
}

void on_frontend_event(enum obs_frontend_event event, void *)
{
	if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		introduce_dock();
	} else if (event == OBS_FRONTEND_EVENT_EXIT) {
		websocket::unregister_vendor();
		g_frontend_gone = true;
	}
}

} // namespace

MODULE_EXPORT const char *obs_module_name(void)
{
	return obs_module_text("PluginName");
}

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("PluginDescription");
}

bool obs_module_load(void)
{
	obs_log(LOG_INFO, "loaded (version %s)", PLUGIN_VERSION);
	return true;
}

void obs_module_post_load(void)
{
	// The controller must exist before OBS loads the scene collection, which happens next.
	Controller::create();
	Controller *controller = Controller::instance();

	auto *main_window = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	auto *dock = new Dock(*controller, main_window);
	if (obs_frontend_add_dock_by_id(kDockId, obs_module_text("PluginName"), dock))
		g_dock = dock;
	else
		obs_log(LOG_WARNING, "could not add the dock");

	// Streamlabs-style buttons in OBS's own Sources list; owned by the dock.
	new SourceListButtons(*controller, main_window, show_dock, dock);

	QObject::connect(controller, &Controller::pipelineStateChanged, controller,
			 [](const QString &) { websocket::emit_state_changed(); });

	websocket::register_vendor();
	obs_frontend_add_event_callback(on_frontend_event, nullptr);
}

void obs_module_unload(void)
{
	if (!g_frontend_gone)
		obs_frontend_remove_event_callback(on_frontend_event, nullptr);
	websocket::unregister_vendor();
	Controller::destroy();
	obs_log(LOG_INFO, "unloaded");
}
