// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "ui/simple-routes.hpp"

#include "ui/icons.hpp"

#include <obs-module.h>

#include <QPainter>
#include <QPixmap>

namespace selective_recording {

namespace {

// Glyphs are designed on a 24×24 grid and drawn at each size an icon is shown at, so they stay
// sharp in OBS's Sources list (16 px) and on high-DPI screens.
constexpr double kGrid = 24.0;
const QPointF kCentre(12, 12);

void dot(QPainter &p, const QPointF &c, double radius, const QColor &color)
{
	p.setPen(Qt::NoPen);
	p.setBrush(color);
	p.drawEllipse(c, radius, radius);
}

QPen stroke(const QColor &color, double width)
{
	QPen pen(color, width);
	pen.setCapStyle(Qt::RoundCap);
	return pen;
}

// "On air": two pairs of radio waves either side of the centre.
void waves(QPainter &p, const QColor &color)
{
	p.setPen(stroke(color, 2.1));
	p.setBrush(Qt::NoBrush);
	for (double radius : {5.6, 9.8}) {
		const QRectF arc(kCentre.x() - radius, kCentre.y() - radius, radius * 2, radius * 2);
		p.drawArc(arc, -38 * 16, 76 * 16);
		p.drawArc(arc, 142 * 16, 76 * 16);
	}
}

QPixmap paint_icon(Route route, bool mixed, int size)
{
	QPixmap pixmap(size, size);
	pixmap.fill(Qt::transparent);
	QPainter p(&pixmap);
	p.setRenderHint(QPainter::Antialiasing);
	p.scale(size / kGrid, size / kGrid);

	switch (route) {
	case Route::StreamOnly:
		// On air, not recorded: waves around a small blue dot.
		waves(p, colors::kStream);
		dot(p, kCentre, 2.6, colors::kStream);
		break;
	case Route::RecordingOnly:
		// A record button: ring and solid centre.
		p.setPen(stroke(colors::kRecord, 1.9));
		p.setBrush(Qt::NoBrush);
		p.drawEllipse(kCentre, 9.3, 9.3);
		dot(p, kCentre, 5.3, colors::kRecord);
		break;
	case Route::Both:
		// On air and recorded: waves around a record dot.
		waves(p, colors::kStream);
		dot(p, kCentre, 3.6, colors::kRecord);
		break;
	case Route::Off:
		// Not recorded: a slashed circle.
		p.setPen(stroke(colors::kOff, 2.0));
		p.setBrush(Qt::NoBrush);
		p.drawEllipse(kCentre, 8.8, 8.8);
		p.drawLine(QPointF(5.8, 18.2), QPointF(18.2, 5.8));
		break;
	}

	if (mixed) {
		// An amber badge with a cut-out edge, so it reads against the glyph underneath.
		p.setCompositionMode(QPainter::CompositionMode_Clear);
		dot(p, QPointF(19.5, 4.5), 5.0, Qt::transparent);
		p.setCompositionMode(QPainter::CompositionMode_SourceOver);
		dot(p, QPointF(19.5, 4.5), 3.6, colors::kMixed);
	}
	return pixmap;
}

} // namespace

QIcon route_icon(Route route, bool mixed)
{
	QIcon icon;
	for (int size : {16, 20, 24, 32, 48})
		icon.addPixmap(paint_icon(route, mixed, size));
	return icon;
}

QString route_label(Route route)
{
	switch (route) {
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

QString route_tooltip(const PlacementRoute &route)
{
	QString text;
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
	return text + QStringLiteral("\n\n") +
	       QString::fromUtf8(obs_module_text(route.locked ? "SimpleHelp.Locked" : "SimpleHelp.Click"));
}

QString folder_tooltip(const FolderRoute &route)
{
	QString text;
	if (route.empty)
		text = obs_module_text("FolderHelp.Empty");
	else if (route.mixed)
		text = obs_module_text("FolderHelp.Mixed");
	else
		text = QString::fromUtf8(obs_module_text("FolderHelp.All")).arg(route_label(route.route));
	return text + QStringLiteral("\n\n") +
	       QString::fromUtf8(obs_module_text(route.locked ? "SimpleHelp.Locked" : "FolderHelp.Click"));
}

} // namespace selective_recording
