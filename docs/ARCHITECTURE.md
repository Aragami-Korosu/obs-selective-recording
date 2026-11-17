# Architecture

This document describes how OBS Selective Recording works internally. For *what* the plugin does, see the [specification](SPECIFICATION.md).

> Parts of this design depend on the proofs of concept in the [roadmap](ROADMAP.md#proofs-of-concept). Anything marked **(POC)** may change based on what they show.

## Overview

```mermaid
flowchart TB
    subgraph OBS["OBS Studio"]
        LIVE["Program scene<br/>(main canvas)"] --> STREAM["Stream / OBS recording<br/>(never modified)"]
        SRC["Shared sources<br/>game capture, webcam, mic, browser…"]
        LIVE -.uses.-> SRC
    end

    subgraph PLUGIN["Selective Recording plugin"]
        SM["SourceManager"] --> RM["RoutingManager"]
        RM --> MIRROR["SceneMirror<br/>private scene tree"]
        RM --> AP["AudioPipeline<br/>capture → mix"]
        MIRROR -.shares.-> SRC
        AP -.captures.-> SRC
        MIRROR --> VP["VideoPipeline<br/>obs_view"]
        VP --> REC["RecordingController<br/>encoders + output"]
        AP --> REC
        VP --> PREV["PreviewController"]
        SET["SettingsManager"] --> RM
        DOCK["Dock UI"] <--> RM
        DOCK <--> REC
    end

    REC --> FILE[("Recording file")]
```

The plugin builds a **second, private composition** alongside OBS's own. That composition reuses OBS's capture sources by reference, so nothing is captured twice. It has its own scene containers, video mix, audio mix and encoders.

## Ownership boundary

The plugin only *reads* OBS state, and only *writes* to objects it created. This is the core guarantee of [ADR-0003](adr/0003-stream-exclusion.md) and NFR-001.

| Object | Access |
| --- | --- |
| Original scenes and scene items, on any canvas | Read: structure, transforms, visibility. Never written. |
| Capture sources | Shared by reference into private scenes. Audio is captured read-only. Settings, filters, volume, mute and mixer tracks are never written. |
| Canvases from OBS or other plugins | Read: scenes, channel 0, video info. Signals are observed. Never written. |
| Outputs and encoders the plugin didn't create (stream, OBS recording, Vertical's outputs) | Read: active state and audio encoder mixer indexes, used only for warnings. Never started, stopped or modified. |
| Private scenes, transitions, views, audio mixes, encoders and outputs | Created, owned and destroyed by the plugin |

To enforce this, every code path that writes to OBS goes through types that can only hold plugin-created objects. Phase 2 adds a test that snapshots the original scene state, runs every plugin operation, and checks that nothing changed.

## Multiple canvases

The diagram above shows one canvas. In practice the plugin runs **one selective pipeline per canvas**, so the main horizontal canvas and a vertical canvas from [Aitum Vertical Canvas](https://github.com/Aitum/obs-vertical-canvas) each get their own. See [ADR-0005](adr/0005-multi-canvas-support.md).

```mermaid
flowchart TB
    subgraph OBS["OBS Studio"]
        MAIN["Main canvas<br/>1920×1080"]
        VERT["'Aitum Vertical' canvas<br/>1080×1920"]
        SRC["Shared sources"]
        MAIN -.-> SRC
        VERT -.-> SRC
    end

    CM["CanvasManager"] --> PH["Selective pipeline<br/>(horizontal)"]
    CM --> PV["Selective pipeline<br/>(vertical)"]
    MAIN -- "frontend scene /<br/>transition events" --> PH
    VERT -- "channel_change,<br/>transition signals" --> PV
    RM["RoutingManager<br/>(shared rules +<br/>per-canvas overrides)"] --> PH & PV
    PH --> F1[("horizontal.mkv")]
    PV --> F2[("vertical.mkv")]
```

Each *selective pipeline* is a `SceneMirror`, `VideoPipeline`, `AudioPipeline` and `RecordingController`, all bound to one canvas. Pipelines share the `RoutingManager` and `SettingsManager`, and never share private scenes or encoders.

| Canvas event | Source | Action |
| --- | --- | --- |
| `FINISHED_LOADING`, `CANVAS_ADDED` | frontend | Discover canvases and create a pipeline for each enabled one |
| `CANVAS_REMOVED`, canvas `remove` | frontend, canvas | Stop that pipeline's recording cleanly, keep the file, then tear the pipeline down. Settings are kept (NFR-009, NFR-010). |
| `channel_change` (channel 0) | canvas | Switch the private program scene, or attach to the new transition |
| `transition_start` / `transition_stop` | the canvas's transition source | Drive the private transition |
| `source_add` / `source_remove` | canvas | Add or remove mirrored scenes |
| `video_reset` | canvas | Update the default recording resolution, applied on the next start |

The main canvas is followed through frontend events (`SCENE_CHANGED`, `TRANSITION_*`), because OBS's own transition lives outside the canvas channel.

## Components

Each component owns its own state and exposes a small interface. Components never reach into each other's internals.

| Component | Directory | Responsibility |
| --- | --- | --- |
| `CanvasManager` | `src/core/` | Discovers canvases, tracks additions and removals, and owns one selective pipeline per enabled canvas. |
| `SourceManager` | `src/core/` | Enumerates scenes (for each canvas), groups, items and audio sources. Emits change events. |
| `RoutingManager` | `src/core/` | Resolves the effective video and audio route for any source or item. |
| `SettingsManager` | `src/core/` | Loads, saves and migrates configuration. |
| `LiveStateMonitor` | `src/core/` | Read-only checks behind the warnings: recording-only sources still live (NFR-008), and audio setups that can't be achieved (NFR-006). |
| `SceneMirror` | `src/video/` | Builds and maintains the private scene tree from the original scenes. |
| `VideoPipeline` | `src/video/` | Renders the private program scene into an independent `obs_view` mix. |
| `PreviewController` | `src/video/` | Draws the recording mix in the dock. Disabled while hidden. |
| `AudioPipeline` | `src/audio/` | Captures each source's audio, applies routing and gain, and mixes the result. |
| `RecordingController` | `src/recording/` | Runs the output lifecycle: start, pause, stop and error handling. |
| `EncoderManager` | `src/recording/` | Discovers available encoders, checks capacity (NFR-007) and builds encoder settings. |
| `RecoveryManager` | `src/recording/` | Writes session markers and detects interrupted recordings at startup. |
| `DockController` | `src/ui/` | Qt dock connecting user actions to the engine. |
| `HotkeyManager` | `src/ui/` | Registers and persists plugin hotkeys. |

Directories are created as each milestone begins, not before.

## Video pipeline

```mermaid
flowchart LR
    A["Original scene"] --> B["SceneMirror<br/>(recursive)"]
    B --> C{"Route for<br/>each item"}
    C -->|both| D1["Follows original<br/>visibility"]
    C -->|recording_only| D2["Always visible"]
    C -->|stream_only / off| E["Always hidden"]
    D1 & D2 --> D["Mirrored item"]
    D & E --> F["Private scene"]
    F --> G["obs_view<br/>(custom video info)"]
    G --> H["video_t → encoder"]
```

**Private scene tree.** For each original scene on the pipeline's canvas, the mirror creates a private scene with `obs_scene_create_private`. It adds the *same* `obs_source_t` objects as items, and copies each item's transform, crop, blend mode and order. Visibility depends on the route. A **Both** item follows the original's visibility. A **Recording only** item is always shown, because the user hides the original from the stream. **Stream only** and **Off** items are always hidden. All of this applies only to the mirrored copy. The original item is never touched. Nested scenes and groups are mirrored recursively, so a nested scene item points to the mirrored child scene instead of the original. A nested scene can belong to a different canvas, for example a horizontal scene used inside a vertical one, so the mirror resolves the child from whichever canvas owns it. See [ADR-0002](adr/0002-private-scene-mirror.md).

**Rendering.** `obs_view_add2()` adds the private program scene to the render loop with its own `obs_video_info`, which allows a different resolution from the canvas. Frame rate follows the global render loop (POC 1). It returns a `video_t` that the video encoder connects to. See [ADR-0001](adr/0001-obs-view-over-canvas-api.md).

**Synchronisation.** The mirror listens to these signals instead of polling:

| Signal | Source | Mirror action |
| --- | --- | --- |
| `item_add`, `item_remove` | scene | Add or remove the mirrored item |
| `reorder`, `refresh` | scene | Re-sync item order |
| `item_transform` | scene | Copy the transform |
| `item_visible` | scene | Re-evaluate the route and visibility |
| `rename` | source | Update the inventory |
| `remove`, `destroy` | source | Release mirrored references |
| `SCENE_CHANGED` | frontend | Switch the private program scene |
| `SCENE_COLLECTION_CHANGING` / `CHANGED` | frontend | Tear down, then rebuild |
| `TRANSITION_*` | frontend | Drive the private transition |

**Transitions (POC).** The view renders a private transition source whose type and duration match the active OBS transition. Its endpoints are the mirrored scenes. Hard cuts come first. In Studio Mode, the mirror follows the program scene only.

## Audio pipeline

```mermaid
flowchart LR
    S1["Mic"] & S2["Game"] & S3["Discord"] & S4["Music"] --> CB["audio capture<br/>callbacks"]
    CB --> BUF["Per-source<br/>timestamped buffers"]
    BUF --> MIX["Mixer<br/>route · gain · mute · resample"]
    MIX --> OUT["audio_t<br/>(recording only)"]
    OUT --> AENC["Audio encoder"]
```

- `obs_source_add_audio_capture_callback()` delivers each source's audio and its mute state on the audio thread.
- Each source writes into its own ring buffer, keyed by timestamp. The mixer pulls an aligned window from every routed source, rather than mixing samples as callbacks happen to arrive.
- The mixed result goes to a plugin-owned `audio_t`. The mix never reads from or writes to OBS's six mixer tracks, so the user's track setup doesn't limit what can be recorded.
- Each canvas pipeline registers its own capture callbacks and keeps its own buffers. Tearing down one pipeline never affects another.
- **(POC)** It hasn't been confirmed which OBS mixer stages (volume, balance, sync offset, push-to-talk, filters) are already applied when the callback runs. The plugin must match OBS for any stage that isn't. See [ADR-0004](adr/0004-independent-audio-mix.md).

## Recording engine

```mermaid
stateDiagram-v2
    [*] --> Idle
    Idle --> Preparing: start
    Preparing --> Recording: output started
    Preparing --> Error: encoder or output failure
    Recording --> Paused: pause
    Paused --> Recording: resume
    Recording --> Stopping: stop
    Paused --> Stopping: stop
    Recording --> Failed: output stops with an error
    Paused --> Failed: output stops with an error
    Stopping --> Idle: output stopped
    Failed --> Idle: resources released, error reported
    Error --> Idle: reset
```

This state machine runs separately for each canvas.

For each canvas, the plugin creates its own `ffmpeg_muxer` output and its own video and audio encoders. It connects them to that pipeline's private `video_t` and `audio_t`. OBS's own outputs are never touched, and neither are another plugin's outputs, such as Vertical's stream, recording and replay buffer.

Before starting, the controller checks that the chosen encoder can open another session. With dual streaming plus two selective recordings, hardware encoder session limits can be reached. If that happens, recording fails with a clear message, and the live streams are left untouched.

## Failure isolation

Each canvas pipeline is a separate failure domain (NFR-002, NFR-003).

- **Nothing shared that can fail.** Pipelines share only read-only configuration and the routing rules. Encoders, outputs, views, private scenes and audio buffers all belong to one pipeline.
- **Failures are detected from the output.** The `RecordingController` listens to its own output's `stop` signal. A non-zero code (`OBS_OUTPUT_ERROR`, `OBS_OUTPUT_NO_SPACE`, `OBS_OUTPUT_ENCODE_ERROR` and so on) moves that pipeline to **Failed**. That pipeline then releases its encoders and view and reports the cause. Other pipelines and every stream keep running.
- **Failures never reach other outputs.** Error handling only ever calls functions on objects the pipeline owns, so a failure path can't stop a stream.
- **No exceptions cross into libobs.** Every callback the plugin registers catches all C++ exceptions at its boundary, logs them, and marks only the owning pipeline as failed.
- **Process-level limits.** A crash in plugin code still crashes OBS. Isolation reduces how often that can happen, but can't prevent it. Phase 7's fault-injection and long-run tests cover this.

## Live-state warnings

`LiveStateMonitor` evaluates the warnings defined in [ADR-0003](adr/0003-stream-exclusion.md). It recalculates them on the same scene signals the mirror uses, and on output start and stop. It doesn't poll.

- **Video (NFR-008).** For each **Recording only** or **Off** source, it walks the scenes on air on that canvas, following groups and nested scenes. It warns if any path to the source is visible.
- **Audio (NFR-008, NFR-006).** It finds the stream outputs for each canvas: `obs_frontend_get_streaming_output()` for the main canvas, and for other canvases, outputs whose video encoder uses that canvas's `video_t`. It reads each output's audio encoder mixer indexes and compares them with the source's mixer flags. A **Recording only** source on one of those tracks raises a warning. If the user can't fix it without affecting another canvas's stream, because both streams share a track, the warning explains that instead.

## Recovery

`RecoveryManager` writes a small session marker when a recording starts and deletes it when the recording stops cleanly. The marker records the canvas, file path, container and start time.

At startup, any leftover marker means a recording was interrupted. The dock lists those files and offers to remux them. Because MKV is the default, the file up to the crash stays playable. POC 3 will check whether OBS's crash-resilient MP4 mode can be used when MP4 is selected.

On `OBS_FRONTEND_EVENT_EXIT`, every active pipeline stops and finalises before the module unloads. A clean stop always removes the marker.

## Threading

| Thread | What runs there |
| --- | --- |
| UI (Qt main) | Dock, frontend events and settings |
| Graphics | `obs_view` rendering and preview drawing |
| Audio | Capture callbacks. These must not block or allocate in the hot path. |
| Output | Encoder and muxer callbacks |

Scene and source signals can fire from any thread. Handlers only collect changes and send them to the UI thread, which owns the mirror state.

## Reference ownership

- Every `obs_source_t`, `obs_scene_t` and `obs_sceneitem_t` the plugin holds uses the RAII wrappers from `obs.hpp` (`OBSSourceAutoRelease` and similar). Raw pointers are borrowed only for the duration of a call.
- When a source is removed (`remove` signal), the plugin drops every mirrored reference to it immediately, so OBS can destroy the source even while a recording is running.
- On `OBS_FRONTEND_EVENT_EXIT`, the plugin stops the output, tears down the mirror and removes every callback, all before `obs_module_unload` runs.

## Portability

The engine (`core/`, `video/`, `audio/` and `recording/`) uses only libobs APIs. Qt is confined to `ui/`, and Windows-specific code isn't allowed outside `utilities/`. This keeps future Linux and macOS ports to packaging work rather than engine changes.
