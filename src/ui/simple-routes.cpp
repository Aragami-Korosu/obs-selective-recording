// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "ui/simple-routes.hpp"

#include "core/controller.hpp"
#include "core/obs-utils.hpp"

#include <obs-module.h>

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace selective_recording {

namespace {

const QColor kStream(77, 143, 214);
const QColor kRecord(224, 80, 80);
const QColor kOff(130, 130, 130);

bool is_container(obs_source_t *source)
{
	return obs_source_is_scene(source) || obs_source_is_group(source);
}

QPixmap paint_icon(Route route, bool mixed, int size)
{
	QPixmap pixmap(size, size);
	pixmap.fill(Qt::transparent);
	QPainter p(&pixmap);
	p.setRenderHint(QPainter::Antialiasing);

	const QRectF r(1.5, 1.5, size - 3.0, size - 3.0);
	switch (route) {
	case Route::StreamOnly:
		p.setPen(Qt::NoPen);
		p.setBrush(kStream);
		p.drawEllipse(r);
		break;
	case Route::RecordingOnly:
		p.setPen(Qt::NoPen);
		p.setBrush(kRecord);
		p.drawEllipse(r);
		break;
	case Route::Both: {
		// Half stream, half recording.
		QPainterPath left;
		left.moveTo(r.center());
		left.arcTo(r, 90, 180);
		QPainterPath right;
		right.moveTo(r.center());
		right.arcTo(r, -90, 180);
		p.setPen(Qt::NoPen);
		p.setBrush(kStream);
		p.drawPath(left);
		p.setBrush(kRecord);
		p.drawPath(right);
		break;
	}
	case Route::Off:
		p.setPen(QPen(kOff, 1.6));
		p.setBrush(Qt::NoBrush);
		p.drawEllipse(r.adjusted(1, 1, -1, -1));
		break;
	}

	if (mixed) {
		// A small white dot marks "video and audio differ".
		p.setPen(Qt::NoPen);
		p.setBrush(Qt::white);
		const double d = size * 0.32;
		p.drawEllipse(QRectF(r.center().x() - d / 2, r.center().y() - d / 2, d, d));
	}
	return pixmap;
}

} // namespace

CombinedRoute combined_route(Controller &controller, const std::string &canvas, obs_source_t *source,
			     obs_sceneitem_t *item)
{
	CombinedRoute result;
	const RouteStore &routes = controller.routes();
	result.has_video = has_video(source) || is_container(source);
	result.has_audio = has_audio(source) && !is_container(source);

	const Route video = routes.resolve_video(canvas, source, item).route;
	const Route audio = routes.resolve_audio(canvas, source).route;

	if (result.has_video && result.has_audio) {
		result.route = video;
		result.mixed = video != audio;
	} else if (result.has_audio) {
		result.route = audio;
	} else {
		result.route = video;
	}
	return result;
}

void set_combined_route(Controller &controller, const std::string &canvas, obs_source_t *source, obs_sceneitem_t *item,
			Route route)
{
	RouteStore &routes = controller.routes();
	const CombinedRoute current = combined_route(controller, canvas, source, item);

	if (current.has_video) {
		routes.set_video("", source, route);
		if (!canvas.empty())
			routes.set_video(canvas, source, std::nullopt);
		if (item && routes.has_item_video(item))
			routes.set_item_video(item, std::nullopt);
	}
	if (current.has_audio) {
		routes.set_audio("", source, route);
		if (!canvas.empty())
			routes.set_audio(canvas, source, std::nullopt);
	}
	controller.routes_changed();
}

void cycle_combined_route(Controller &controller, const std::string &canvas, obs_source_t *source,
			  obs_sceneitem_t *item)
{
	const CombinedRoute current = combined_route(controller, canvas, source, item);
	Route next = Route::Both;
	if (!current.mixed) {
		switch (current.route) {
		case Route::Both:
			next = Route::StreamOnly;
			break;
		case Route::StreamOnly:
			next = Route::RecordingOnly;
			break;
		case Route::RecordingOnly:
		case Route::Off:
			next = Route::Both;
			break;
		}
	}
	set_combined_route(controller, canvas, source, item, next);
}

QIcon route_icon(Route route, bool mixed)
{
	QIcon icon;
	for (int size : {16, 20, 32})
		icon.addPixmap(paint_icon(route, mixed, size));
	return icon;
}

QString combined_label(const CombinedRoute &route)
{
	if (route.mixed)
		return obs_module_text("Simple.Custom");
	switch (route.route) {
	case Route::Both:
		return obs_module_text("Simple.Both");
	case Route::StreamOnly:
		return obs_module_text("Simple.Stream");
	case Route::RecordingOnly:
		return obs_module_text("Simple.Recording");
	case Route::Off:
		return obs_module_text("Simple.Off");
	}
	return {};
}

QString combined_tooltip(const CombinedRoute &route)
{
	QString text;
	if (route.mixed) {
		text = obs_module_text("SimpleHelp.Custom");
	} else {
		switch (route.route) {
		case Route::Both:
			text = obs_module_text("SimpleHelp.Both");
			break;
		case Route::StreamOnly:
			text = obs_module_text("SimpleHelp.Stream");
			break;
		case Route::RecordingOnly:
			text = obs_module_text("SimpleHelp.Recording");
			break;
		case Route::Off:
			text = obs_module_text("SimpleHelp.Off");
			break;
		}
	}
	return text + QStringLiteral("\n\n") + QString::fromUtf8(obs_module_text("SimpleHelp.Click"));
}

} // namespace selective_recording
