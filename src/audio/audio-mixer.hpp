// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright (C) 2026 Aragami Korosu

#pragma once

#include <obs.hpp>

#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

namespace selective_recording {

// Builds the recording's audio from individual sources (ADR-0004).
//
// Each source is captured after its filters, then placed on a shared timeline, scaled by
// its OBS fader and recording gain, and mixed into a plugin-owned audio output. OBS's own
// tracks are never read or changed.
class AudioMixer {
public:
	struct Input {
		OBSSource source;
		float gain = 1.0f;
		bool include = true;
	};

	AudioMixer();
	~AudioMixer();
	AudioMixer(const AudioMixer &) = delete;
	AudioMixer &operator=(const AudioMixer &) = delete;

	bool start();
	void stop();
	audio_t *audio() const { return audio_; }

	// UI thread. Sources not listed are detached.
	void set_inputs(const std::vector<Input> &inputs);

	uint64_t latency_ms() const { return latency_ns_.load() / 1000000; }

private:
	struct SourceState;

	static void on_capture(void *param, obs_source_t *source, const audio_data *data, bool muted);
	static bool on_mix(void *param, uint64_t start_ts, uint64_t end_ts, uint64_t *new_ts, uint32_t active_mixers,
			   audio_output_data *mixes);
	bool mix(uint64_t end_ts, uint64_t *new_ts, audio_output_data *mixes);
	void raise_latency(uint64_t lag_ns);

	audio_t *audio_ = nullptr;
	uint32_t sample_rate_ = 48000;
	size_t channels_ = 2;

	std::mutex sources_mutex_;
	std::map<obs_source_t *, std::shared_ptr<SourceState>> sources_;

	// Mix thread state.
	bool started_ = false;
	uint64_t cursor_base_ = 0;
	uint64_t blocks_ = 0;
	std::vector<float> scratch_;
	std::atomic<uint64_t> latency_ns_{0};
};

} // namespace selective_recording
