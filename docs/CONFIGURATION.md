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
      "recording": {
        "container": "mkv",
        "width": 1920,
        "height": 1080,
        "fps_num": 60,
        "fps_den": 1,
        "video_encoder": "auto",
        "audio_encoder": "aac",
        "audio_bitrate": 192,
        "audio_sample_rate": 48000,
        "output_directory": "",
        "filename_format": "%CCYY-%MM-%DD %hh-%mm-%ss"
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
| `plugin_enabled` | When `false`, the recording includes every source. |
| `profiles[].scene_collection` | Optional. If set, the profile activates when that scene collection loads. |
| `source_routes` | Keyed by source UUID, so routes survive renames. |
| `item_overrides` | Keyed by the scene UUID plus the scene-item ID, for one placement of a source. Either `video` or `audio` can be left out to inherit. |
| `recording.video_encoder` | `auto` picks the best hardware encoder available, falling back to x264. |
| `recording.output_directory` | An empty value means the OBS recording path is used. |

## Resolution order

A source's effective route is the first match in this list:

1. `item_overrides` for this scene item
2. `source_routes` for this source
3. The route inherited from the parent group or nested scene
4. The profile's `default_*_route`
5. `both`

## Housekeeping

- Entries for sources that no longer exist are removed when a scene collection loads, but only after every source has finished loading. This stops a slow-loading collection from losing its routes.
- Import and export use the same profile object as above.
