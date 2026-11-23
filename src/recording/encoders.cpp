// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "recording/encoders.hpp"

#include <algorithm>
#include <cstring>

namespace selective_recording {

namespace {

bool contains(const std::string &haystack, const char *needle)
{
	return haystack.find(needle) != std::string::npos;
}

bool output_registered(const char *id)
{
	const char *type = nullptr;
	for (size_t i = 0; obs_enum_output_types(i, &type); i++) {
		if (type && strcmp(type, id) == 0)
			return true;
	}
	return false;
}

} // namespace

std::vector<EncoderOption> video_encoders()
{
	std::vector<EncoderOption> result;
	const char *id = nullptr;
	for (size_t i = 0; obs_enum_encoder_types(i, &id); i++) {
		if (!id || obs_get_encoder_type(id) != OBS_ENCODER_VIDEO)
			continue;
		if (obs_get_encoder_caps(id) & (OBS_ENCODER_CAP_DEPRECATED | OBS_ENCODER_CAP_INTERNAL))
			continue;

		const char *codec = obs_get_encoder_codec(id);
		if (!codec || (strcmp(codec, "h264") != 0 && strcmp(codec, "hevc") != 0 && strcmp(codec, "av1") != 0))
			continue;

		const char *name = obs_encoder_get_display_name(id);
		result.push_back({id, name ? name : id, is_hardware_encoder(id)});
	}
	return result;
}

bool encoder_available(const std::string &id)
{
	const char *type = nullptr;
	for (size_t i = 0; obs_enum_encoder_types(i, &type); i++) {
		if (type && id == type)
			return true;
	}
	return false;
}

bool is_hardware_encoder(const std::string &id)
{
	return contains(id, "nvenc") || contains(id, "amf") || contains(id, "qsv") || contains(id, "vt_") ||
	       contains(id, "vaapi");
}

std::string resolve_video_encoder(const std::string &wanted)
{
	if (wanted != "auto" && encoder_available(wanted))
		return wanted;

	static const char *preferred[] = {"obs_nvenc_h264_tex", "jim_nvenc", "h264_texture_amf",
					  "obs_qsv11_v2",       "obs_qsv11", kSoftwareEncoder};
	for (const char *id : preferred) {
		if (encoder_available(id))
			return id;
	}
	return kSoftwareEncoder;
}

obs_data_t *video_encoder_settings(const std::string &id, RateControl rate_control, int quality, int bitrate)
{
	obs_data_t *s = obs_data_create();
	obs_data_set_int(s, "keyint_sec", 2);
	quality = std::clamp(quality, 1, 51);
	bitrate = std::max(bitrate, 500);

	if (rate_control == RateControl::Bitrate) {
		obs_data_set_string(s, "rate_control", "CBR");
		obs_data_set_int(s, "bitrate", bitrate);
		if (id == kSoftwareEncoder)
			obs_data_set_string(s, "preset", "veryfast");
		return s;
	}

	// Constant quality: CRF for x264, CQP for hardware encoders.
	if (id == kSoftwareEncoder) {
		obs_data_set_string(s, "rate_control", "CRF");
		obs_data_set_int(s, "crf", quality);
		obs_data_set_string(s, "preset", "veryfast");
	} else if (contains(id, "nvenc")) {
		obs_data_set_string(s, "rate_control", "CQP");
		obs_data_set_int(s, "cqp", quality);
		obs_data_set_string(s, "preset2", "p5");
	} else if (contains(id, "amf") || contains(id, "qsv")) {
		obs_data_set_string(s, "rate_control", "CQP");
		obs_data_set_int(s, "cqp", quality);
	} else {
		obs_data_set_string(s, "rate_control", "CBR");
		obs_data_set_int(s, "bitrate", bitrate);
	}
	return s;
}

obs_data_t *audio_encoder_settings(int bitrate)
{
	obs_data_t *s = obs_data_create();
	obs_data_set_int(s, "bitrate", std::clamp(bitrate, 64, 512));
	return s;
}

int active_encoder_count(const std::string &id)
{
	struct Count {
		const std::string *id;
		int count;
	} count{&id, 0};

	obs_enum_encoders(
		[](void *param, obs_encoder_t *encoder) {
			auto *c = static_cast<Count *>(param);
			const char *eid = obs_encoder_get_id(encoder);
			if (eid && *c->id == eid && obs_encoder_active(encoder))
				c->count++;
			return true;
		},
		&count);
	return count.count;
}

ContainerInfo container_info(const std::string &container)
{
	if (container == "mp4") {
		// The hybrid MP4 output survives crashes; fall back to FFmpeg on older OBS.
		if (output_registered("mp4_output"))
			return {"mp4_output", "mp4"};
		return {"ffmpeg_muxer", "mp4"};
	}
	if (container == "mov")
		return {"ffmpeg_muxer", "mov"};
	return {"ffmpeg_muxer", "mkv"};
}

} // namespace selective_recording
