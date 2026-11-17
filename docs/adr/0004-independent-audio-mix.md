# 0004. Build an independent audio mix instead of using OBS tracks

- **Status:** Accepted, pending POC 2
- **Date:** 2026-11-17

## Context

OBS mixes audio into six tracks, and the user chooses which sources feed each track. A simpler design would point the recording at one of those tracks. But users often already have tracks set up for VOD audio or multitrack recording, and a single track can't express a separate route for each source without overwriting that setup.

## Decision

Capture each source with `obs_source_add_audio_capture_callback()`. Buffer the audio by timestamp, then mix the routed sources into a plugin-owned `audio_t` that feeds the recording's audio encoder.

## Consequences

- The user's track setup is never touched, and audio routing is truly per source.
- The plugin takes on mixing, resampling, clipping protection and A/V alignment itself.
- It needs to be verified which OBS mixer stages (volume fader, balance, sync offset, push-to-talk, filters) have already been applied when the callback runs. POC 2 must answer this. If some stages are missing, the mixer has to reproduce them, or the documentation has to state the limitation.
- Separate tracks (stems) for each source become possible later, without redesigning anything.
