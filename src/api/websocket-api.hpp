// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

namespace selective_recording::websocket {

// Registers the "selective-recording" obs-websocket vendor, if obs-websocket is installed.
void register_vendor();
void unregister_vendor();
void emit_state_changed();

} // namespace selective_recording::websocket
