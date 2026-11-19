// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include "core/routes.hpp"

#include <QToolButton>

namespace selective_recording {

// Shows a source's route (its video and audio together) and cycles through routes on click.
// Right-click opens explicit choices.
class RouteButton : public QToolButton {
	Q_OBJECT

public:
	explicit RouteButton(QWidget *parent = nullptr);

	void set_route(const ResolvedRoute &route);
	Route route() const { return route_.route; }

	static QString label(Route route);
	static QString description(Route route);

signals:
	void cycleRequested();
	void menuRequested(const QPoint &global_pos);

private:
	ResolvedRoute route_;
};

} // namespace selective_recording
