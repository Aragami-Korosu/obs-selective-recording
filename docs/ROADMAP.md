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
| 1. Independent video | Can a private scene tree be rendered through `obs_view` while the original composition keeps streaming? | 2 |
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

- [ ] Register an empty plugin dock
- [ ] Enumerate scenes, items, nested scenes and groups
- [ ] Identify audio-only sources
- [ ] Track sources by UUID
- [ ] Handle frontend events and scene signals

**Done when:** every scene, nested source and audio source can be discovered, and renames, scene changes and scene-collection changes are detected.

## Phase 2: Video routing engine

**Goal:** a recording composition that differs from the stream. This is the first major viability milestone.

- [ ] Build the private scene mirror
- [ ] Apply recording visibility rules
- [ ] Render through a separate `obs_view`
- [ ] Connect a video encoder and a basic output
- [ ] Test streaming and recording at the same time

**Done when:** both outputs run together, `stream_only` sources are missing from the recording, and the original scene visibility is unchanged.

## Phase 3: Scene synchronisation

**Goal:** the recording composition follows OBS without any manual work.

- [ ] Item add, remove and reorder
- [ ] Transform synchronisation
- [ ] Nested scenes and groups
- [ ] Filters
- [ ] Scene switching and Studio Mode
- [ ] Transitions: hard cuts first, then animated

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

**Done when:** the recording uses its own encoders, starting or stopping it never touches the stream, files are always valid, and failures are reported clearly.

## Phase 6: Dock interface

**Goal:** the finished user experience.

- [ ] Source list with video and audio routing controls
- [ ] Search, filtering and group inheritance
- [ ] Profiles
- [ ] Recording controls and status
- [ ] Preview
- [ ] Hotkeys
- [ ] Settings dialog

**Done when:** everything can be managed without editing files, settings persist, and every control reflects the real engine state.

## Phase 7: Testing and optimisation

**Test matrix:** single-scene and large collections, nested scenes, groups, browser sources, game, window and display capture, webcams, media sources, audio devices, application audio capture, multiple devices, audio filters, transitions, Studio Mode, deleting a source while recording, switching scene collections, shutting OBS down while recording, and recordings longer than 4 hours.

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
