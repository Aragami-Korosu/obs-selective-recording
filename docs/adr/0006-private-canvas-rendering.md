# 0006. Render through a private canvas

- **Status:** Accepted. Supersedes [0001](0001-obs-view-over-canvas-api.md).
- **Date:** 2026-11-17

## Context

ADR-0001 chose `obs_view_add2()` to render the recording composition and avoided the Canvas API as too new. While implementing POC 1 against the OBS 31.1 sources, three problems with views came up:

1. **Scene size.** Scenes take their size from the canvas they belong to. A private scene with no canvas reports the main canvas's size, so a mirrored vertical scene would claim to be 1920×1080 instead of 1080×1920. Since OBS 31, item positions are stored relative to that size, which breaks nested scenes and bounds.
2. **Activation.** A view renders sources without activating them. A "Recording only" webcam or media source that's hidden in the live scene would never start.
3. **Stability.** Vertical Canvas 1.6, the canvas this plugin most needs to support, is itself built on the Canvas API. The API ships in every OBS release we support.

## Decision

Each pipeline creates a **private canvas** with `obs_canvas_create_private()`:

- **Base resolution:** the source canvas's base resolution, so mirrored scenes have the right size.
- **Output resolution:** the recording resolution.
- **Flags:** `ACTIVATE | EPHEMERAL`.
  - `ACTIVATE` makes recording-only sources render even when they're hidden live.
  - `EPHEMERAL` keeps the canvas out of saved data.
  - `MIX_AUDIO` is deliberately left out, so nothing in the canvas is mixed into OBS's audio tracks.

Mirrored scenes are created with `obs_scene_create_private()`, then moved into the private canvas with `obs_canvas_move_scene()` before any items are added. They are removed with `obs_canvas_scene_remove()` before they're released, because libobs doesn't do that for private sources.

The canvas exists only while a recording or preview is running, because libobs renders every mix every frame.

## Consequences

- Nested scenes, groups and relative transforms render exactly like the originals. The integration tests confirm this pixel by pixel on both canvases.
- Hidden recording-only sources render in the recording. OBS mixes a scene item's audio only when the item is visible in a canvas that has `MIX_AUDIO`, so activating them doesn't put their audio on the stream.
- `obs_canvas_reset_video()` refuses to run while any output is active. A different recording resolution therefore needs a new canvas, which is created when the next recording starts.
- OBS 33 logs `Attempted to add Scene without specifying a canvas!` once for each mirrored scene when a recording or preview starts. That comes from `obs_scene_create_private()`. It's harmless, and it was accepted over creating public canvas scenes, which would announce our private scenes to obs-websocket clients.
