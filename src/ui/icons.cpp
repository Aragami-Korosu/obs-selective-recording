// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "ui/icons.hpp"

#include <obs-frontend-api.h>

#include <QMainWindow>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QVariant>

#include <functional>

namespace selective_recording {

namespace {

// Paints a glyph designed on a 24×24 grid at each size an icon may be shown at.
QIcon painted(const std::function<void(QPainter &)> &draw)
{
	QIcon icon;
	for (int size : {16, 20, 24, 32, 48}) {
		QPixmap pixmap(size, size);
		pixmap.fill(Qt::transparent);
		QPainter p(&pixmap);
		p.setRenderHint(QPainter::Antialiasing);
		p.scale(size / 24.0, size / 24.0);
		draw(p);
		p.end();
		icon.addPixmap(pixmap);
	}
	return icon;
}

QPen round_pen(const QColor &color, double width)
{
	QPen pen(color, width);
	pen.setCapStyle(Qt::RoundCap);
	pen.setJoinStyle(Qt::RoundJoin);
	return pen;
}

// Themed icons are properties of OBS's main window (the theme sets them, e.g. qproperty-imageIcon).
QIcon theme_icon(const char *property)
{
	auto *main = static_cast<QMainWindow *>(obs_frontend_get_main_window());
	if (!main)
		return {};
	return main->property(property).value<QIcon>();
}

} // namespace

QIcon record_icon()
{
	return painted([](QPainter &p) {
		p.setPen(round_pen(colors::kRecord, 1.8));
		p.setBrush(Qt::NoBrush);
		p.drawEllipse(QPointF(12, 12), 9.0, 9.0);
		p.setPen(Qt::NoPen);
		p.setBrush(colors::kRecord);
		p.drawEllipse(QPointF(12, 12), 5.2, 5.2);
	});
}

QIcon stop_icon()
{
	return painted([](QPainter &p) {
		p.setPen(Qt::NoPen);
		p.setBrush(colors::kRecord);
		p.drawRoundedRect(QRectF(6, 6, 12, 12), 2.5, 2.5);
	});
}

QIcon pause_icon(const QColor &color)
{
	return painted([color](QPainter &p) {
		p.setPen(Qt::NoPen);
		p.setBrush(color);
		p.drawRoundedRect(QRectF(6.5, 5, 4, 14), 1.4, 1.4);
		p.drawRoundedRect(QRectF(13.5, 5, 4, 14), 1.4, 1.4);
	});
}

QIcon preview_icon(const QColor &color)
{
	return painted([color](QPainter &p) {
		QPainterPath eye;
		eye.moveTo(2.5, 12);
		eye.cubicTo(6, 5.5, 18, 5.5, 21.5, 12);
		eye.cubicTo(18, 18.5, 6, 18.5, 2.5, 12);
		p.setPen(round_pen(color, 1.7));
		p.setBrush(Qt::NoBrush);
		p.drawPath(eye);
		p.setPen(Qt::NoPen);
		p.setBrush(color);
		p.drawEllipse(QPointF(12, 12), 3.2, 3.2);
	});
}

QIcon warning_icon()
{
	return painted([](QPainter &p) {
		QPainterPath triangle;
		triangle.moveTo(12, 3);
		triangle.lineTo(22, 20.5);
		triangle.lineTo(2, 20.5);
		triangle.closeSubpath();
		p.setPen(round_pen(colors::kWarning, 1.6));
		p.setBrush(colors::kWarning);
		p.drawPath(triangle);
		p.setPen(round_pen(QColor(30, 30, 30), 2.2));
		p.drawLine(QPointF(12, 9), QPointF(12, 13.6));
		p.setPen(Qt::NoPen);
		p.setBrush(QColor(30, 30, 30));
		p.drawEllipse(QPointF(12, 17.2), 1.25, 1.25);
	});
}

QIcon canvas_scope_icon(const QColor &color)
{
	// A screen: this canvas only.
	return painted([color](QPainter &p) {
		p.setPen(round_pen(color, 2.0));
		p.setBrush(Qt::NoBrush);
		p.drawRoundedRect(QRectF(3, 4.5, 18, 12), 2, 2);
		p.drawLine(QPointF(9, 20.5), QPointF(15, 20.5));
	});
}

QIcon placement_scope_icon(const QColor &color)
{
	// A map pin: this placement only.
	return painted([color](QPainter &p) {
		QPainterPath pin;
		pin.moveTo(12, 21.5);
		pin.cubicTo(7, 15.5, 5, 12.5, 5, 9.5);
		pin.arcTo(QRectF(5, 2.5, 14, 14), 180, -180);
		pin.cubicTo(19, 12.5, 17, 15.5, 12, 21.5);
		p.setPen(round_pen(color, 2.0));
		p.setBrush(Qt::NoBrush);
		p.drawPath(pin);
		p.setPen(Qt::NoPen);
		p.setBrush(color);
		p.drawEllipse(QPointF(12, 9.5), 2.4, 2.4);
	});
}

QIcon source_type_icon(obs_source_t *source)
{
	if (!source)
		return {};
	if (obs_source_is_group(source))
		return theme_icon("groupIcon");
	if (obs_source_is_scene(source))
		return theme_icon("sceneIcon");

	switch (obs_source_get_icon_type(obs_source_get_id(source))) {
	case OBS_ICON_TYPE_IMAGE:
		return theme_icon("imageIcon");
	case OBS_ICON_TYPE_COLOR:
		return theme_icon("colorIcon");
	case OBS_ICON_TYPE_SLIDESHOW:
		return theme_icon("slideshowIcon");
	case OBS_ICON_TYPE_AUDIO_INPUT:
		return theme_icon("audioInputIcon");
	case OBS_ICON_TYPE_AUDIO_OUTPUT:
		return theme_icon("audioOutputIcon");
	case OBS_ICON_TYPE_DESKTOP_CAPTURE:
		return theme_icon("desktopCapIcon");
	case OBS_ICON_TYPE_WINDOW_CAPTURE:
		return theme_icon("windowCapIcon");
	case OBS_ICON_TYPE_GAME_CAPTURE:
		return theme_icon("gameCapIcon");
	case OBS_ICON_TYPE_CAMERA:
		return theme_icon("cameraIcon");
	case OBS_ICON_TYPE_TEXT:
		return theme_icon("textIcon");
	case OBS_ICON_TYPE_MEDIA:
		return theme_icon("mediaIcon");
	case OBS_ICON_TYPE_BROWSER:
		return theme_icon("browserIcon");
	case OBS_ICON_TYPE_PROCESS_AUDIO_OUTPUT:
		return theme_icon("audioProcessOutputIcon");
	default:
		return theme_icon("defaultIcon");
	}
}

QIcon audio_devices_icon()
{
	return theme_icon("audioOutputIcon");
}

} // namespace selective_recording
