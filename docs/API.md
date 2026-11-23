# Automation API

Selective Recording registers an [obs-websocket](https://github.com/obsproject/obs-websocket) vendor called **`selective-recording`**. Any obs-websocket 5 client (Stream Deck, Touch Portal, scripts) can call it with `CallVendorRequest`:

```json
{
  "requestType": "CallVendorRequest",
  "requestData": {
    "vendorName": "selective-recording",
    "requestType": "StartRecording",
    "requestData": { "canvas": "main" }
  }
}
```

Canvases are identified by key: `main` for OBS's main canvas, otherwise the canvas name, for example `Aitum Vertical`. Wherever `canvas` is optional, leaving it out means every canvas.

## Requests

| Request | Fields | Response |
| --- | --- | --- |
| `GetStatus` | — | `enabled`, `profile`, `canvases[]` (see below) |
| `StartRecording` | `canvas`? | `ok` |
| `StopRecording` | `canvas`? | `ok` |
| `PauseRecording` | `canvas`? | `ok` |
| `ResumeRecording` | `canvas`? | `ok` |
| `SetEnabled` | `enabled` | `ok` |
| `SetRoute` | `source_name` or `source_uuid`; `canvas`?; `video`?; `gain_db`? | `ok` |
| `GetRoute` | `source_name` or `source_uuid`; `canvas`? | `video`, `audio`, `gain_db` |
| `SetItemRoute` | `scene_name` or `scene_uuid`; `item_id`; `video`; `canvas`? | `ok` |
| `GetItemRoute` | `scene_name` or `scene_uuid`; `item_id`; `canvas`? | `video`, `folder`, `mixed` (folders), `locked`, `visible` |
| `SetCanvasSettings` | `canvas`; any of `enabled`, `output_directory`, `video_encoder`, `width`, `height`, `fps_divisor` | `ok` |
| `GetCanvasScenes` | `canvas` | `scenes[]` with `name` and `uuid` |
| `GetWarnings` | — | `warnings[]` |
| `GetInterruptedRecordings` | — | `recordings[]` with `id`, `canvas`, `path`, `started` |

Route values are `both`, `stream_only`, `recording_only` and `off`. An empty string clears the rule.

A source has one route for its video and audio ([ADR-0010](adr/0010-audio-follows-video.md)). `video` sets it; `audio` is accepted as another name for the same route, for older scripts. `GetRoute` returns the same value in both fields.

In `SetRoute`, leaving out `canvas` sets the rule for every canvas. Giving a canvas sets a per-canvas override. Like the dock's All scenes view, `recording_only` hides each placement of the source whose route it changes, so it leaves the stream, and moving off `recording_only` shows them again if the plugin hid them.

`SetItemRoute` sets one placement's video, exactly like clicking a Sources-list button ([ADR-0008](adr/0008-streamlabs-style-placements.md)):
- `recording_only` also hides the scene item in OBS, so it leaves the stream, and moving it off `recording_only` shows it again if the plugin hid it.
- It fails with `ok: false` for a locked scene item.
- On a **folder** (group), it sets every source inside, like a folder button; the folder itself has no setting. `GetItemRoute` on a folder returns its sources' route, with `mixed: true` when they differ.
- `SetRoute` on a folder is ignored: folders follow their eye icon.
- Use `scene_uuid` for scenes on other canvases or for a group's items (the group's UUID). `canvas` (default `main`) is the canvas the scene belongs to; it decides which per-canvas rule the placement falls back to.

`GetCanvasScenes` exists because obs-websocket looks scenes up by name only on the main canvas. Use the UUIDs it returns with obs-websocket's `sceneUuid` fields to work with scenes on other canvases.

### Canvas status

```json
{
  "canvas": "Aitum Vertical",
  "name": "Aitum Vertical",
  "state": "recording",
  "error": "",
  "file": "D:/Videos/2026-11-17 20-15-02 Aitum Vertical.mkv",
  "encoder": "x264",
  "preview": false,
  "duration_ms": 61234,
  "bytes": 104857600,
  "dropped_frames": 0,
  "total_frames": 1837,
  "audio_latency_ms": 64
}
```

`state` is one of `idle`, `recording`, `paused`, `stopping` or `failed`. When it's `failed`, `error` says why.

### Warnings

```json
{ "canvas": "main", "source_uuid": "…", "source_name": "Alerts", "video_live": true, "audio_live": false, "shared_track": false, "tracks": 0 }
```

`tracks` is a bitmask of the stream audio tracks the source is still on (bit 0 is track 1).

## Events

| Event | Data |
| --- | --- |
| `RecordingStateChanged` | `canvases[]`, the same shape as in `GetStatus` |

Subscribe to vendor events (`EventSubscription::Vendors`) to receive it.

## Hotkeys

Hotkeys can also be triggered with obs-websocket's `TriggerHotkeyByName`:

| Name | Action |
| --- | --- |
| `selective_recording.toggle.<canvas>` | Start or stop that canvas's recording |
| `selective_recording.pause.<canvas>` | Pause or resume it |
| `selective_recording.toggle_all` | Start or stop every canvas |
