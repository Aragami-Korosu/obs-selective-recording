// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "recording/recorder.hpp"

#include "core/obs-utils.hpp"

#include <plugin-support.h>
#include <util/platform.h>

namespace selective_recording {

Recorder::Recorder(StoppedFn on_stopped) : on_stopped_(std::move(on_stopped)) {}

Recorder::~Recorder()
{
	*alive_ = false;
	if (active()) {
		obs_output_stop(output_);
		wait_stopped(5000);
	}
	release();
}

void Recorder::release()
{
	stop_signal_.Disconnect();
	output_ = nullptr;
	video_encoder_ = nullptr;
	audio_encoder_ = nullptr;
}

bool Recorder::start(video_t *video, audio_t *audio, const RecorderConfig &config, std::string &error)
{
	release();
	paused_ = false;
	paused_ns_ = 0;

	if (!video || !audio) {
		error = "The recording pipeline is not ready.";
		return false;
	}

	video_encoder_ = obs_video_encoder_create(config.video_encoder.c_str(), (config.name + " video").c_str(),
						  config.video_settings, nullptr);
	if (!video_encoder_) {
		error = "Could not create the video encoder '" + config.video_encoder + "'.";
		return false;
	}
	obs_encoder_set_video(video_encoder_, video);
	if (config.fps_divisor > 1)
		obs_encoder_set_frame_rate_divisor(video_encoder_, config.fps_divisor);

	audio_encoder_ = obs_audio_encoder_create(config.audio_encoder.c_str(), (config.name + " audio").c_str(),
						  config.audio_settings, 0, nullptr);
	if (!audio_encoder_) {
		error = "Could not create the audio encoder '" + config.audio_encoder + "'.";
		release();
		return false;
	}
	obs_encoder_set_audio(audio_encoder_, audio);

	OBSDataAutoRelease output_settings = obs_data_create();
	obs_data_set_string(output_settings, "path", config.path.c_str());
	output_ = obs_output_create(config.output_id.c_str(), config.name.c_str(), output_settings, nullptr);
	if (!output_) {
		error = "Could not create the recording output.";
		release();
		return false;
	}

	obs_output_set_video_encoder(output_, video_encoder_);
	obs_output_set_audio_encoder(output_, audio_encoder_, 0);
	stop_signal_.Connect(obs_output_get_signal_handler(output_), "stop", on_stop, this);

	path_ = config.path;
	start_ns_ = os_gettime_ns();

	if (!obs_output_start(output_)) {
		const char *last = obs_output_get_last_error(output_);
		error = last && *last ? last : "The recording output could not start.";
		release();
		return false;
	}
	return true;
}

void Recorder::stop()
{
	if (output_ && obs_output_active(output_))
		obs_output_stop(output_);
}

bool Recorder::wait_stopped(uint32_t timeout_ms)
{
	const uint64_t deadline = os_gettime_ns() + (uint64_t)timeout_ms * 1000000ULL;
	while (output_ && obs_output_active(output_)) {
		if (os_gettime_ns() > deadline) {
			obs_log(LOG_WARNING, "recording did not stop in time; forcing it to stop");
			obs_output_force_stop(output_);
			return false;
		}
		os_sleep_ms(10);
	}
	return true;
}

bool Recorder::set_paused(bool paused)
{
	if (!active() || paused == paused_)
		return false;
	if (!obs_output_pause(output_, paused))
		return false;

	const uint64_t now = os_gettime_ns();
	if (paused)
		pause_started_ns_ = now;
	else
		paused_ns_ += now - pause_started_ns_;
	paused_ = paused;
	return true;
}

bool Recorder::active() const
{
	return output_ && obs_output_active(output_);
}

RecorderStats Recorder::stats() const
{
	RecorderStats s;
	if (!output_)
		return s;

	s.bytes = obs_output_get_total_bytes(output_);
	s.dropped_frames = obs_output_get_frames_dropped(output_);
	s.total_frames = obs_output_get_total_frames(output_);

	const uint64_t now = os_gettime_ns();
	const uint64_t paused = paused_ns_ + (paused_ ? now - pause_started_ns_ : 0);
	if (start_ns_ && now > start_ns_ + paused)
		s.duration_ms = (now - start_ns_ - paused) / 1000000ULL;
	return s;
}

void Recorder::on_stop(void *param, calldata_t *cd)
{
	auto *self = static_cast<Recorder *>(param);
	const int code = (int)calldata_int(cd, "code");
	obs_output_t *output = static_cast<obs_output_t *>(calldata_ptr(cd, "output"));
	const char *last = output ? obs_output_get_last_error(output) : nullptr;
	std::string error = last ? last : "";
	std::weak_ptr<bool> alive = self->alive_;

	post_to_ui([self, alive, code, error]() {
		if (alive.expired() || !*alive.lock())
			return;
		self->paused_ = false;
		// Copied first: the owner may destroy this recorder from inside the callback.
		StoppedFn callback = self->on_stopped_;
		if (callback)
			callback(code, error);
	});
}

} // namespace selective_recording
