// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <obs.h>

#include <QColor>
#include <QIcon>

namespace selective_recording {

// Colours shared by the route chips and icons.
namespace colors {
inline const QColor kStream(77, 160, 230);
inline const QColor kRecord(232, 72, 72);
inline const QColor kOff(140, 140, 140);
inline const QColor kMixed(240, 180, 60);
inline const QColor kWarning(217, 164, 65);
} // namespace colors

// Small painted icons for the dock. Each is drawn at several sizes so it stays sharp on
// high-DPI screens. `color` is used for monochrome glyphs, normally the text colour.
QIcon record_icon();
QIcon stop_icon();
QIcon pause_icon(const QColor &color);
QIcon preview_icon(const QColor &color);
QIcon warning_icon();

// Marks a route that applies to one canvas only, or to one placement only.
QIcon canvas_scope_icon(const QColor &color);
QIcon placement_scope_icon(const QColor &color);

// The icon OBS's own Sources list shows for this source, from the current OBS theme. Null if
// the theme doesn't provide one.
QIcon source_type_icon(obs_source_t *source);
QIcon audio_devices_icon();

} // namespace selective_recording
