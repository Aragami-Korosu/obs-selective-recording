# Configuration

> **Status:** proposed schema. It will be finalised in Phase 6.

## Storage

Files live in the plugin's config directory, which `obs_module_config_path()` returns. On Windows, that's:

```
%APPDATA%\obs-studio\plugin_config\obs-selective-recording\
├── settings.json        # Profiles, routes and recording preferences
└── sessions\            # One marker per active recording (see Recovery)
```

The plugin writes files atomically: it writes to a temporary file, then renames it over the original. That way a crash can't leave a half-written file behind.

## Schema

```json
{
  "schema_version": 1,
  "plugin_enabled": true,
  "active_profile": "default",
  "profiles": [
    {
      "id": "default",
      "name": "Default Recording",
      "scene_collection": null,

      "default_video_route": "both",
      "default_audio_route": "both",
      "source_routes": {
        "<source-uuid>": { "video": "both", "audio": "recording_only" }
      },
      "item_overrides": {
        "<scene-uuid>/<item-id>": { "video": "stream_only" }
      },

      "recording": {
        "container": "mkv",
        "audio_encoder": "aac",
        "audio_bitrate": 192,
        "audio_sample_rate": 48000,
        "filename_format": "%CCYY-%MM-%DD %hh-%mm-%ss",
        "offer_recovery": true
      },

      "warnings": {
        "live_visibility": true,
        "audio_tracks": true
      },

      "canvases": {
        "main": {
          "enabled": true,
          "source_routes": {},
          "recording": {
            "width": 0,
            "height": 0,
            "video_encoder": "auto",
            "video_quality": {},
            "output_directory": ""
          }
        },
        "Aitum Vertical": {
          "enabled": true,
          "source_routes": {
            "<source-uuid>": { "video": "off" }
          },
          "recording": {
            "width": 0,
            "height": 0,
            "video_encoder": "auto",
            "video_quality": {},
            "output_directory": "",
            "filename_format": "%CCYY-%MM-%DD %hh-%mm-%ss Vertical"
          }
        }
      }
    }
  ]
}
```

### What's shared and what's per canvas

| Level | Contents |
| --- | --- |
| **Profile** (shared by every canvas) | Default routes, source routes, item overrides, global recording preferences (`recording`), warning preferences |
| **Canvas** (`canvases.<name>`) | Whether it's enabled, source route overrides, resolution, video encoder and quality, destination. A canvas can also override any profile-level `recording` field, such as `filename_format` above. |

When the plugin builds a canvas's recording settings, it starts from the profile's `recording` object and applies the canvas's `recording` object on top.

### Route values

`both`, `stream_only`, `recording_only` and `off`. Any unknown value is treated as `both`.

A route controls what goes into the **recording**. For `recording_only` and `off`, it also tells the plugin to warn if the source is still live on that canvas. A route never changes OBS ([ADR-0003](adr/0003-stream-exclusion.md)).

### Fields

| Field | Notes |
| --- | --- |
| `schema_version` | Incremented on every breaking change. Migrations run in order on load. |
| `plugin_enabled` | When `false`, every recording includes every source. |
| `profiles[].scene_collection` | Optional. If set, the profile activates when that scene collection loads. |
| `source_routes` (profile level) | Keyed by source UUID, so routes survive renames. These apply on every canvas. |
| `item_overrides` | Keyed by the scene UUID plus the scene-item ID. Scene items belong to exactly one canvas, so no canvas key is needed. Either `video` or `audio` can be left out to inherit. |
| `recording.offer_recovery` | Offer to remux interrupted recordings at startup. |
| `warnings.*` | Turn off individual warning types (NFR-006, NFR-008). Both are on by default. |
| `canvases` | Keyed by canvas name. `main` is reserved for OBS's main canvas. Names are used instead of UUIDs because plugins like Vertical Canvas look up their canvas by a fixed name. |
| `canvases.*.enabled` | Whether this canvas has a selective recording pipeline. |
| `canvases.*.source_routes` | Per-canvas overrides of the profile-level `source_routes`. |
| `canvases.*.recording.width` / `height` | `0` means the canvas's base resolution, which follows the canvas if it changes. |
| `canvases.*.recording.video_encoder` | `auto` picks the best hardware encoder with free capacity, falling back to x264. |
| `canvases.*.recording.video_quality` | Settings passed to the chosen encoder (rate control, bitrate or CQ, preset). They're specific to that encoder. |
| `canvases.*.recording.output_directory` | An empty value means the OBS recording path is used. |

There's no frame-rate field. Recordings follow OBS's global frame rate, because canvases and views render on the main render loop. If POC 1 shows that a lower frame rate is possible, a field will be added.

## Resolution order

A source's effective route on a given canvas is the first match in this list:

1. `item_overrides` for this scene item
2. `canvases.<canvas>.source_routes` for this source
3. Profile-level `source_routes` for this source
4. The route inherited from the parent group or nested scene
5. The profile's `default_*_route`
6. `both`

## Recovery

Each active recording has a marker file, `sessions/<recording-id>.json`, where the ID is generated when the recording starts:

```json
{ "canvas": "Aitum Vertical", "path": "D:/Videos/2026-11-17 20-15-02 Vertical.mkv", "container": "mkv", "started": "2026-11-17T20:15:02+10:00" }
```

The plugin deletes the marker when the recording stops cleanly. Any marker left at startup means that recording was interrupted (see the [specification](SPECIFICATION.md#37-recovery)).

## Housekeeping

- Entries for sources that no longer exist are removed when a scene collection loads, but only after every source and canvas has finished loading. This stops a slow-loading collection, or a plugin canvas that appears late, from losing its routes.
- Settings for a canvas that is currently missing are kept, so uninstalling Vertical Canvas temporarily doesn't erase them (NFR-010).
- Import and export use the same profile object as above.
