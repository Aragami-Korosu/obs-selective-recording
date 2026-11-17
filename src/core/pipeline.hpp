// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include "recording/recorder.hpp"

#include <obs.hpp>

#include <memory>
#include <string>

namespace selective_recording {

class AudioMixer;
class Controller;
class SceneMirror;

enum class PipelineState { Idle, Recording, Paused, Stopping, Failed };

// The selective recording for one canvas: scene mirror, audio mix and recorder.
// Each pipeline owns everything it uses, so a failure here never reaches another
// canvas or any stream (NFR-002, NFR-003). UI thread only.
class Pipeline {
public:
	Pipeline(Controller &controller, obs_canvas_t *canvas, std::string key, std::string name);
	~Pipeline();
	Pipeline(const Pipeline &) = delete;
	Pipeline &operator=(const Pipeline &) = delete;

	const std::string &key() const { return key_; }
	const std::string &name() const { return name_; }
	obs_canvas_t *canvas() const { return canvas_; }

	PipelineState state() const { return state_; }
	const std::string &last_error() const { return last_error_; }
	bool recording() const { return state_ == PipelineState::Recording || state_ == PipelineState::Paused; }
	bool busy() const { return recording() || state_ == PipelineState::Stopping; }

	bool start();
	void stop();
	bool set_paused(bool paused);
	void stop_and_wait(uint32_t timeout_ms);
	void clear_error();

	void set_preview(bool enabled);
	bool preview() const { return preview_; }
	obs_canvas_t *render_canvas() const;

	void update();
	void routes_changed();
	void suspend();
	void resume();

	RecorderStats stats() const;
	std::string file_path() const;
	uint64_t audio_latency_ms() const;
	std::string encoder_name() const { return encoder_name_; }

private:
	bool ensure_mirror(uint32_t width, uint32_t height);
	void release_unused();
	void update_audio_inputs();
	bool visible_in_recording(obs_sceneitem_t *item) const;
	bool start_with_encoder(const std::string &encoder_id, std::string &error);
	void on_recorder_stopped(int code, const std::string &error);
	void fail(const std::string &message);
	std::string make_path(std::string &error) const;
	void notify();

	static void on_canvas_removed(void *param, calldata_t *cd);

	Controller &controller_;
	OBSCanvas canvas_;
	std::string key_;
	std::string name_;

	std::unique_ptr<SceneMirror> mirror_;
	std::unique_ptr<AudioMixer> mixer_;
	std::unique_ptr<Recorder> recorder_;

	PipelineState state_ = PipelineState::Idle;
	std::string last_error_;
	std::string session_id_;
	std::string encoder_id_;
	std::string encoder_name_;
	uint64_t started_ns_ = 0;
	bool fallback_used_ = false;
	bool preview_ = false;
	bool suspended_ = false;
	uint32_t mirror_width_ = 0;
	uint32_t mirror_height_ = 0;
	uint64_t routes_revision_ = 0;

	OBSSignal canvas_remove_signal_;
	std::shared_ptr<bool> alive_ = std::make_shared<bool>(true);
};

} // namespace selective_recording
