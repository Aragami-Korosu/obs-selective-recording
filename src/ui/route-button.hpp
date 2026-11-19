// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include "core/routes.hpp"

#include <QAbstractButton>

namespace selective_recording {

// A pill-shaped chip showing a route (a source's video and sound together): its icon, its label
// and a tint in the route's colour. Click cycles; right-click asks for the menu.
//
// A route set for one canvas or one placement only gets a small badge (a screen or a pin), and
// a route that only uses the profile default is drawn softer.
class RouteButton : public QAbstractButton {
	Q_OBJECT

public:
	explicit RouteButton(QWidget *parent = nullptr);

	// A source or placement route in the All scenes view, with where it comes from.
	void set_route(const ResolvedRoute &route);
	// A placement or folder button in the current-scene view. "mixed" is a folder whose sources differ.
	void set_placement(Route route, bool mixed, const QString &tooltip);

	Route route() const { return route_; }

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override { return sizeHint(); }

	static QString description(Route route);

signals:
	void cycleRequested();
	void menuRequested(const QPoint &global_pos);

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	Route route_ = Route::Both;
	RouteOrigin origin_ = RouteOrigin::Item;
	bool inherited_ = false;
	bool mixed_ = false;
	bool show_scope_ = false;
};

} // namespace selective_recording
