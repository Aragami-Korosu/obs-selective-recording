// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <obs.hpp>

#include <functional>
#include <memory>
#include <string>

namespace selective_recording {

struct RecorderConfig {
	std::string path;
	std::string output_id;
	std::string video_encoder;
	OBSData video_settings;
	uint32_t fps_divisor = 1;
	std::string audio_encoder;
	OBSData audio_settings;
	std::string name;
};

struct RecorderStats {
	uint64_t bytes = 0;
	int dropped_frames = 0;
	int total_frames = 0;
	uint64_t duration_ms = 0;
};

// One recording output with its own encoders. Never touches outputs it did not create.
class Recorder {
public:
	// Called on the UI thread when the output stops. A non-zero code means it failed.
	using StoppedFn = std::function<void(int code, const std::string &error)>;

	explicit Recorder(StoppedFn on_stopped);
	~Recorder();
	Recorder(const Recorder &) = delete;
	Recorder &operator=(const Recorder &) = delete;

	bool start(video_t *video, audio_t *audio, const RecorderConfig &config, std::string &error);
	void stop();
	bool wait_stopped(uint32_t timeout_ms);
	bool set_paused(bool paused);

	bool active() const;
	bool paused() const { return paused_; }
	const std::string &path() const { return path_; }
	RecorderStats stats() const;

private:
	void release();
	static void on_stop(void *param, calldata_t *cd);

	OBSOutputAutoRelease output_;
	OBSEncoderAutoRelease video_encoder_;
	OBSEncoderAutoRelease audio_encoder_;
	OBSSignal stop_signal_;
	StoppedFn on_stopped_;
	std::string path_;
	bool paused_ = false;
	uint64_t start_ns_ = 0;
	uint64_t paused_ns_ = 0;
	uint64_t pause_started_ns_ = 0;
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace selective_recording
