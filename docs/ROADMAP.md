# Roadmap

Each phase must meet its acceptance criteria before the next one starts. The estimates assume one developer and are for planning only. They aren't commitments.

**Estimated total: 12–18 engineering weeks.**

| Phase | Name | Estimate | Status |
| --- | --- | --- | --- |
| 0 | Development foundation | 1 week | 🟡 In progress |
| 1 | OBS integration and source discovery | 1 week | ⚪ Not started |
| 2 | Video routing engine | 2–3 weeks | ⚪ Not started |
| 3 | Scene synchronisation | 2 weeks | ⚪ Not started |
| 4 | Independent audio routing | 3–4 weeks | ⚪ Not started |
| 5 | Recording engine | 1–2 weeks | ⚪ Not started |
| 6 | Dock interface | 1–2 weeks | ⚪ Not started |
| 7 | Testing and optimisation | 1–2 weeks | ⚪ Not started |
| 8 | Packaging and release | 1 week | ⚪ Not started |

## Proofs of concept

These three questions decide whether the architecture is sound. They come before any significant UI work.

| POC | Question | Feeds phase |
| --- | --- | --- |
| 1. Independent video | Can a private scene tree be rendered through `obs_view` while the original composition keeps streaming? Does it work for a vertical canvas scene too, at a vertical resolution, while Vertical Canvas is streaming? Is `obs_view` or a private canvas the better base? | 2 |
| 2. Independent audio | Can individual sources be captured, mixed selectively and sent to a separate encoder without affecting the stream? | 4 |
| 3. Combined pipeline | Can both be muxed into one file with stable A/V sync and correct timestamps at a custom resolution? | 5 |

---

## Phase 0: Development foundation

**Goal:** a working, empty OBS plugin.

- [x] Create the Git repository
- [x] Set up the project from the official OBS plugin template
- [x] Configure CMake presets with Qt6 and the frontend API enabled
- [x] Register plugin metadata and locale
- [x] Add basic logging
- [x] Confirm CI passes on all platforms
- [ ] Build Debug and RelWithDebInfo locally on Windows
- [ ] Confirm the plugin loads and unloads cleanly in OBS

**Done when:** the project compiles, OBS loads and unloads the plugin without crashing, and log lines appear in the OBS log.

## Phase 1: OBS integration and source discovery

**Goal:** a complete, live inventory of the OBS environment.

- [x] Log the main canvas and every frontend canvas, including Aitum Vertical, on load and on change
- [ ] Register an empty plugin dock
- [ ] `CanvasManager`: track canvases as they're added and removed
- [ ] Enumerate scenes for each canvas, plus items, nested scenes and groups
- [ ] Identify audio-only sources
- [ ] Track sources by UUID
- [ ] Handle frontend events and scene signals

**Done when:** every canvas, scene, nested source and audio source can be discovered, and renames, scene changes (on every canvas), scene-collection changes and canvas additions or removals are detected.

## Phase 2: Video routing engine

**Goal:** a recording composition that differs from the stream. This is the first major viability milestone.

- [ ] Build the private scene mirror
- [ ] Apply recording visibility rules
- [ ] Render through a separate `obs_view`
- [ ] Connect a video encoder and a basic output
- [ ] Test streaming and recording at the same time
- [ ] Run one selective pipeline per canvas (horizontal and vertical)

**Done when:** both outputs run together, `stream_only` sources are missing from the recording, and the original scene visibility is unchanged. This must hold on the main canvas and on the Aitum Vertical canvas.

## Phase 3: Scene synchronisation

**Goal:** the recording composition follows OBS without any manual work.

- [ ] Item add, remove and reorder
- [ ] Transform synchronisation
- [ ] Nested scenes and groups
- [ ] Filters
- [ ] Scene switching and Studio Mode
- [ ] Transitions: hard cuts first, then animated
- [ ] Non-main canvases: follow `channel_change` and Vertical's own transitions, including per-scene transition overrides
- [ ] Nested scenes that cross canvases

**Done when:** every edit in OBS reaches the recording, scene switches don't interrupt it, and transitions match.

## Phase 4: Independent audio routing

**Goal:** the recording audio contains only the routed sources.

- [ ] Audio capture callbacks
- [ ] Timestamped per-source buffers
- [ ] Mixer with routing, gain and mute
- [ ] Sample-rate conversion
- [ ] A/V synchronisation
- [ ] Parity with OBS fader and filter behaviour
- [ ] Test across different device configurations

**Done when:** sources route independently, the stream audio is unaffected, there's no measurable drift or glitching, and audio holds up across scene changes.

> This phase needs extra care. Don't rush it to meet a release date.

## Phase 5: Recording engine

**Goal:** a complete, independent recording pipeline.

- [ ] Encoder discovery and configuration
- [ ] Independent resolution, FPS and quality settings
- [ ] Container and path configuration
- [ ] Start, stop, pause and resume
- [ ] Error handling and file finalisation
- [ ] Simultaneous horizontal and vertical selective recordings
- [ ] Encoder session limit checks

**Done when:** the recording uses its own encoders, starting or stopping it never touches either stream, files are always valid, and failures are reported clearly.

## Phase 6: Dock interface

**Goal:** the finished user experience.

- [ ] A tab for each canvas (Horizontal, Vertical, …)
- [ ] Source list with video and audio routing controls, plus per-canvas overrides
- [ ] Search, filtering and group inheritance
- [ ] Profiles
- [ ] Recording controls and status
- [ ] Preview
- [ ] Hotkeys
- [ ] Settings dialog

**Done when:** everything can be managed without editing files, settings persist, and every control reflects the real engine state.

## Phase 7: Testing and optimisation

**Test matrix:** single-scene and large collections, nested scenes, groups, browser sources, game, window and display capture, webcams, media sources, audio devices, application audio capture, multiple devices, audio filters, transitions, Studio Mode, deleting a source while recording, switching scene collections, shutting OBS down while recording, and recordings longer than 4 hours.

**Dual output (Aitum Vertical Canvas, latest release and 1.6.0):** a vertical selective recording on its own, and both recordings together, each while both streams are live. Also: Vertical's own recording running at the same time, disabling or removing Vertical while a recording runs, changing the vertical resolution, per-scene transition overrides, and shared tracks between the two streams.

**Measure:** CPU, GPU, memory and dropped frames, with the plugin idle, previewing and recording.

## Phase 8: Packaging and release

- [ ] Windows installer and ZIP package
- [ ] Installation, configuration and troubleshooting docs
- [ ] Clean install and uninstall tests
- [ ] Tag `1.0.0`

---

## v1.0 release checklist

**Video**
- [ ] Independent stream and recording composition
- [ ] All routing states
- [ ] Nested scenes and groups
- [ ] Scene changes and transitions
- [ ] Source transforms and filters
- [ ] Recording preview

**Audio**
- [ ] Independent source routing
- [ ] Source gain and mute handling
- [ ] Audio timestamp synchronisation
- [ ] Multiple audio devices
- [ ] Audio filter and processing compatibility

**Recording**
- [ ] Independent video and audio encoders
- [ ] Independent recording quality
- [ ] Start, stop and pause
- [ ] Container and file settings
- [ ] Error handling and recovery

**Dual output**
- [ ] Selective recording of the Aitum Vertical canvas
- [ ] Horizontal and vertical selective recordings at the same time
- [ ] Vertical scene switches and transitions mirrored
- [ ] Per-canvas routing overrides
- [ ] Vertical Canvas streams and recordings unaffected

**Interface**
- [ ] Dockable OBS interface
- [ ] Source search and filtering
- [ ] Routing profiles
- [ ] Hotkeys
- [ ] Persistent configuration

**Reliability**
- [ ] Original scene visibility never changed unexpectedly
- [ ] Livestream never interrupted
- [ ] No resource leaks
- [ ] Long-duration recording tests passed
- [ ] Clean install and removal

## Risks

| Risk | Severity | Mitigation |
| --- | --- | --- |
| Audio synchronisation | High | Timestamped buffers and a dedicated mixer |
| Nested scene mirroring | High | Recursive scene graph |
| Browser source behaviour | High | Test shared source references early |
| Animated transitions | High | Private transition driven by frontend events |
| Source deletion while recording | High | Strict reference ownership and `remove` handling |
| OBS shutdown while recording | High | Controlled teardown on `OBS_FRONTEND_EVENT_EXIT` |
| OBS API compatibility | Medium | Pinned build dependencies and version tests |
| Encoder compatibility | Medium | Runtime encoder discovery |
| GPU overhead | Medium | Render profiling and a preview that costs nothing while closed |
| Vertical Canvas internal changes | Medium | Rely only on public OBS canvas APIs and test every Vertical release |
| Encoder sessions with dual output | Medium | Check sessions before starting, with clear errors and a software fallback |
| Shared audio tracks across canvases | Medium | Detect the conflict and explain it in the dock (ADR-0003) |
