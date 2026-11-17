# 0005. Support multiple canvases, including Aitum Vertical Canvas

- **Status:** Accepted
- **Date:** 2026-11-17

## Context

Many streamers send a horizontal stream and a vertical stream at the same time. The usual tool for this in OBS is [Aitum Vertical Canvas](https://github.com/Aitum/obs-vertical-canvas). Streamlabs Desktop can't use Selective Recording while its Dual Output mode is on, so supporting this workflow is a key reason for this plugin to exist.

From version 1.6.0, Vertical Canvas is built on the OBS Canvas API, which needs OBS 31.1 or newer. Its design:

- It registers a frontend canvas named `Aitum Vertical` with `obs_frontend_add_canvas(..., PROGRAM)`.
- Its scenes belong to that canvas (`obs_canvas_scene_create`). They don't appear in the main scene list.
- Channel 0 of the canvas holds the active scene, or Vertical's own transition source. Scene switches and per-scene transition overrides fire the canvas's `channel_change` signal.
- Capture sources are shared with the main canvas. The same webcam can appear in both a horizontal and a vertical scene.
- It runs its own stream, recording and replay outputs. Their audio comes from OBS's mixer tracks.
- Version 1.6.0 also added an optional extra canvas for Twitch Dual Format streaming.

Before 1.6.0, Vertical Canvas used private views and private scenes that other plugins can't discover.

## Decision

Treat canvases as the top-level unit, rather than adding a special case for Vertical.

1. **Discovery.** On `FINISHED_LOADING`, `CANVAS_ADDED` and `CANVAS_REMOVED`, enumerate the main canvas (`obs_get_main_canvas`) and every frontend canvas (`obs_frontend_get_canvases`). Each canvas's scenes come from `obs_canvas_enum_scenes`.
2. **One selective pipeline per canvas.** Each enabled canvas gets its own scene mirror, `obs_view`, audio mix, encoders and output. The horizontal and vertical selective recordings can run separately or together.
3. **Following the program.** For the main canvas, the plugin follows frontend scene and transition events. For any other canvas, it follows that canvas's `channel_change` signal. When channel 0 holds a transition, the plugin connects to that transition's `transition_start` and `transition_stop` signals and mirrors it the same way as the main canvas's transition.
4. **Resolution.** A canvas's selective recording defaults to that canvas's base resolution (for example 1080×1920), and updates on the canvas's `video_reset` signal.
5. **Routing scope.** A source rule applies to every canvas by default, and can be overridden per canvas. Hiding an alert overlay once therefore hides it from both recordings, while a canvas-specific exception is still possible.
6. **Generic, with known names.** The canvas name `Aitum Vertical` is used only for labels and icons in the dock. Any canvas that follows the same conventions works, including the Twitch Dual Format canvas and OBS's own future canvases.
7. **Read-only.** The plugin never creates, renames or modifies another plugin's canvas, and never calls Vertical's outputs. Vertical's own **Record** button stays independent of the selective recording.

## Consequences

- The plugin now reads canvases through the Canvas API. This partly supersedes [ADR-0001](0001-obs-view-over-canvas-api.md): rendering still uses `obs_view`, but discovery depends on `obs_canvas_*`. POC 1 should also check whether a private canvas (`obs_canvas_create_private`) is simpler than a view. Note that canvases ignore any per-canvas frame rate and render at the global frame rate.
- The minimum supported versions are **OBS 31.1** and **Vertical Canvas 1.6.0**. Older Vertical versions can't be supported because their scenes are private to that plugin.
- Two selective recordings plus two live streams can mean four simultaneous video encodes. Encoder availability and session limits (for example the NVENC session cap) must be checked at start, with clear errors.
- **Stream exclusion for audio** ([ADR-0003](0003-stream-exclusion.md)): OBS mixer tracks are shared by all canvases. If the horizontal and vertical streams use the same track, a source's audio can't be removed from one stream without removing it from the other. The dock must detect this and explain it.
- Mirrored nested scenes can cross canvases, for example when a vertical scene contains a horizontal scene as a source. The mirror has to resolve the nested scene from whichever canvas owns it.
- Vertical Canvas is under active development. Integration tests must cover the latest release, and the README must list the versions that have been tested.
