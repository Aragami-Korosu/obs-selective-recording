# Specification

**Status:** Draft v1.0 · **Platform:** Windows x64 first, with a portable architecture

## 1. Product vision

OBS Selective Recording is a native OBS Studio plugin that lets users choose, independently, which video and audio sources go to their livestream and which go to their local recording.

The plugin must:

- Run entirely inside OBS Studio, with no external services.
- Provide a dedicated, dockable management interface.
- Route video and audio independently for every supported source.
- Record a composition that differs from the live stream **without modifying the live scene**.
- Stay in sync with the active scene collection.
- Keep routing configuration across OBS restarts.
- Work with dual-output setups (horizontal and vertical). Each canvas gets its own selective recording. See [section 3.5](#35-multiple-canvases-dual-output).

| Identifier | Value |
| --- | --- |
| Project name | OBS Selective Recording |
| Repository / module | `obs-selective-recording` |
| Plugin ID | `selective-recording` |

## 2. Routing model

Each source has a **video route** and an **audio route**, and each can take one of these values:

| Route | Config value | Stream | Recording |
| --- | --- | :---: | :---: |
| Both | `both` | Included | Included |
| Stream only | `stream_only` | Included | Excluded |
| Recording only | `recording_only` | Excluded | Included |
| Off | `off` | Excluded | Excluded |

Rules:

- Video and audio routes are independent. Changing one never changes the other.
- When more than one rule applies, the most specific wins:
  1. Scene-item override (one placement of a source)
  2. Canvas-specific source rule (the source on one canvas only)
  3. Source rule (every placement of the source, on every canvas)
  4. Group or nested-scene inheritance
  5. Profile default
  6. Built-in default (`both`)
- **Stream exclusion** (`recording_only` and `off`) must remove the source from OBS's main output, which the plugin doesn't own. How it does this is an open decision, tracked in [ADR-0003](adr/0003-stream-exclusion.md).
- **Disabling the plugin** returns the recording to the default all-included composition. It must never produce a blank or partial recording.

## 3. Functional requirements

| ID | System | Responsibility |
| --- | --- | --- |
| FR-001 | Source Manager | Discover and track OBS sources, scenes, groups and audio sources |
| FR-002 | Video Router | Decide recording visibility for each source |
| FR-003 | Audio Router | Route each audio source into the recording mix |
| FR-004 | Scene Synchroniser | Mirror scene changes, edits and transitions |
| FR-005 | Recording Engine | Run an independent encoder and output pipeline |
| FR-006 | Preview Engine | Display the recording composition |
| FR-007 | Configuration Manager | Save, restore and migrate settings |
| FR-008 | Control Interface | Provide the dock, hotkeys and controls |

### 3.1 Video

The recording composition must preserve, for every included item:

- Position, rotation, scale, alignment and bounds
- Crop
- Layer order
- Blending mode and method
- Groups and nested scenes
- Source filters and scene filters
- Shared source references, so captures are never duplicated

It must also:

- Follow the **program** scene of its canvas, not the Studio Mode preview.
- Reproduce scene transitions. Hard cuts are required first, then animated transitions with matching duration before v1.0.

### 3.2 Audio

| Feature | v1.0 |
| --- | --- |
| Independent per-source routing | Required |
| Source gain and mute handling | Required |
| Timestamp alignment between sources | Required |
| Sample-rate conversion | Required |
| Stereo output | Required |
| Clipping protection | Required |
| Recording-only audio | Required |
| Multichannel layouts | Advanced |
| Monitoring the recording mix | Optional |
| Separate tracks (stems) per source | Post-1.0 |

The plugin needs to match OBS mixer behaviour, including volume, balance, sync offset, push-to-talk and audio filters. The first audio proof of concept will establish how closely it can do this.

### 3.3 Recording

| Setting | v1.0 |
| --- | --- |
| Container | MKV (default), MP4 |
| Video encoder | NVENC, QuickSync, AMF or x264, discovered at runtime |
| Audio encoder | AAC, plus Opus where the container allows it |
| Resolution | Independent. Defaults to the canvas's base resolution. |
| Frame rate | Follows OBS's global frame rate. libobs renders every canvas and view on one render loop. POC 1 will confirm whether a lower rate (for example 30 fps from a 60 fps canvas) is possible. |
| Video quality and audio bitrate | Independent |
| Output directory and filename format | Configurable |
| Pause and resume | Required |
| File splitting, automatic remux | Post-1.0 |

The recording must start and stop without affecting streaming, and it must be independent of OBS's own **Start Recording** state.

### 3.4 Dock interface

**Source management**
- One row per source, with a video routing control and an audio routing control.
- An expandable scene hierarchy with group inheritance.
- Search, filtering and bulk changes.
- Icons for each source type, plus warnings for unsupported sources.

**Recording controls**
- Start, pause and stop.
- Duration, file name, file size, dropped frames, encoder status and low-disk-space warnings.

**Preview**
- A toggleable preview of the real recording composition. It must cost nothing while closed.

**Profiles**
- Save, switch, import and export routing presets without restarting OBS.
- Optionally link a profile to a scene collection.

**Interaction**
- Left-click a routing control to cycle through its states. Right-click it to choose a state from a menu.
- Child items inherit group routing unless they have their own override.

### 3.5 Multiple canvases (dual output)

Streamlabs Desktop disables Selective Recording when Dual Output is on. This plugin must support both at once.

| Requirement | v1.0 |
| --- | --- |
| Discover the main canvas and every frontend canvas (OBS 31.1+ Canvas API) | Required |
| Compatibility with [Aitum Vertical Canvas](https://github.com/Aitum/obs-vertical-canvas) 1.6.0+ | Required |
| A separate selective recording for each canvas, which can be started separately or together | Required |
| Recording resolution follows the canvas by default (for example 1080×1920 vertical) | Required |
| Follow scene switches, transitions and per-scene transition overrides on each canvas | Required |
| Source rules shared across canvases, with per-canvas overrides | Required |
| Never modify, start or stop another plugin's canvas or outputs | Required |
| Other canvases that follow the same conventions (for example Twitch Dual Format) | Best effort |

In the dock, each canvas has its own tab (for example **Horizontal** and **Vertical**). Each tab has its own source list and recording controls.

See [ADR-0005](adr/0005-multi-canvas-support.md).

### 3.6 Persistence

Routing must survive OBS restarts, scene and scene-collection changes, source renames and reordering, and profile changes. Sources are identified by UUID, never by name. See [CONFIGURATION.md](CONFIGURATION.md).

## 4. Out of scope for v1.0

- Separate audio tracks (stems) for each source
- File splitting and automatic remuxing
- Linux and macOS builds. The architecture supports them, but they aren't tested yet.
- Changing the live stream composition
- Vertical Canvas versions earlier than 1.6.0, whose scenes are private to that plugin
