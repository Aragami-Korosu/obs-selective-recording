# Configuration

Everything can be changed from the dock and the Settings dialog. This page documents the files behind them, for support, backups and automation.

## Where settings live

| Data | Location | Why |
| --- | --- | --- |
| Routing profiles | Inside each scene collection, under `selective-recording` | Routes refer to source UUIDs, which belong to a collection ([ADR-0007](adr/0007-routes-in-scene-collection.md)) |
| Recording and warning settings | `settings.json` in the plugin config folder | About the machine, not the scenes |
| Global hotkey | `hotkeys.json` in the plugin config folder | |
| Per-canvas hotkeys | Inside `settings.json` | |
| Recovery markers | `sessions/` in the plugin config folder | One per active recording |

The plugin config folder is `%APPDATA%\obs-studio\plugin_config\obs-selective-recording\` on Windows, or `config/obs-studio/plugin_config/obs-selective-recording/` in a portable OBS. Files are written atomically and keep a `.bak` copy.

## Routing profiles (scene collection)

```json
"selective-recording": {
  "schema_version": 1,
  "active_profile": "default",
  "profiles": [
    {
      "id": "default",
      "name": "Default",
      "default_video_route": "both",
      "default_audio_route": "both",
      "source_routes": {
        "<source-uuid>": { "video": "stream_only", "audio": "off", "gain_db": -6.0 }
      },
      "canvases": {
        "Aitum Vertical": {
          "source_routes": { "<source-uuid>": { "video": "off" } }
        }
      },
      "item_overrides": {
        "<scene-uuid>/<item-id>": { "video": "recording_only" }
      }
    }
  ]
}
```

### Route values

| Value | In the recording | Stream (set by you in OBS) |
| --- | --- | --- |
| `both` | Included | Unchanged |
| `stream_only` | Excluded | Unchanged |
| `recording_only` | Included, even when hidden live | You hide it; the plugin warns if it's still live |
| `off` | Excluded | You hide it; the plugin warns if it's still live |

Setting a route in the dock's Advanced view or with `SetRoute` never changes OBS ([ADR-0003](adr/0003-stream-exclusion.md)). Any unknown value is treated as `both`.

### Where the Streamlabs-style buttons save

The Sources-list buttons, the dock's simple view and `SetItemRoute` save to `item_overrides`, one entry per placement (`"<scene-uuid>/<item-id>"`). An entry is only kept where the placement differs from what its source would get anyway. To find the key, take the scene's `"uuid"` in the collection file, then the item's `"id"` from that scene's `"settings"` → `"items"`. For a source inside a group, use the group's `"uuid"`.

Choosing Recording only also hides the scene item in OBS ([ADR-0008](adr/0008-streamlabs-style-placements.md)). The plugin marks it with `"selective_recording_hidden": true` in the item's `"private_settings"`, so it only ever shows items again that it hid itself.

A placement with a route also gets `"selective_recording_placement": "<scene-uuid>/<item-id>"` in its `"private_settings"`. OBS copies private settings when it duplicates a scene (including Studio Mode's on-air copy), so the copy uses the original's setting until it's given its own.

Edit the collection file only while OBS is closed: OBS rewrites it when it saves and when it exits.

### Resolution order

The first match wins:

1. `item_overrides` for this placement (video only)
2. `canvases.<canvas>.source_routes` for this source
3. `source_routes` for this source
4. The profile's `default_*_route`
5. `both`

Groups and nested scenes pass routes down naturally. A hidden parent hides its children in the recording, and a Recording only parent shows them.

`gain_db` adds gain to the recording only, from −30 to +20 dB. The OBS volume fader is always applied as well.

## settings.json

```json
{
  "schema_version": 1,
  "plugin_enabled": true,
  "offer_recovery": true,
  "dock_introduced": true,
  "recording": {
    "container": "mkv",
    "audio_encoder": "ffmpeg_aac",
    "audio_bitrate": 192,
    "filename_format": "%CCYY-%MM-%DD %hh-%mm-%ss",
    "fallback_to_software": true
  },
  "warnings": { "live_visibility": true, "audio_tracks": true },
  "canvases": {
    "main": {
      "enabled": true,
      "width": 0, "height": 0,
      "fps_divisor": 1,
      "video_encoder": "auto",
      "rate_control": "quality",
      "quality": 20,
      "bitrate": 12000,
      "output_directory": "",
      "filename_suffix": "",
      "hotkeys": { "toggle": [], "pause": [] }
    }
  }
}
```

| Field | Meaning |
| --- | --- |
| `plugin_enabled` | When `false`, recordings include every source exactly as it appears live |
| `offer_recovery` | Check for interrupted recordings at startup |
| `recording.container` | `mkv` (recommended), `mp4` or `mov` |
| `recording.audio_encoder` | `ffmpeg_aac` or `ffmpeg_opus` |
| `recording.filename_format` | The same codes as OBS's filename format |
| `recording.fallback_to_software` | Retry with x264 if a hardware encoder can't start |
| `canvases` | Keyed by canvas: `main` for OBS's main canvas, otherwise the canvas name (for example `Aitum Vertical`) |
| `width`, `height` | `0` means the canvas's base resolution |
| `fps_divisor` | `1` = OBS's frame rate, `2` = half, `3` = one third, `4` = one quarter |
| `video_encoder` | `auto`, or an encoder ID such as `obs_x264`, `obs_nvenc_h264_tex` or `h264_texture_amf` |
| `rate_control` | `quality` (CRF or CQP) or `bitrate` (CBR) |
| `quality` | 1–51. Lower is better quality and bigger files |
| `bitrate` | In kbps, used when `rate_control` is `bitrate` |
| `output_directory` | Empty means OBS's recording folder |
| `filename_suffix` | Added to the filename. Empty means the canvas name, for canvases other than main |

Settings for a canvas that's currently missing are kept, so uninstalling Vertical Canvas temporarily doesn't erase them (NFR-010).

## Recovery markers

`sessions/<id>.json` is written when a recording starts and deleted when it stops cleanly:

```json
{ "canvas": "main", "path": "D:/Videos/2026-11-17 20-15-02.mkv", "container": "mkv", "started": "2026-11-17T20:15:02" }
```

A marker that's still there at startup means the recording was interrupted. The dock lists it until you dismiss it.
