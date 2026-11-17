// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <string>
#include <vector>

namespace selective_recording {

struct InterruptedRecording {
	std::string id;
	std::string canvas;
	std::string path;
	std::string container;
	std::string started;
};

// Session markers that reveal recordings interrupted by a crash or power loss (FR-009).
namespace recovery {

std::string begin(const std::string &canvas, const std::string &path, const std::string &container);
void end(const std::string &id);
std::vector<InterruptedRecording> find_interrupted();
void dismiss(const std::string &id);

} // namespace recovery

} // namespace selective_recording
