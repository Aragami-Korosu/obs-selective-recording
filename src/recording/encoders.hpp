// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include "core/settings.hpp"

#include <obs.h>

#include <string>
#include <vector>

namespace selective_recording {

inline constexpr const char *kSoftwareEncoder = "obs_x264";

struct EncoderOption {
	std::string id;
	std::string name;
	bool hardware = false;
};

// Video encoders registered in this OBS, discovered at runtime.
std::vector<EncoderOption> video_encoders();
bool encoder_available(const std::string &id);
bool is_hardware_encoder(const std::string &id);

// "auto" or an encoder that is no longer installed resolves to the best available one.
std::string resolve_video_encoder(const std::string &wanted);

obs_data_t *video_encoder_settings(const std::string &id, RateControl rate_control, int quality, int bitrate);
obs_data_t *audio_encoder_settings(int bitrate);

// Number of encoders of this type currently running anywhere in OBS.
int active_encoder_count(const std::string &id);

struct ContainerInfo {
	std::string output_id;
	std::string extension;
};
ContainerInfo container_info(const std::string &container);

} // namespace selective_recording
