// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "ui/route-button.hpp"

#include "ui/icons.hpp"
#include "ui/simple-routes.hpp"

#include <obs-module.h>

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>

namespace selective_recording {

namespace {

constexpr int kIconSize = 16;
constexpr int kBadgeSize = 13;
constexpr int kPadding = 9;
constexpr int kGap = 6;

QColor with_alpha(QColor color, int alpha)
{
	color.setAlpha(std::clamp(alpha, 0, 255));
	return color;
}

// Short labels keep the column narrow; menus and tooltips use the full wording.
QString chip_label(Route route, bool mixed)
{
	if (mixed)
		return QString::fromUtf8(obs_module_text("Folder.Mixed"));
	switch (route) {
	case Route::Both:
		return QString::fromUtf8(obs_module_text("Chip.Both"));
	case Route::StreamOnly:
		return QString::fromUtf8(obs_module_text("Chip.StreamOnly"));
	case Route::RecordingOnly:
		return QString::fromUtf8(obs_module_text("Chip.RecordingOnly"));
	case Route::Off:
		return QString::fromUtf8(obs_module_text("Chip.Off"));
	}
	return {};
}

} // namespace

RouteButton::RouteButton(QWidget *parent) : QAbstractButton(parent)
{
	setCursor(Qt::PointingHandCursor);
	setFocusPolicy(Qt::NoFocus);
	setAttribute(Qt::WA_Hover);
	setContextMenuPolicy(Qt::CustomContextMenu);
	setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

	connect(this, &QAbstractButton::clicked, this, &RouteButton::cycleRequested);
	connect(this, &QWidget::customContextMenuRequested, this,
		[this](const QPoint &pos) { emit menuRequested(mapToGlobal(pos)); });
}

QString RouteButton::description(Route route)
{
	switch (route) {
	case Route::Both:
		return obs_module_text("RouteHelp.Both");
	case Route::StreamOnly:
		return obs_module_text("RouteHelp.StreamOnly");
	case Route::RecordingOnly:
		return obs_module_text("RouteHelp.RecordingOnly");
	case Route::Off:
		return obs_module_text("RouteHelp.Off");
	}
	return {};
}

void RouteButton::set_route(const ResolvedRoute &route)
{
	route_ = route.route;
	origin_ = route.origin;
	inherited_ = route.origin == RouteOrigin::ProfileDefault;
	mixed_ = false;
	show_scope_ = true;

	const char *origin_key = "RouteOrigin.Default";
	switch (route.origin) {
	case RouteOrigin::Item:
		origin_key = "RouteOrigin.Item";
		break;
	case RouteOrigin::Canvas:
		origin_key = "RouteOrigin.Canvas";
		break;
	case RouteOrigin::Source:
		origin_key = "RouteOrigin.Source";
		break;
	case RouteOrigin::ProfileDefault:
		break;
	}
	setToolTip(description(route.route) + QStringLiteral("\n\n") + QString::fromUtf8(obs_module_text(origin_key)) +
		   QStringLiteral("\n") + QString::fromUtf8(obs_module_text("RouteHelp.Click")));
	setAccessibleName(chip_label(route.route, false));
	update();
}

void RouteButton::set_placement(Route route, bool mixed, const QString &tooltip)
{
	route_ = route;
	origin_ = RouteOrigin::Item;
	inherited_ = false;
	mixed_ = mixed;
	// Every button in the current-scene view sets a placement, so a badge would say nothing.
	show_scope_ = false;
	setToolTip(tooltip);
	setAccessibleName(chip_label(route, mixed));
	update();
}

QSize RouteButton::sizeHint() const
{
	// Every chip is as wide as the longest label, so the column lines up. Measure with the font
	// OBS's theme gives the chip, not the default one it starts with.
	ensurePolished();
	const QFontMetrics fm(font());
	int text = fm.horizontalAdvance(chip_label(Route::Both, true));
	for (Route r : {Route::Both, Route::StreamOnly, Route::RecordingOnly, Route::Off})
		text = std::max(text, fm.horizontalAdvance(chip_label(r, false)));
	const int width = kPadding + kIconSize + kGap + text + kGap + kBadgeSize + kPadding;
	return {width, std::max(24, fm.height() + 8)};
}

void RouteButton::paintEvent(QPaintEvent *)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing);

	const QRectF chip = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
	const double radius = chip.height() / 2.0;

	// Strength of the tint: softer for a profile default, stronger on hover and press.
	int fill = inherited_ ? 22 : 42;
	int border = inherited_ ? 85 : 170;
	if (!isEnabled()) {
		fill = 10;
		border = 45;
	} else if (isDown()) {
		fill += 45;
	} else if (underMouse()) {
		fill += 24;
		border = std::min(255, border + 50);
	}

	QBrush fill_brush;
	QBrush border_brush;
	if (mixed_) {
		fill_brush = with_alpha(colors::kMixed, fill);
		border_brush = with_alpha(colors::kMixed, border);
	} else if (route_ == Route::Both) {
		// Stream and recording: blend from the stream colour to the recording colour.
		QLinearGradient fg(chip.topLeft(), chip.topRight());
		fg.setColorAt(0, with_alpha(colors::kStream, fill));
		fg.setColorAt(1, with_alpha(colors::kRecord, fill));
		QLinearGradient bg(chip.topLeft(), chip.topRight());
		bg.setColorAt(0, with_alpha(colors::kStream, border));
		bg.setColorAt(1, with_alpha(colors::kRecord, border));
		fill_brush = fg;
		border_brush = bg;
	} else {
		const QColor accent = route_ == Route::StreamOnly      ? colors::kStream
				      : route_ == Route::RecordingOnly ? colors::kRecord
								       : colors::kOff;
		fill_brush = with_alpha(accent, fill);
		border_brush = with_alpha(accent, border);
	}

	p.setPen(QPen(border_brush, 1.0));
	p.setBrush(fill_brush);
	p.drawRoundedRect(chip, radius, radius);

	// Icon.
	const int icon_y = (height() - kIconSize) / 2;
	QRect icon_rect(kPadding, icon_y, kIconSize, kIconSize);
	route_icon(route_, mixed_).paint(&p, icon_rect, Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled);

	// Scope badge: one canvas, or one placement.
	QColor text = palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText);
	const QRect badge_rect(width() - kPadding - kBadgeSize, (height() - kBadgeSize) / 2, kBadgeSize, kBadgeSize);
	if (show_scope_ && origin_ == RouteOrigin::Canvas)
		canvas_scope_icon(with_alpha(text, 200)).paint(&p, badge_rect);
	else if (show_scope_ && origin_ == RouteOrigin::Item)
		placement_scope_icon(with_alpha(text, 200)).paint(&p, badge_rect);

	// Label.
	if (inherited_)
		text = with_alpha(text, 170);
	p.setPen(text);
	const QRect text_rect(icon_rect.right() + 1 + kGap, 0, badge_rect.left() - kGap - icon_rect.right() - 1 - kGap,
			      height());
	p.drawText(text_rect, Qt::AlignVCenter | Qt::AlignLeft,
		   fontMetrics().elidedText(chip_label(route_, mixed_), Qt::ElideRight, text_rect.width()));
}

} // namespace selective_recording
