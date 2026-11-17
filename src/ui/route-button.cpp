// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "ui/route-button.hpp"

#include <obs-module.h>

namespace selective_recording {

namespace {

const char *color_for(Route route)
{
	switch (route) {
	case Route::Both:
		return "#3fa66b";
	case Route::StreamOnly:
		return "#4d8fd6";
	case Route::RecordingOnly:
		return "#c9863a";
	case Route::Off:
		return "#7a7a7a";
	}
	return "#7a7a7a";
}

} // namespace

RouteButton::RouteButton(Kind kind, QWidget *parent) : QToolButton(parent), kind_(kind)
{
	setAutoRaise(false);
	setCursor(Qt::PointingHandCursor);
	setContextMenuPolicy(Qt::CustomContextMenu);
	setMinimumWidth(78);
	setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

	connect(this, &QToolButton::clicked, this, &RouteButton::cycleRequested);
	connect(this, &QWidget::customContextMenuRequested, this,
		[this](const QPoint &pos) { emit menuRequested(mapToGlobal(pos)); });
}

QString RouteButton::label(Route route)
{
	switch (route) {
	case Route::Both:
		return obs_module_text("Route.Both");
	case Route::StreamOnly:
		return obs_module_text("Route.StreamOnly");
	case Route::RecordingOnly:
		return obs_module_text("Route.RecordingOnly");
	case Route::Off:
		return obs_module_text("Route.Off");
	}
	return {};
}

QString RouteButton::description(Route route, Kind kind)
{
	const bool video = kind == Kind::Video;
	switch (route) {
	case Route::Both:
		return obs_module_text(video ? "RouteHelp.Video.Both" : "RouteHelp.Audio.Both");
	case Route::StreamOnly:
		return obs_module_text(video ? "RouteHelp.Video.StreamOnly" : "RouteHelp.Audio.StreamOnly");
	case Route::RecordingOnly:
		return obs_module_text(video ? "RouteHelp.Video.RecordingOnly" : "RouteHelp.Audio.RecordingOnly");
	case Route::Off:
		return obs_module_text(video ? "RouteHelp.Video.Off" : "RouteHelp.Audio.Off");
	}
	return {};
}

void RouteButton::set_route(const ResolvedRoute &route)
{
	route_ = route;

	QString text = label(route.route);
	const char *origin_key = "RouteOrigin.Default";
	switch (route.origin) {
	case RouteOrigin::Item:
		text += QStringLiteral(" •");
		origin_key = "RouteOrigin.Item";
		break;
	case RouteOrigin::Canvas:
		text += QStringLiteral(" •");
		origin_key = "RouteOrigin.Canvas";
		break;
	case RouteOrigin::Source:
		origin_key = "RouteOrigin.Source";
		break;
	case RouteOrigin::ProfileDefault:
		break;
	}
	setText(text);

	const bool inherited = route.origin == RouteOrigin::ProfileDefault;
	setStyleSheet(QStringLiteral("QToolButton { border: 1px solid %1; border-radius: 3px; padding: 1px 6px; "
				     "color: %1; font-style: %2; }")
			      .arg(QString::fromLatin1(color_for(route.route)),
				   inherited ? QStringLiteral("italic") : QStringLiteral("normal")));

	setToolTip(description(route.route, kind_) + QStringLiteral("\n\n") +
		   QString::fromUtf8(obs_module_text(origin_key)) + QStringLiteral("\n") +
		   QString::fromUtf8(obs_module_text("RouteHelp.Click")));
}

} // namespace selective_recording
