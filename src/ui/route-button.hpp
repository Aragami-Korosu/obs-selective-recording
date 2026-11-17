// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include "core/routes.hpp"

#include <QToolButton>

namespace selective_recording {

// Shows a route and cycles through routes on click. Right-click opens explicit choices.
class RouteButton : public QToolButton {
	Q_OBJECT

public:
	enum class Kind { Video, Audio };

	RouteButton(Kind kind, QWidget *parent = nullptr);

	void set_route(const ResolvedRoute &route);
	Route route() const { return route_.route; }
	Kind kind() const { return kind_; }

	static QString label(Route route);
	static QString description(Route route, Kind kind);

signals:
	void cycleRequested();
	void menuRequested(const QPoint &global_pos);

private:
	Kind kind_;
	ResolvedRoute route_;
};

} // namespace selective_recording
