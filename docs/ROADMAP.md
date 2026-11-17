# Roadmap

## Status

| Phase | Name | Status |
| --- | --- | --- |
| 0 | Development foundation | ✅ Done |
| 1 | OBS integration and source discovery | ✅ Done |
| 2 | Video routing engine | ✅ Done |
| 3 | Scene synchronisation | ✅ Done |
| 4 | Independent audio routing | ✅ Done |
| 5 | Recording engine | ✅ Done |
| 6 | Dock interface | ✅ Done |
| 7 | Testing and optimisation | ✅ Done (Windows) |
| 8 | Packaging and release | ✅ 1.0.0 |

## Decisions

| ADR | Decision | Status |
| --- | --- | --- |
| [0001](adr/0001-obs-view-over-canvas-api.md) | Render with `obs_view` | Superseded by 0006 |
| [0002](adr/0002-private-scene-mirror.md) | Private scene mirror with shared sources | Accepted |
| [0003](adr/0003-stream-exclusion.md) | Never modify a live composition; stream exclusion is user-managed | Accepted |
| [0004](adr/0004-independent-audio-mix.md) | Independent per-source audio mix | Accepted |
| [0005](adr/0005-multi-canvas-support.md) | One pipeline per canvas, including Aitum Vertical | Accepted |
| [0006](adr/0006-private-canvas-rendering.md) | Render through a private canvas | Accepted |
| [0007](adr/0007-routes-in-scene-collection.md) | Store routes in the scene collection | Accepted |

## Proofs of concept

| POC | Result |
| --- | --- |
| 1. Independent video | A private canvas mirror renders on both canvases while the originals are unchanged. Frame-rate divisors give lower recording frame rates. |
| 2. Independent audio | Per-source capture with a timestamped mix. Audio routed Off measures −102 dB, and A/V offset is within 10 ms of OBS's own recording. See [ADR-0004](adr/0004-independent-audio-mix.md) for what OBS has already applied when the callback runs. |
| 3. Combined pipeline | MKV through `ffmpeg_muxer`, and MP4 through OBS's hybrid MP4 output. Files survive crashes and exits while recording. |

---

## v1.0 release checklist

Each item is covered by an automated integration test ([tests/integration](../tests/integration/)) unless noted.

**Video**
- [x] Independent recording composition on each canvas
- [x] All routing states, worded as ADR-0003 requires
- [x] Nested scenes and groups
- [x] Scene changes, animated transitions and Studio Mode
- [x] Source transforms and filters (transforms are tested; filters belong to the shared source)
- [x] Recording preview (checked manually)

**Audio**
- [x] Independent source routing
- [x] OBS fader and mute; recording gain
- [x] Timestamp alignment matching OBS, with no drift over a long recording
- [x] Multiple audio sources
- [x] Audio filters (applied before capture by design)

**Recording**
- [x] Independent video and audio encoders
- [x] Independent resolution, frame rate and quality
- [x] Start, stop, pause and resume
- [x] Container and file settings
- [x] Failure handling for each canvas
- [x] Interrupted-recording recovery

**Dual output**
- [x] Selective recording of the Aitum Vertical canvas
- [x] Horizontal and vertical recordings at the same time
- [x] Vertical scene switches mirrored
- [x] Per-canvas routing overrides and settings
- [x] Aitum Vertical's own scenes untouched
- [x] Canvases recreated at runtime (scene collection switch) picked up again

**Interface**
- [x] Dockable interface, shown on first run
- [x] Live-visibility and audio-track warnings
- [x] Search, filters and bulk changes
- [x] Routing profiles with import and export
- [x] Hotkeys
- [x] Settings that persist

**Reliability**
- [x] No live composition is ever modified (NFR-001)
- [x] A recording failure doesn't affect another canvas or a stream (NFR-002, NFR-003)
- [x] Route changes apply live, without restarts (NFR-004)
- [x] No memory leaks after a long session that ends while recording
- [x] Files finalised when OBS exits mid-recording
- [x] No render or encoder frames skipped with two recordings at once
- [x] Long-duration soak test

## After 1.0

| Item | Notes |
| --- | --- |
| Runtime testing on macOS and Linux | Builds compile in CI. The preview supports X11 but not Wayland yet. |
| Separate tracks for each source (stems) | The mixer already has every source separately |
| Audio crossfades during transitions | Follow the transition's progress |
| Per-placement routes in Studio Mode duplicates | Map duplicated items back to their originals |
| Installer for Windows | CI produces a ZIP. An installer would place it in `ProgramData` automatically. |
