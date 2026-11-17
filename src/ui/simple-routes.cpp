// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "ui/simple-routes.hpp"

#include "core/controller.hpp"
#include "core/obs-utils.hpp"

#include <obs-module.h>

#include <QPainter>
#include <QPixmap>

#include <algorithm>

namespace selective_recording {

namespace {

const QColor kStream(77, 160, 230);
const QColor kRecord(232, 72, 72);
const QColor kOff(140, 140, 140);
const QColor kMixed(240, 180, 60);

bool is_container(obs_source_t *source)
{
	return obs_source_is_scene(source) || obs_source_is_group(source);
}

void draw_dot(QPainter &p, const QPointF &c, double radius, const QColor &color)
{
	p.setPen(Qt::NoPen);
	p.setBrush(color);
	p.drawEllipse(c, radius, radius);
}

// Broadcast symbol: two pairs of radio-wave arcs either side of the centre.
void draw_waves(QPainter &p, const QPointF &c, double half, const QColor &color)
{
	QPen pen(color, std::max(1.2, half * 0.17));
	pen.setCapStyle(Qt::RoundCap);
	p.setPen(pen);
	p.setBrush(Qt::NoBrush);
	for (double scale : {0.52, 0.9}) {
		const double rad = half * scale;
		const QRectF arc(c.x() - rad, c.y() - rad, rad * 2, rad * 2);
		p.drawArc(arc, -45 * 16, 90 * 16);
		p.drawArc(arc, 135 * 16, 90 * 16);
	}
}

QPixmap paint_icon(Route route, bool mixed, int size)
{
	QPixmap pixmap(size, size);
	pixmap.fill(Qt::transparent);
	QPainter p(&pixmap);
	p.setRenderHint(QPainter::Antialiasing);

	const QPointF c(size / 2.0, size / 2.0);
	const double half = size / 2.0 - 1.0;

	switch (route) {
	case Route::StreamOnly:
		draw_waves(p, c, half, kStream);
		draw_dot(p, c, half * 0.22, kStream);
		break;
	case Route::RecordingOnly: {
		// Record button: a ring with a solid centre.
		QPen pen(kRecord, std::max(1.2, half * 0.15));
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.drawEllipse(c, half * 0.82, half * 0.82);
		draw_dot(p, c, half * 0.45, kRecord);
		break;
	}
	case Route::Both:
		// Broadcasting and recording: radio waves around a record dot.
		draw_waves(p, c, half, kStream);
		draw_dot(p, c, half * 0.32, kRecord);
		break;
	case Route::Off: {
		// Not recorded: a slashed circle.
		QPen pen(kOff, std::max(1.2, half * 0.15));
		pen.setCapStyle(Qt::RoundCap);
		p.setPen(pen);
		p.setBrush(Qt::NoBrush);
		p.drawEllipse(c, half * 0.78, half * 0.78);
		const double d = half * 0.55;
		p.drawLine(QPointF(c.x() - d, c.y() + d), QPointF(c.x() + d, c.y() - d));
		break;
	}
	}

	if (mixed) {
		// Video and audio are set differently.
		draw_dot(p, QPointF(size - half * 0.3 - 0.5, half * 0.3 + 0.5), half * 0.3, kMixed);
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
