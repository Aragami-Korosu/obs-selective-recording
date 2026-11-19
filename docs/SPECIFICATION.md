# Specification

**Status:** v1.0 · **Platform:** Windows x64, tested on OBS 33.0.1 with Aitum Vertical 1.6.7. macOS and Linux builds compile but haven't been tested at runtime.

## 1. Product vision

OBS Selective Recording is a native OBS Studio plugin that records a composition that differs from your livestream. You choose, per source or per placement, what goes into the recording; a source's sound goes with its video. The livestream is left as you built it in OBS, except that a source you set to Recording only is hidden from it.

The plugin must:

- Run entirely inside OBS Studio, with no external services.
- Provide a dedicated, dockable management interface.
- Route every supported source into the recording, its video and audio together ([ADR-0010](adr/0010-audio-follows-video.md)).
- **Change the live composition only when the user chooses Recording only**, by hiding that placement in OBS ([ADR-0003](adr/0003-stream-exclusion.md), amended by [ADR-0008](adr/0008-streamlabs-style-placements.md)). Otherwise it controls only its own recordings.
- Stay in sync with the active scene collection.
- Keep routing configuration across OBS restarts.
- Work with dual-output setups (horizontal and vertical). Each canvas gets its own selective recording ([section 3.5](#35-multiple-canvases-dual-output)).

| Identifier | Value |
| --- | --- |
| Project name | OBS Selective Recording |
| Repository / module | `obs-selective-recording` |
| Plugin ID | `selective-recording` |

## 2. Routing model

Each source, or each placement of a source, has one **route**, covering its video and its audio. The recording takes a source's audio while the source is visible in the recording. Audio devices that aren't in any scene (Settings → Audio) have a route of their own.

| Route | Config value | Recording (plugin) | Stream |
| --- | --- | :---: | --- |
| Both | `both` | Included | Unchanged |
| Stream only | `stream_only` | Excluded | Unchanged |
| Recording only | `recording_only` | Included | The plugin hides the placement in OBS, and shows it again when the route changes back. An audio device has to be removed from the stream's track by the user; the plugin warns until it is. |
| Off | `off` | Excluded | User hides it. The plugin warns if it's still live. |

Rules:

- One route covers video and audio ([ADR-0010](adr/0010-audio-follows-video.md)).
- When more than one rule applies, the most specific wins:
  1. Scene-item override (one placement of a source)
  2. Canvas-specific source rule (the source on one canvas only)
  3. Source rule (every placement of the source, on every canvas)
  4. Group or nested-scene inheritance
  5. Profile default
  6. Built-in default (`both`)
- **Disabling the plugin** returns every recording to the default all-included composition. It must never produce a blank or partial recording.

## 3. Functional requirements

| ID | System | Responsibility |
| --- | --- | --- |
| FR-001 | Source Manager | Discover and track canvases, sources, scenes, groups and audio sources |
| FR-002 | Video Router | Decide recording visibility for each source |
| FR-003 | Audio Router | Route each audio source into the recording mix |
| FR-004 | Scene Synchroniser | Mirror scene changes, edits and transitions |
| FR-005 | Recording Engine | Run an independent encoder and output pipeline for each canvas |
| FR-006 | Preview Engine | Display the recording composition |
| FR-007 | Configuration Manager | Save, restore and migrate settings |
| FR-008 | Control Interface | Provide the dock, hotkeys, controls and warnings |
| FR-009 | Recovery | Detect interrupted recordings and help recover them |

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

The plugin keeps three things separate:

| Layer | Owner | Plugin access |
| --- | --- | --- |
| Audio sources (mic, game, Discord…) | OBS | Read-only capture of each source's audio |
| OBS mixer tracks 1–6, which feed streams and OBS recordings | OBS and the user | Read-only, used only for warnings |
| Recording audio mix | Plugin | Full control |

The recording mix is built directly from each source ([ADR-0004](adr/0004-independent-audio-mix.md)). It never reads from or changes OBS's tracks, so the track setup doesn't limit what can be recorded. Track layout only matters for keeping audio *out of a stream*, which the user does in OBS.

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

The plugin needs to match OBS mixer behaviour, including volume, balance, sync offset, push-to-talk and audio filters. POC 2 will establish how closely it can do this, before anything is declared impossible.

### 3.3 Recording

| Setting | v1.0 |
| --- | --- |
| Container | MKV (default), MP4 |
| Video encoder | NVENC, QuickSync, AMF or x264, discovered at runtime |
| Audio encoder | AAC, plus Opus where the container allows it |
| Resolution | Independent. Defaults to the canvas's base resolution. |
| Frame rate | OBS's frame rate, or a half, third or quarter of it (an encoder frame-rate divisor) |
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

**Warnings**
- A **Recording only** or **Off** source that's still live on that canvas (NFR-008). The warning names the scene or audio track responsible.
- Audio setups the user can't achieve, for example two streams sharing a track (NFR-006).
- Not enough encoder capacity to start (NFR-007).
- An interrupted recording found at startup (FR-009).

**Recording controls**
- Start, pause and stop for each canvas.
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
| Never modify, start or stop another plugin's canvas or outputs | Required |
| Other canvases that follow the same conventions (for example Twitch Dual Format) | Best effort |

**Shared across canvases** (profile level): source routing rules, audio routing rules, global recording preferences (container, audio encoder and bitrate, filename format, recovery options), and profile management.

**Specific to each canvas:** source overrides, enabling the canvas, recording resolution, video encoder and quality, recording destination, and the recording controls.

In the dock, each canvas has its own tab (for example **Horizontal** and **Vertical**). Each tab has its own source list and recording controls.

See [ADR-0005](adr/0005-multi-canvas-support.md).

### 3.6 Persistence

Routing must survive OBS restarts, scene and scene-collection changes, source renames and reordering, and profile changes. Sources are identified by UUID, never by name. See [CONFIGURATION.md](CONFIGURATION.md).

### 3.7 Recovery

| Situation | Required behaviour |
| --- | --- |
| Encoder or output error while recording | Stop that canvas's recording, finalise what's been written, and report the cause. Other canvases and every stream keep running. |
| Disk full or destination unavailable | Same as above, with a specific message. Warn early when free space is low. |
| A canvas is removed while recording | Stop that canvas's recording cleanly, and keep the file. |
| A source is deleted while recording | Remove it from the mirror. The recording continues. |
| OBS exits normally while recording | Stop and finalise every recording before the plugin unloads. |
| OBS crashes or the power fails | The file stays playable (MKV, or OBS's hybrid MP4 output where available). On the next start, the dock lists the interrupted recording and links to it. OBS's **File → Remux Recordings** converts it to MP4. |

The plugin records a session marker for each active recording. It removes the marker when the recording stops cleanly, and checks for leftover markers at startup.

## 4. Non-functional requirements

| ID | Requirement | Verified by |
| --- | --- | --- |
| NFR-001 | Never modify the visibility, order, transform or audio track assignment of anything in an original OBS canvas | Code review rule. A test asserts that the original scene state is unchanged after every plugin operation. |
| NFR-002 | Recording failures must not interrupt streaming | Fault-injection tests (encoder failure, disk full, output error) while streaming |
| NFR-003 | Each canvas handles its own recording failures. A failure on one canvas never stops another canvas's recording. | Fault injection on one canvas while both are recording |
| NFR-004 | Routing changes apply to active recordings immediately, and never restart a stream or a recording | Integration test: change routes while streaming and recording |
| NFR-005 | Aitum Vertical keeps working as before: its scenes, transitions, streams, recording, replay buffer and hotkeys | Regression checklist against the latest Vertical release |
| NFR-006 | Detect and explain audio setups the user can't achieve, for example a **Recording only** source on a track two streams share | Unit tests on the track analysis, plus a dock warning |
| NFR-007 | Detect encoder capacity limits before starting, and fail with a clear message instead of disturbing other encoders | Start with hardware sessions exhausted |
| NFR-008 | Warn when a **Recording only** or **Off** source is still live on its canvas, for video or audio | Integration test for each canvas |
| NFR-009 | Handle canvases being created and removed while OBS is running, including while recording | Add and remove Vertical's canvas at runtime |
| NFR-010 | Keep settings separately for each canvas. A canvas that's missing keeps its settings until it returns. | Settings round-trip tests |

**Scope of NFR-002 and NFR-003.** The plugin runs inside the OBS process. A crash in plugin code takes OBS down with it, and the stream with it. These requirements cover failures the plugin can catch and handle. Crash risk is reduced by strict reference ownership, never letting C++ exceptions cross into libobs callbacks, defensive checks on every OBS handle, and the long-run tests in Phase 7.

## 5. Known limitations in v1.0

| Limitation | Effect |
| --- | --- |
| Audio transitions | Recording audio follows routes immediately. It doesn't crossfade during scene transitions or item show/hide fades. |
| Show/hide transitions on items | Mirrored items appear and disappear instantly |
| Studio Mode with Duplicate Scene | Per-placement routes don't apply to OBS's duplicated program scene. Source and canvas routes do. |
| Nested scenes from another canvas | Use the size of the canvas being recorded |
| OBS 33 log line | `Attempted to add Scene without specifying a canvas!` appears once for each mirrored scene when a recording or preview starts. It's harmless (see [ADR-0006](adr/0006-private-canvas-rendering.md)). |
| Hardware encoder limits | Detected when a recording starts (with an optional fallback to x264), not predicted in advance |

## 6. Out of scope for v1.0

- Separate audio tracks (stems) for each source
- File splitting and automatic remuxing during recording
- Linux and macOS builds. The architecture supports them, but they aren't tested yet.
- Any change to a live composition, by design ([ADR-0003](adr/0003-stream-exclusion.md))
- Vertical Canvas versions earlier than 1.6.0, whose scenes are private to that plugin
