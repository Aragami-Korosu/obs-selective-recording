# 0004. Build an independent audio mix instead of using OBS tracks

- **Status:** Accepted. POC 2 is complete; see "Findings" below.
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

## Findings (POC 2)

From reading `source_output_audio_data()` in libobs and from measurements:

| OBS mixer stage | Already applied when the callback runs? | What the plugin does |
| --- | --- | --- |
| Audio filters | Yes | Nothing |
| Resampling to the OBS format | Yes | Nothing |
| Balance and force-mono | Yes | Nothing |
| Mute, push-to-talk, push-to-mute | Reported as the `muted` flag | Writes silence while muted |
| Volume fader | No | Multiplies by `obs_source_get_volume()` at mix time |
| Sync offset | No | Adds `obs_source_get_sync_offset()` to the timestamp |
| Timestamp smoothing and re-anchoring | No (raw timestamps are passed) | Re-implements the libobs rules (2 s direct threshold, 70 ms smoothing) |
| Monitor Only (OBS 31–32) | Excluded from OBS's mix | Excluded unless the route is set explicitly |

The mix runs on its own `audio_t` and trails real time by a buffer that starts at 64 ms and only grows when a source arrives late, as OBS's own mixer does. In tests:

- Audio routed Off measures −102 dB in the recording.
- The A/V offset is within 10 ms of OBS's own recording of the same media.
- No drift was measured over a long recording.

Known differences from OBS's mix: scene transitions don't crossfade audio, and item show/hide fades aren't applied. Audio follows routes immediately.
