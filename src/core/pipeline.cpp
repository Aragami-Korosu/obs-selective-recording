// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "core/pipeline.hpp"

#include "audio/audio-mixer.hpp"
#include "core/controller.hpp"
#include "core/obs-utils.hpp"
#include "recording/encoders.hpp"
#include "recording/recovery.hpp"
#include "video/scene-mirror.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <plugin-support.h>
#include <util/platform.h>

#include <cmath>

namespace selective_recording {

namespace {

// Refuse to start below this; the recording would fail within seconds.
constexpr uint64_t kMinFreeSpace = 200ULL * 1024 * 1024;
constexpr uint64_t kFallbackWindowNs = 5000000000ULL;

std::string describe_stop_code(int code, const std::string &detail)
{
	std::string message;
	switch (code) {
	case OBS_OUTPUT_NO_SPACE:
		message = obs_module_text("Error.NoSpace");
		break;
	case OBS_OUTPUT_ENCODE_ERROR:
		message = obs_module_text("Error.Encoder");
		break;
	case OBS_OUTPUT_UNSUPPORTED:
		message = obs_module_text("Error.Unsupported");
		break;
	default:
		message = obs_module_text("Error.Output");
		break;
	}
	if (!detail.empty())
		message += " (" + detail + ")";
	return message;
}

} // namespace

Pipeline::Pipeline(Controller &controller, obs_canvas_t *canvas, std::string key, std::string name)
	: controller_(controller),
	  canvas_(canvas),
	  key_(std::move(key)),
	  name_(std::move(name))
{
	canvas_remove_signal_.Connect(obs_canvas_get_signal_handler(canvas_), "remove", on_canvas_removed, this);
}

Pipeline::~Pipeline()
{
	*alive_ = false;
	stop_and_wait(5000);
	recorder_.reset();
	mixer_.reset();
	mirror_.reset();
	canvas_remove_signal_.Disconnect();
}

void Pipeline::on_canvas_removed(void *param, calldata_t *)
{
	auto *self = static_cast<Pipeline *>(param);
	std::weak_ptr<bool> alive = self->alive_;
	post_to_ui([self, alive]() {
		if (alive.expired() || !*alive.lock())
			return;
		self->controller_.refresh_canvases();
	});
}

obs_canvas_t *Pipeline::render_canvas() const
{
	return mirror_ ? mirror_->canvas() : nullptr;
}

bool Pipeline::visible_in_recording(obs_sceneitem_t *item) const
{
	const bool live_visible = obs_sceneitem_visible(item);
	if (!controller_.settings().enabled)
		return live_visible;

	const ResolvedRoute route = controller_.routes().resolve_video(key_, obs_sceneitem_get_source(item), item);
	switch (route.route) {
	case Route::Both:
		return live_visible;
	case Route::RecordingOnly:
		return true;
	case Route::StreamOnly:
	case Route::Off:
		return false;
	}
	return live_visible;
}

bool Pipeline::ensure_mirror(uint32_t width, uint32_t height)
{
	if (mirror_ && mirror_width_ == width && mirror_height_ == height)
		return mirror_->valid();

	// The output size is fixed for a canvas's lifetime, so a new size means a new mirror.
	if (mirror_ && recording())
		return mirror_->valid();

	mirror_.reset();
	mirror_ = std::make_unique<SceneMirror>(canvas_, width, height,
						[this](obs_sceneitem_t *item) { return visible_in_recording(item); });
	mirror_width_ = width;
	mirror_height_ = height;
	routes_revision_ = controller_.routes().revision();

	if (suspended_)
		mirror_->suspend();
	return mirror_->valid();
}

void Pipeline::release_unused()
{
	if (busy())
		return;
	recorder_.reset();
	mixer_.reset();
	if (!preview_)
		mirror_.reset();
}

void Pipeline::update_audio_inputs()
{
	if (!mixer_)
		return;

	const bool enabled = controller_.settings().enabled;
	std::vector<OBSSource> candidates = global_audio_sources();

	OBSSourceAutoRelease live = obs_canvas_get_channel(canvas_, 0);
	for (auto &source : active_tree(live)) {
		if (!contains_source(candidates, source))
			candidates.push_back(source);
	}
	if (mirror_) {
		for (auto &source : mirror_->active_sources()) {
			if (!contains_source(candidates, source))
				candidates.push_back(source);
		}
	}

	std::vector<AudioMixer::Input> inputs;
	for (auto &source : candidates) {
		if (!has_audio(source) || obs_source_is_scene(source) ||
		    obs_source_get_type(source) == OBS_SOURCE_TYPE_TRANSITION)
			continue;

		AudioMixer::Input input;
		input.source = source;

		if (!enabled) {
			input.include = !is_monitor_only(source);
		} else {
			const ResolvedRoute route = controller_.routes().resolve_audio(key_, source);
			input.include = in_recording(route.route);
			// Monitor-only sources are left out of OBS's mix, so they stay out unless asked for.
			if (route.origin == RouteOrigin::ProfileDefault && is_monitor_only(source))
				input.include = false;
			input.gain = (float)std::pow(10.0, controller_.routes().gain_db(key_, source) / 20.0);
		}
		inputs.push_back(std::move(input));
	}
	mixer_->set_inputs(inputs);
}

std::string Pipeline::make_path(std::string &error) const
{
	const Settings &settings = controller_.settings();
	const CanvasSettings &cs = controller_.settings().canvas(key_);

	std::string dir = cs.output_directory;
	if (dir.empty()) {
		char *path = obs_frontend_get_current_record_output_path();
		dir = path ? path : "";
		bfree(path);
	}
	if (dir.empty()) {
		error = obs_module_text("Error.NoDirectory");
		return "";
	}

	if (os_mkdirs(dir.c_str()) == MKDIR_ERROR) {
		error = std::string(obs_module_text("Error.Directory")) + " " + dir;
		return "";
	}

	const uint64_t free_space = os_get_free_disk_space(dir.c_str());
	if (free_space && free_space < kMinFreeSpace) {
		error = obs_module_text("Error.NoSpace");
		return "";
	}

	std::string format = settings.recording.filename_format;
	std::string suffix = cs.filename_suffix;
	if (suffix.empty() && key_ != kMainCanvasKey)
		suffix = " " + name_;
	format += suffix;

	const ContainerInfo container = container_info(settings.recording.container);
	char *name = os_generate_formatted_filename(container.extension.c_str(), true, format.c_str());
	std::string path = dir + "/" + (name ? name : "recording." + container.extension);
	bfree(name);

	// Never overwrite an existing file.
	if (os_file_exists(path.c_str())) {
		const std::string stem = path.substr(0, path.size() - container.extension.size() - 1);
		for (int i = 2; i < 1000; i++) {
			std::string candidate = stem + " (" + std::to_string(i) + ")." + container.extension;
			if (!os_file_exists(candidate.c_str()))
				return candidate;
		}
	}
	return path;
}

bool Pipeline::start_with_encoder(const std::string &encoder_id, std::string &error)
{
	const Settings &settings = controller_.settings();
	const CanvasSettings &cs = controller_.settings().canvas(key_);
	const ContainerInfo container = container_info(settings.recording.container);

	RecorderConfig config;
	config.name = "Selective Recording (" + name_ + ")";
	config.path = make_path(error);
	if (config.path.empty())
		return false;

	config.output_id = container.output_id;
	config.video_encoder = encoder_id;
	OBSDataAutoRelease video_settings = video_encoder_settings(encoder_id, cs.rate_control, cs.quality, cs.bitrate);
	config.video_settings = video_settings.Get();
	config.fps_divisor = cs.fps_divisor;
	config.audio_encoder = settings.recording.audio_encoder;
	OBSDataAutoRelease audio_settings = audio_encoder_settings(settings.recording.audio_bitrate);
	config.audio_settings = audio_settings.Get();

	std::weak_ptr<bool> alive = alive_;
	recorder_ = std::make_unique<Recorder>([this, alive](int code, const std::string &detail) {
		if (alive.expired() || !*alive.lock())
			return;
		on_recorder_stopped(code, detail);
	});

	if (!recorder_->start(mirror_->video(), mixer_->audio(), config, error)) {
		recorder_.reset();
		return false;
	}

	encoder_id_ = encoder_id;
	const char *display = obs_encoder_get_display_name(encoder_id.c_str());
	encoder_name_ = display ? display : encoder_id;
	session_id_ = recovery::begin(key_, config.path, settings.recording.container);
	return true;
}

bool Pipeline::start()
{
	if (busy())
		return true;

	last_error_.clear();
	fallback_used_ = false;
	const CanvasSettings &cs = controller_.settings().canvas(key_);

	try {
		if (!ensure_mirror(cs.width, cs.height)) {
			fail(obs_module_text("Error.Canvas"));
			return false;
		}

		mixer_ = std::make_unique<AudioMixer>();
		if (!mixer_->start()) {
			fail(obs_module_text("Error.Audio"));
			return false;
		}
		update_audio_inputs();

		std::string encoder = resolve_video_encoder(cs.video_encoder);
		std::string error;
		bool started = start_with_encoder(encoder, error);

		// Hardware encoders can run out of sessions with dual streaming (NFR-007).
		if (!started && is_hardware_encoder(encoder) && controller_.settings().recording.fallback_to_software) {
			obs_log(LOG_WARNING, "[%s] encoder '%s' failed to start (%s); retrying with x264", key_.c_str(),
				encoder.c_str(), error.c_str());
			fallback_used_ = true;
			encoder = kSoftwareEncoder;
			error.clear();
			started = start_with_encoder(encoder, error);
		}

		if (!started) {
			fail(error.empty() ? obs_module_text("Error.Output") : error);
			return false;
		}
	} catch (const std::exception &e) {
		fail(e.what());
		return false;
	}

	started_ns_ = os_gettime_ns();
	state_ = PipelineState::Recording;
	obs_log(LOG_INFO, "[%s] recording started: %s (%s)", key_.c_str(), recorder_->path().c_str(),
		encoder_id_.c_str());
	notify();
	return true;
}

void Pipeline::stop()
{
	if (!recording() || !recorder_)
		return;
	state_ = PipelineState::Stopping;
	recorder_->stop();
	notify();
}

void Pipeline::stop_and_wait(uint32_t timeout_ms)
{
	if (!recorder_ || !busy())
		return;
	recorder_->stop();
	recorder_->wait_stopped(timeout_ms);
	recovery::end(session_id_);
	session_id_.clear();
	state_ = PipelineState::Idle;
}

bool Pipeline::set_paused(bool paused)
{
	if (!recording() || !recorder_ || !recorder_->set_paused(paused))
		return false;
	state_ = paused ? PipelineState::Paused : PipelineState::Recording;
	notify();
	return true;
}

void Pipeline::clear_error()
{
	if (state_ == PipelineState::Failed) {
		state_ = PipelineState::Idle;
		last_error_.clear();
		notify();
	}
}

void Pipeline::fail(const std::string &message)
{
	obs_log(LOG_ERROR, "[%s] %s", key_.c_str(), message.c_str());
	last_error_ = message;
	state_ = PipelineState::Failed;
	if (recorder_ && recorder_->active())
		recorder_->stop();
	recovery::end(session_id_);
	session_id_.clear();
	recorder_.reset();
	mixer_.reset();
	if (!preview_)
		mirror_.reset();
	notify();
}

void Pipeline::on_recorder_stopped(int code, const std::string &error)
{
	recovery::end(session_id_);
	session_id_.clear();

	if (code == OBS_OUTPUT_SUCCESS) {
		obs_log(LOG_INFO, "[%s] recording saved: %s", key_.c_str(), recorder_ ? recorder_->path().c_str() : "");
		state_ = PipelineState::Idle;
		release_unused();
		notify();
		return;
	}

	// An encoder that fails right after starting is usually out of sessions; retry once in software.
	const bool early = os_gettime_ns() - started_ns_ < kFallbackWindowNs;
	if (code == OBS_OUTPUT_ENCODE_ERROR && early && !fallback_used_ && is_hardware_encoder(encoder_id_) &&
	    controller_.settings().recording.fallback_to_software) {
		obs_log(LOG_WARNING, "[%s] encoder '%s' failed; retrying with x264", key_.c_str(), encoder_id_.c_str());
		fallback_used_ = true;
		recorder_.reset();
		std::string retry_error;
		if (start_with_encoder(kSoftwareEncoder, retry_error)) {
			started_ns_ = os_gettime_ns();
			state_ = PipelineState::Recording;
			notify();
			return;
		}
	}

	fail(describe_stop_code(code, error));
}

void Pipeline::set_preview(bool enabled)
{
	if (preview_ == enabled)
		return;
	preview_ = enabled;

	if (enabled) {
		const CanvasSettings &cs = controller_.settings().canvas(key_);
		ensure_mirror(cs.width, cs.height);
	} else {
		release_unused();
	}
	notify();
}

void Pipeline::update()
{
	if (!mirror_)
		return;

	if (routes_revision_ != controller_.routes().revision()) {
		routes_revision_ = controller_.routes().revision();
		mirror_->refresh_visibility();
	}
	mirror_->reconcile();
	update_audio_inputs();
}

void Pipeline::routes_changed()
{
	if (!mirror_)
		return;
	routes_revision_ = controller_.routes().revision();
	mirror_->refresh_visibility();
	update_audio_inputs();
}

void Pipeline::suspend()
{
	suspended_ = true;
	if (mirror_)
		mirror_->suspend();
	if (mixer_)
		mixer_->set_inputs({});
}

void Pipeline::resume()
{
	suspended_ = false;
	if (mirror_)
		mirror_->resume();
	update_audio_inputs();
}

RecorderStats Pipeline::stats() const
{
	return recorder_ ? recorder_->stats() : RecorderStats{};
}

std::string Pipeline::file_path() const
{
	return recorder_ ? recorder_->path() : "";
}

uint64_t Pipeline::audio_latency_ms() const
{
	return mixer_ ? mixer_->latency_ms() : 0;
}

void Pipeline::notify()
{
	controller_.pipeline_changed(key_);
}

} // namespace selective_recording
