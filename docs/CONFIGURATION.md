# Configuration

> **Status:** proposed schema. It will be finalised in Phase 6.

## Storage

Settings are stored as JSON in the plugin's config directory, which `obs_module_config_path()` returns. On Windows, that's:

```
%APPDATA%\obs-studio\plugin_config\obs-selective-recording\
```

The plugin writes the file atomically: it writes to a temporary file, then renames it over the original. That way a crash can't leave a half-written file behind.

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
      "canvases": {
        "main": {
          "enabled": true,
          "source_routes": {},
          "recording": {
            "container": "mkv",
            "width": 1920,
            "height": 1080,
            "video_encoder": "auto",
            "audio_encoder": "aac",
            "audio_bitrate": 192,
            "audio_sample_rate": 48000,
            "output_directory": "",
            "filename_format": "%CCYY-%MM-%DD %hh-%mm-%ss"
          }
        },
        "Aitum Vertical": {
          "enabled": true,
          "source_routes": {
            "<source-uuid>": { "video": "off" }
          },
          "recording": {
            "container": "mkv",
            "width": 0,
            "height": 0,
            "video_encoder": "auto",
            "audio_encoder": "aac",
            "audio_bitrate": 192,
            "audio_sample_rate": 48000,
            "output_directory": "",
            "filename_format": "%CCYY-%MM-%DD %hh-%mm-%ss Vertical"
          }
        }
      }
    }
  ]
}
```

### Route values

`both`, `stream_only`, `recording_only` and `off`. Any unknown value is treated as `both`.

### Fields

| Field | Notes |
| --- | --- |
| `schema_version` | Incremented on every breaking change. Migrations run in order on load. |
| `plugin_enabled` | When `false`, every recording includes every source. |
| `profiles[].scene_collection` | Optional. If set, the profile activates when that scene collection loads. |
| `source_routes` (profile level) | Keyed by source UUID, so routes survive renames. These apply on every canvas. |
| `item_overrides` | Keyed by the scene UUID plus the scene-item ID. Scene items belong to exactly one canvas, so no canvas key is needed. Either `video` or `audio` can be left out to inherit. |
| `canvases` | Keyed by canvas name. `main` is reserved for OBS's main canvas. Names are used instead of UUIDs because plugins like Vertical Canvas look up their canvas by a fixed name. |
| `canvases.*.enabled` | Whether this canvas has a selective recording pipeline. |
| `canvases.*.source_routes` | Per-canvas overrides of the profile-level `source_routes`. |
| `canvases.*.recording.width` / `height` | `0` means the canvas's base resolution, which follows the canvas if it changes. |
| `recording.video_encoder` | `auto` picks the best hardware encoder available, falling back to x264. |
| `recording.output_directory` | An empty value means the OBS recording path is used. |

There's no frame-rate field. Recordings follow OBS's global frame rate, because canvases and views render on the main render loop. If POC 1 shows that a lower frame rate is possible, a field will be added.

## Resolution order

A source's effective route on a given canvas is the first match in this list:

1. `item_overrides` for this scene item
2. `canvases.<canvas>.source_routes` for this source
3. Profile-level `source_routes` for this source
4. The route inherited from the parent group or nested scene
5. The profile's `default_*_route`
6. `both`

## Housekeeping

- Entries for sources that no longer exist are removed when a scene collection loads, but only after every source and canvas has finished loading. This stops a slow-loading collection, or a plugin canvas that appears late, from losing its routes.
- Settings for a canvas that is currently missing are kept, so uninstalling Vertical Canvas temporarily doesn't erase them.
- Import and export use the same profile object as above.
