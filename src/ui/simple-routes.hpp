// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include "core/placement.hpp"

#include <QIcon>
#include <QString>

namespace selective_recording {

// Symbols and text for the Streamlabs-style placement buttons.
QIcon route_icon(Route route);
QString route_label(Route route);
QString route_tooltip(const PlacementRoute &route);

} // namespace selective_recording
