// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "recording/recovery.hpp"

#include "core/settings.hpp"

#include <obs.hpp>
#include <util/platform.h>

#include <ctime>

namespace selective_recording::recovery {

namespace {

std::string sessions_dir()
{
	return Settings::config_path("sessions");
}

std::string marker_path(const std::string &id)
{
	return sessions_dir() + "/" + id + ".json";
}

std::string iso_now()
{
	char buffer[64] = {};
	const std::time_t now = std::time(nullptr);
	std::tm local = {};
#ifdef _WIN32
	localtime_s(&local, &now);
#else
	localtime_r(&now, &local);
#endif
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", &local);
	return buffer;
}

} // namespace

std::string begin(const std::string &canvas, const std::string &path, const std::string &container)
{
	char *uuid = os_generate_uuid();
	const std::string id = uuid ? uuid : std::to_string(os_gettime_ns());
	bfree(uuid);

	os_mkdirs(sessions_dir().c_str());

	OBSDataAutoRelease data = obs_data_create();
	obs_data_set_string(data, "canvas", canvas.c_str());
	obs_data_set_string(data, "path", path.c_str());
	obs_data_set_string(data, "container", container.c_str());
	obs_data_set_string(data, "started", iso_now().c_str());
	obs_data_save_json_safe(data, marker_path(id).c_str(), "tmp", nullptr);
	return id;
}

void end(const std::string &id)
{
	if (!id.empty())
		os_unlink(marker_path(id).c_str());
}

std::vector<InterruptedRecording> find_interrupted()
{
	std::vector<InterruptedRecording> result;
	os_dir_t *dir = os_opendir(sessions_dir().c_str());
	if (!dir)
		return result;

	for (os_dirent *entry = os_readdir(dir); entry; entry = os_readdir(dir)) {
		if (entry->directory)
			continue;
		std::string name = entry->d_name;
		const std::string ext = ".json";
		if (name.size() <= ext.size() || name.compare(name.size() - ext.size(), ext.size(), ext) != 0)
			continue;

		const std::string id = name.substr(0, name.size() - ext.size());
		OBSDataAutoRelease data = obs_data_create_from_json_file(marker_path(id).c_str());
		if (!data)
			continue;

		InterruptedRecording rec;
		rec.id = id;
		rec.canvas = obs_data_get_string(data, "canvas");
		rec.path = obs_data_get_string(data, "path");
		rec.container = obs_data_get_string(data, "container");
		rec.started = obs_data_get_string(data, "started");

		// A marker whose file never got written has nothing to recover.
		if (os_file_exists(rec.path.c_str()))
			result.push_back(rec);
		else
			end(id);
	}
	os_closedir(dir);
	return result;
}

void dismiss(const std::string &id)
{
	end(id);
}

} // namespace selective_recording::recovery
