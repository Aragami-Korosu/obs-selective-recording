// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#include "audio/audio-mixer.hpp"

#include <plugin-support.h>
#include <util/platform.h>
#include <util/util_uint64.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace selective_recording {

namespace {

// Mirrors the thresholds libobs uses for audio timing.
constexpr uint64_t kMaxTsVar = 2000000000ULL;
constexpr uint64_t kSmoothingThreshold = 70000000ULL;
constexpr uint64_t kMinLatency = 64000000ULL;
constexpr uint64_t kMaxLatency = 1000000000ULL;
constexpr size_t kBufferSeconds = 3;

uint64_t frames_to_ns(uint32_t rate, uint64_t frames)
{
	return util_mul_div64(frames, 1000000000ULL, rate);
}

int64_t ns_to_frames(uint32_t rate, int64_t ns)
{
	const bool negative = ns < 0;
	const uint64_t magnitude = (uint64_t)(negative ? -ns : ns);
	const int64_t frames = (int64_t)util_mul_div64(magnitude, rate, 1000000000ULL);
	return negative ? -frames : frames;
}

uint64_t diff(uint64_t a, uint64_t b)
{
	return a > b ? a - b : b - a;
}

// Keeps peaks under full scale without the harsh edge of hard clipping.
float soft_limit(float x)
{
	constexpr float knee = 0.9f;
	const float magnitude = std::fabs(x);
	if (magnitude <= knee)
		return x;
	const float over = (magnitude - knee) / (1.0f - knee);
	const float limited = knee + (1.0f - knee) * std::tanh(over);
	return std::copysign(limited, x);
}

} // namespace

// Audio for one source, placed on the shared timeline by timestamp.
struct AudioMixer::SourceState {
	OBSSource source;
	AudioMixer *owner = nullptr;
	std::atomic<float> gain{1.0f};
	std::atomic<bool> include{true};

	std::mutex mutex;
	std::vector<std::vector<float>> buffer;
	size_t capacity = 0;
	size_t head = 0;
	size_t size = 0;
	uint64_t base_ts = 0;
	uint64_t consumed = 0;

	bool timing_set = false;
	uint64_t timing_adjust = 0;
	uint64_t next_ts = 0;

	uint64_t start_ts(uint32_t rate) const { return base_ts + frames_to_ns(rate, consumed); }

	void reset(uint64_t ts)
	{
		base_ts = ts;
		consumed = 0;
		head = 0;
		size = 0;
	}

	void write(uint32_t rate, uint64_t ts, const float *const *data, size_t channels, size_t frames, bool silent)
	{
		if (size == 0)
			reset(ts);

		int64_t offset = ns_to_frames(rate, (int64_t)(ts - start_ts(rate)));
		size_t skip = 0;
		if (offset < 0) {
			if ((size_t)-offset >= frames)
				return;
			skip = (size_t)-offset;
			offset = 0;
		}

		if ((size_t)offset + frames > capacity) {
			reset(ts);
			offset = 0;
			skip = 0;
		}

		for (size_t i = size; i < (size_t)offset; i++) {
			for (size_t ch = 0; ch < channels; ch++)
				buffer[ch][(head + i) % capacity] = 0.0f;
		}

		for (size_t i = skip; i < frames; i++) {
			const size_t pos = (head + (size_t)offset + i - skip) % capacity;
			for (size_t ch = 0; ch < channels; ch++)
				buffer[ch][pos] = (silent || !data[ch]) ? 0.0f : data[ch][i];
		}

		size = std::max(size, (size_t)offset + frames - skip);
	}

	// Copies one window into out (zero where nothing was captured) and drops everything before its end.
	void read(uint32_t rate, uint64_t window_ts, float *const *out, size_t channels, size_t frames)
	{
		const int64_t offset = ns_to_frames(rate, (int64_t)(window_ts - start_ts(rate)));

		for (size_t i = 0; i < frames; i++) {
			const int64_t idx = offset + (int64_t)i;
			const bool have = idx >= 0 && (size_t)idx < size;
			for (size_t ch = 0; ch < channels; ch++)
				out[ch][i] = have ? buffer[ch][(head + (size_t)idx) % capacity] : 0.0f;
		}

		const int64_t drop = std::clamp<int64_t>(offset + (int64_t)frames, 0, (int64_t)size);
		head = (head + (size_t)drop) % capacity;
		size -= (size_t)drop;
		consumed += (uint64_t)drop;
	}
};

AudioMixer::AudioMixer()
{
	obs_audio_info oai = {};
	if (obs_get_audio_info(&oai)) {
		sample_rate_ = oai.samples_per_sec;
		channels_ = get_audio_channels(oai.speakers);
	}
}

AudioMixer::~AudioMixer()
{
	stop();
	set_inputs({});
}

bool AudioMixer::start()
{
	if (audio_)
		return true;

	obs_audio_info oai = {};
	if (!obs_get_audio_info(&oai))
		return false;

	sample_rate_ = oai.samples_per_sec;
	channels_ = get_audio_channels(oai.speakers);
	scratch_.assign(channels_ * AUDIO_OUTPUT_FRAMES, 0.0f);
	started_ = false;
	blocks_ = 0;
	latency_ns_ = kMinLatency;

	audio_output_info info = {};
	info.name = "selective-recording";
	info.samples_per_sec = oai.samples_per_sec;
	info.format = AUDIO_FORMAT_FLOAT_PLANAR;
	info.speakers = oai.speakers;
	info.input_callback = on_mix;
	info.input_param = this;

	if (audio_output_open(&audio_, &info) != AUDIO_OUTPUT_SUCCESS) {
		obs_log(LOG_ERROR, "could not open the recording audio output");
		audio_ = nullptr;
		return false;
	}
	return true;
}

void AudioMixer::stop()
{
	if (audio_) {
		audio_output_close(audio_);
		audio_ = nullptr;
	}
}

void AudioMixer::set_inputs(const std::vector<Input> &inputs)
{
	std::vector<std::shared_ptr<SourceState>> added;
	std::vector<std::shared_ptr<SourceState>> removed;

	{
		std::lock_guard lock(sources_mutex_);
		std::map<obs_source_t *, std::shared_ptr<SourceState>> next;

		for (const Input &input : inputs) {
			obs_source_t *raw = input.source.Get();
			if (!raw || next.count(raw))
				continue;

			auto it = sources_.find(raw);
			std::shared_ptr<SourceState> state;
			if (it != sources_.end()) {
				state = it->second;
			} else {
				state = std::make_shared<SourceState>();
				state->source = input.source;
				state->owner = this;
				state->capacity = sample_rate_ * kBufferSeconds;
				state->buffer.assign(channels_, std::vector<float>(state->capacity, 0.0f));
				added.push_back(state);
			}
			state->gain = input.gain;
			state->include = input.include;
			next[raw] = state;
		}

		for (auto &[raw, state] : sources_) {
			if (!next.count(raw))
				removed.push_back(state);
		}
		sources_ = std::move(next);
	}

	// Callbacks are added and removed outside our lock; libobs serialises them per source.
	for (auto &state : added)
		obs_source_add_audio_capture_callback(state->source, on_capture, state.get());
	for (auto &state : removed)
		obs_source_remove_audio_capture_callback(state->source, on_capture, state.get());
}

void AudioMixer::raise_latency(uint64_t lag_ns)
{
	const uint64_t block = frames_to_ns(sample_rate_, AUDIO_OUTPUT_FRAMES);
	const uint64_t wanted = std::min(kMaxLatency, lag_ns + block);
	uint64_t current = latency_ns_.load();
	while (wanted > current && !latency_ns_.compare_exchange_weak(current, wanted)) {
	}
}

void AudioMixer::on_capture(void *param, obs_source_t *source, const audio_data *data, bool muted)
{
	auto *state = static_cast<SourceState *>(param);
	AudioMixer *self = state->owner;
	if (!data || !data->frames)
		return;

	const uint32_t rate = self->sample_rate_;
	const uint64_t now = os_gettime_ns();
	const uint64_t duration = frames_to_ns(rate, data->frames);
	uint64_t ts = data->timestamp;

	std::lock_guard lock(state->mutex);

	// Same rules libobs applies before mixing: trust timestamps close to the system clock,
	// re-anchor others to it, and smooth small jitter so audio stays contiguous.
	const bool direct = diff(ts, now) < kMaxTsVar;
	if (direct) {
		state->timing_adjust = 0;
		state->timing_set = true;
	} else if (!state->timing_set || (state->next_ts && diff(state->next_ts, ts) > kMaxTsVar)) {
		state->timing_adjust = now - ts;
		state->timing_set = true;
	}

	if (state->next_ts && diff(state->next_ts, ts) < kSmoothingThreshold)
		ts = state->next_ts;
	state->next_ts = ts + duration;

	const uint64_t placed = ts + state->timing_adjust + (uint64_t)obs_source_get_sync_offset(source);
	state->write(rate, placed, reinterpret_cast<const float *const *>(data->data), self->channels_, data->frames,
		     muted);

	if (now > placed + duration)
		self->raise_latency(now - (placed + duration));
}

bool AudioMixer::on_mix(void *param, uint64_t, uint64_t end_ts, uint64_t *new_ts, uint32_t, audio_output_data *mixes)
{
	return static_cast<AudioMixer *>(param)->mix(end_ts, new_ts, mixes);
}

bool AudioMixer::mix(uint64_t end_ts, uint64_t *new_ts, audio_output_data *mixes)
{
	const uint64_t block = frames_to_ns(sample_rate_, AUDIO_OUTPUT_FRAMES);
	const uint64_t latency = latency_ns_.load();

	if (!started_) {
		cursor_base_ = end_ts - latency - block;
		blocks_ = 0;
		started_ = true;
	}

	// Output windows are contiguous and trail real time by the current latency. Skipping a
	// tick is how latency grows, exactly as libobs buffers its own mix.
	const uint64_t cursor = cursor_base_ + frames_to_ns(sample_rate_, blocks_ * AUDIO_OUTPUT_FRAMES);
	const uint64_t window_end = cursor_base_ + frames_to_ns(sample_rate_, (blocks_ + 1) * AUDIO_OUTPUT_FRAMES);
	if (end_ts < window_end + latency)
		return false;

	std::vector<std::shared_ptr<SourceState>> sources;
	{
		std::lock_guard lock(sources_mutex_);
		sources.reserve(sources_.size());
		for (auto &[raw, state] : sources_)
			sources.push_back(state);
	}

	float *window[MAX_AUDIO_CHANNELS] = {};
	for (size_t ch = 0; ch < channels_; ch++)
		window[ch] = scratch_.data() + ch * AUDIO_OUTPUT_FRAMES;

	float *out[MAX_AUDIO_CHANNELS] = {};
	for (size_t ch = 0; ch < channels_; ch++)
		out[ch] = mixes[0].data[ch];

	for (auto &state : sources) {
		{
			std::lock_guard lock(state->mutex);
			state->read(sample_rate_, cursor, window, channels_, AUDIO_OUTPUT_FRAMES);
		}
		if (!state->include || !out[0])
			continue;

		const float volume = obs_source_get_volume(state->source) * state->gain.load();
		if (volume <= 0.0f)
			continue;

		for (size_t ch = 0; ch < channels_; ch++) {
			if (!out[ch])
				continue;
			for (size_t i = 0; i < AUDIO_OUTPUT_FRAMES; i++)
				out[ch][i] += window[ch][i] * volume;
		}
	}

	for (size_t ch = 0; ch < channels_; ch++) {
		if (!out[ch])
			continue;
		for (size_t i = 0; i < AUDIO_OUTPUT_FRAMES; i++)
			out[ch][i] = soft_limit(out[ch][i]);
	}

	*new_ts = cursor;
	blocks_++;
	return true;
}

} // namespace selective_recording
