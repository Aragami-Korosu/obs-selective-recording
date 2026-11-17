// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "core/settings.hpp"

#include <obs-module.h>
#include <obs.hpp>
#include <plugin-support.h>
#include <util/platform.h>

#include <algorithm>
#include <cstring>

namespace selective_recording {

namespace {

constexpr int kSchemaVersion = 1;

const char *rate_control_name(RateControl rc)
{
	return rc == RateControl::Bitrate ? "bitrate" : "quality";
}

RateControl rate_control_from(const char *value)
{
	return value && strcmp(value, "bitrate") == 0 ? RateControl::Bitrate : RateControl::Quality;
}

} // namespace

Settings::Settings() = default;

Settings::~Settings()
{
	for (auto &[key, canvas] : canvases_)
		obs_data_release(canvas.hotkeys);
}

std::string Settings::config_path(const char *file)
{
	char *path = obs_module_config_path(file);
	std::string result = path ? path : "";
	bfree(path);
	return result;
}

void Settings::ensure_config_dir()
{
	os_mkdirs(config_path("").c_str());
}

CanvasSettings &Settings::canvas(const std::string &key)
{
	return canvases_[key];
}

void Settings::load()
{
	const std::string path = config_path("settings.json");
	OBSDataAutoRelease data = obs_data_create_from_json_file_safe(path.c_str(), "bak");
	if (!data)
		return;

	enabled = !obs_data_has_user_value(data, "plugin_enabled") || obs_data_get_bool(data, "plugin_enabled");
	offer_recovery = !obs_data_has_user_value(data, "offer_recovery") || obs_data_get_bool(data, "offer_recovery");

	OBSDataAutoRelease rec = obs_data_get_obj(data, "recording");
	if (rec) {
		if (*obs_data_get_string(rec, "container"))
			recording.container = obs_data_get_string(rec, "container");
		if (*obs_data_get_string(rec, "audio_encoder"))
			recording.audio_encoder = obs_data_get_string(rec, "audio_encoder");
		if (obs_data_has_user_value(rec, "audio_bitrate"))
			recording.audio_bitrate = (int)obs_data_get_int(rec, "audio_bitrate");
		if (*obs_data_get_string(rec, "filename_format"))
			recording.filename_format = obs_data_get_string(rec, "filename_format");
		if (obs_data_has_user_value(rec, "fallback_to_software"))
			recording.fallback_to_software = obs_data_get_bool(rec, "fallback_to_software");
	}

	OBSDataAutoRelease warn = obs_data_get_obj(data, "warnings");
	if (warn) {
		if (obs_data_has_user_value(warn, "live_visibility"))
			warnings.live_visibility = obs_data_get_bool(warn, "live_visibility");
		if (obs_data_has_user_value(warn, "audio_tracks"))
			warnings.audio_tracks = obs_data_get_bool(warn, "audio_tracks");
	}

	OBSDataAutoRelease canvases = obs_data_get_obj(data, "canvases");
	for (obs_data_item_t *item = obs_data_first(canvases); item; obs_data_item_next(&item)) {
		OBSDataAutoRelease entry = obs_data_item_get_obj(item);
		if (!entry)
			continue;

		CanvasSettings &cs = canvas(obs_data_item_get_name(item));
		cs.enabled = !obs_data_has_user_value(entry, "enabled") || obs_data_get_bool(entry, "enabled");
		cs.width = (uint32_t)obs_data_get_int(entry, "width");
		cs.height = (uint32_t)obs_data_get_int(entry, "height");
		cs.fps_divisor = (uint32_t)std::max<long long>(1, obs_data_get_int(entry, "fps_divisor"));
		if (*obs_data_get_string(entry, "video_encoder"))
			cs.video_encoder = obs_data_get_string(entry, "video_encoder");
		cs.rate_control = rate_control_from(obs_data_get_string(entry, "rate_control"));
		if (obs_data_has_user_value(entry, "quality"))
			cs.quality = (int)obs_data_get_int(entry, "quality");
		if (obs_data_has_user_value(entry, "bitrate"))
			cs.bitrate = (int)obs_data_get_int(entry, "bitrate");
		cs.output_directory = obs_data_get_string(entry, "output_directory");
		cs.filename_suffix = obs_data_get_string(entry, "filename_suffix");

		obs_data_release(cs.hotkeys);
		cs.hotkeys = obs_data_get_obj(entry, "hotkeys");
	}
}

void Settings::save() const
{
	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_int(data, "schema_version", kSchemaVersion);
	obs_data_set_bool(data, "plugin_enabled", enabled);
	obs_data_set_bool(data, "offer_recovery", offer_recovery);

	OBSDataAutoRelease rec = obs_data_create();
	obs_data_set_string(rec, "container", recording.container.c_str());
	obs_data_set_string(rec, "audio_encoder", recording.audio_encoder.c_str());
	obs_data_set_int(rec, "audio_bitrate", recording.audio_bitrate);
	obs_data_set_string(rec, "filename_format", recording.filename_format.c_str());
	obs_data_set_bool(rec, "fallback_to_software", recording.fallback_to_software);
	obs_data_set_obj(data, "recording", rec);

	OBSDataAutoRelease warn = obs_data_create();
	obs_data_set_bool(warn, "live_visibility", warnings.live_visibility);
	obs_data_set_bool(warn, "audio_tracks", warnings.audio_tracks);
	obs_data_set_obj(data, "warnings", warn);

	OBSDataAutoRelease canvases = obs_data_create();
	for (const auto &[key, cs] : canvases_) {
		OBSDataAutoRelease entry = obs_data_create();
		obs_data_set_bool(entry, "enabled", cs.enabled);
		obs_data_set_int(entry, "width", cs.width);
		obs_data_set_int(entry, "height", cs.height);
		obs_data_set_int(entry, "fps_divisor", cs.fps_divisor);
		obs_data_set_string(entry, "video_encoder", cs.video_encoder.c_str());
		obs_data_set_string(entry, "rate_control", rate_control_name(cs.rate_control));
		obs_data_set_int(entry, "quality", cs.quality);
		obs_data_set_int(entry, "bitrate", cs.bitrate);
		obs_data_set_string(entry, "output_directory", cs.output_directory.c_str());
		obs_data_set_string(entry, "filename_suffix", cs.filename_suffix.c_str());
		if (cs.hotkeys)
			obs_data_set_obj(entry, "hotkeys", cs.hotkeys);
		obs_data_set_obj(canvases, key.c_str(), entry);
	}
	obs_data_set_obj(data, "canvases", canvases);

	ensure_config_dir();
	const std::string path = config_path("settings.json");
	if (!obs_data_save_json_safe(data, path.c_str(), "tmp", "bak"))
		obs_log(LOG_WARNING, "could not save settings to %s", path.c_str());
}

} // namespace selective_recording
