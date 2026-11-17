# Roadmap

Each phase must meet its acceptance criteria before the next one starts. The estimates assume one developer and are for planning only. They aren't commitments.

**Estimated total: 12–18 engineering weeks.**

| Phase | Name | Estimate | Status |
| --- | --- | --- | --- |
| 0 | Development foundation | 1 week | 🟡 Design done. Runtime load test pending. |
| 1 | OBS integration and source discovery | 1 week | ⚪ Next |
| 2 | Video routing engine | 2–3 weeks | ⚪ Not started |
| 3 | Scene synchronisation | 2 weeks | ⚪ Not started |
| 4 | Independent audio routing | 3–4 weeks | ⚪ Not started |
| 5 | Recording engine | 1–2 weeks | ⚪ Not started |
| 6 | Dock interface | 1–2 weeks | ⚪ Not started |
| 7 | Testing and optimisation | 1–2 weeks | ⚪ Not started |
| 8 | Packaging and release | 1 week | ⚪ Not started |

## Decisions

| ADR | Decision | Status |
| --- | --- | --- |
| [0001](adr/0001-obs-view-over-canvas-api.md) | Render with `obs_view` | Accepted, partly superseded by 0005 |
| [0002](adr/0002-private-scene-mirror.md) | Private scene mirror with shared sources | Accepted |
| [0003](adr/0003-stream-exclusion.md) | Never modify a live composition; stream exclusion is user-managed | Accepted |
| [0004](adr/0004-independent-audio-mix.md) | Independent per-source audio mix | Accepted, pending POC 2 |
| [0005](adr/0005-multi-canvas-support.md) | One pipeline per canvas, including Aitum Vertical | Accepted |

## Proofs of concept

These three questions decide whether the architecture is sound. They come before any significant UI work.

| POC | Question | Feeds phase |
| --- | --- | --- |
| 1. Independent video | Can a private scene tree be rendered through `obs_view` while the original composition keeps streaming? Does it work for a vertical canvas scene too, at a vertical resolution, while Vertical Canvas is streaming? Is `obs_view` or a private canvas the better base? Can the frame rate differ from OBS's? | 2 |
| 2. Independent audio | Can individual sources be captured, mixed selectively and sent to a separate encoder without affecting the stream? Which OBS mixer stages (fader, balance, sync offset, push-to-talk, filters) are already applied when the capture callback runs? | 4 |
| 3. Combined pipeline | Can both be muxed into one file with stable A/V sync and correct timestamps at a custom resolution? Is there a crash-resilient MP4 mode we can use? | 5 |

---

## Phase 0: Development foundation

**Goal:** a working, empty OBS plugin.

- [x] Create the Git repository
- [x] Set up the project from the official OBS plugin template
- [x] Configure CMake presets with Qt6 and the frontend API enabled
- [x] Register plugin metadata and locale
- [x] Add basic logging
- [x] CI builds on Windows, macOS and Ubuntu
- [ ] Build Debug and RelWithDebInfo locally on Windows
- [ ] **Load test in OBS 32.2.2**: the plugin loads, unloads, and logs both the main canvas and the `Aitum Vertical` canvas

**Done when:** the project compiles, OBS loads and unloads the plugin without crashing, and the log lists every canvas.

## Phase 1: OBS integration and source discovery

**Goal:** a complete, live, read-only inventory of the OBS environment. Build the inventory before the dock, and keep the dock minimal in this phase.

1. **Canvases**
   - [x] Log the main canvas and every frontend canvas on load and when canvases change
   - [ ] `CanvasManager`: track canvases as they're added and removed at runtime (NFR-009)
2. **Inventory**
   - [ ] Enumerate scenes for each canvas, plus items, nested scenes (including across canvases) and groups
   - [ ] Identify audio-only sources and global audio devices
   - [ ] Track sources by UUID, and detect renames
   - [ ] Follow the program scene on each canvas (frontend events for main, `channel_change` for others)
   - [ ] Find each canvas's stream outputs and their audio tracks, read-only, as groundwork for NFR-006 and NFR-008
   - [ ] Write the inventory to the log on demand, to check it against real setups
3. **Dock (minimal)**
   - [ ] Register an empty dock showing the canvas list and source inventory, read-only

**Done when:** every canvas, scene, nested source and audio source can be discovered. Renames, scene changes on every canvas, scene-collection changes, and canvases being added or removed are all detected. Nothing in OBS is modified (NFR-001).

## Phase 2: Video routing engine

**Goal:** a recording composition that differs from the stream. This is the first major viability milestone.

- [ ] Build the private scene mirror
- [ ] Apply recording visibility rules (Both follows the original; Recording only is always shown; Stream only and Off are always hidden)
- [ ] Render through a separate `obs_view`
- [ ] Connect a video encoder and a basic output
- [ ] Run one selective pipeline per canvas (horizontal and vertical)
- [ ] **Original-state test:** snapshot every original scene, run every plugin operation, and check nothing changed (NFR-001)
- [ ] Apply route changes live, without restarting anything (NFR-004)

**Done when:** recording and streaming run together on both canvases, Stream only sources are missing from the recording, Recording only sources appear in it, and the original-state test passes.

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

**Goal:** the recording audio contains only the routed sources, and is built without touching OBS's tracks.

- [ ] POC 2: capture callback behaviour and mixer-stage parity
- [ ] Timestamped per-source buffers
- [ ] Mixer with routing, gain and mute
- [ ] Sample-rate conversion
- [ ] A/V synchronisation
- [ ] Parity with OBS fader and filter behaviour
- [ ] Separate capture and mix for each canvas
- [ ] Test across different device configurations

**Done when:** sources route independently, the stream audio and track setup are unchanged, there's no measurable drift or glitching, and audio holds up across scene changes.

> This phase needs extra care. Don't rush it to meet a release date.

## Phase 5: Recording engine

**Goal:** a complete, independent recording pipeline for each canvas.

- [ ] Encoder discovery and configuration
- [ ] Encoder capacity check before starting (NFR-007)
- [ ] Independent resolution and quality settings
- [ ] Container and path configuration
- [ ] Start, stop, pause and resume for each canvas
- [ ] Failure handling for each canvas: output `stop` codes, the **Failed** state, and resource release (NFR-002, NFR-003)
- [ ] Session markers and interrupted-recording detection (FR-009)
- [ ] Clean stop on canvas removal and on OBS exit (NFR-009)
- [ ] Simultaneous horizontal and vertical selective recordings

**Done when:** each recording uses its own encoders, and starting, stopping or failing never touches any stream or another canvas's recording. Files are always valid, and failures are reported clearly.

## Phase 6: Dock interface

**Goal:** the finished user experience.

- [ ] A tab for each canvas (Horizontal, Vertical, …)
- [ ] Source list with video and audio routing controls, plus per-canvas overrides
- [ ] Route wording that describes the effect on the recording only (ADR-0003)
- [ ] Live-visibility warnings for video and audio (NFR-008)
- [ ] Audio track conflict warnings (NFR-006)
- [ ] Encoder capacity and recovery notices (NFR-007, FR-009)
- [ ] Search, filtering and group inheritance
- [ ] Profiles, with shared settings and per-canvas settings (NFR-010)
- [ ] Recording controls and status for each canvas
- [ ] Preview
- [ ] Hotkeys
- [ ] Settings dialog

**Done when:** everything can be managed without editing files, settings persist, and every control and warning reflects the real engine and OBS state.

## Phase 7: Testing and optimisation

**Test matrix:** single-scene and large collections, nested scenes, groups, browser sources, game, window and display capture, webcams, media sources, audio devices, application audio capture, multiple devices, audio filters, transitions, Studio Mode, deleting a source while recording, switching scene collections, shutting OBS down while recording, and recordings longer than 4 hours.

**Dual output (Aitum Vertical Canvas, latest release and 1.6.0):** a vertical selective recording on its own, and both recordings together, each while both streams are live. Also: Vertical's own recording and replay buffer running at the same time, disabling or removing Vertical while a recording runs, changing the vertical resolution, per-scene transition overrides, and shared tracks between the two streams. Check that every Vertical feature still works (NFR-005).

**Fault injection:** force an encoder failure, fill the disk, remove the destination, and kill OBS. Do each while one canvas is recording and while both are, with both streams live. Check that streams continue (NFR-002), the other canvas continues (NFR-003), and interrupted files are detected and playable (FR-009).

**Measure:** CPU, GPU, memory and dropped frames, with the plugin idle, previewing and recording.

## Phase 8: Packaging and release

- [ ] Windows installer and ZIP package
- [ ] Installation, configuration and troubleshooting docs
- [ ] Clean install and uninstall tests
- [ ] Tag `1.0.0`

---

## v1.0 release checklist

**Video**
- [ ] Independent recording composition on each canvas
- [ ] All routing states, with wording that matches ADR-0003
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
- [ ] Failure handling for each canvas
- [ ] Interrupted-recording recovery

**Dual output**
- [ ] Selective recording of the Aitum Vertical canvas
- [ ] Horizontal and vertical selective recordings at the same time
- [ ] Vertical scene switches and transitions mirrored
- [ ] Per-canvas routing overrides and settings
- [ ] Every Aitum Vertical feature unaffected (NFR-005)
- [ ] Canvases added or removed at runtime handled (NFR-009)

**Interface**
- [ ] Dockable OBS interface
- [ ] Live-visibility and audio-track warnings
- [ ] Source search and filtering
- [ ] Routing profiles
- [ ] Hotkeys
- [ ] Persistent configuration

**Reliability**
- [ ] Original-state test passes: no live composition is ever modified (NFR-001)
- [ ] No stream is interrupted under fault injection (NFR-002)
- [ ] A failure on one canvas doesn't affect another (NFR-003)
- [ ] Route changes never restart anything (NFR-004)
- [ ] No resource leaks
- [ ] Long-duration recording tests passed
- [ ] Clean install and removal

## Risks

| Risk | Severity | Mitigation |
| --- | --- | --- |
| Audio synchronisation | High | Timestamped buffers and a dedicated mixer |
| Nested scene mirroring | High | Recursive scene graph |
| Browser source behaviour | High | Test shared source references early |
| Animated transitions | High | Private transition driven by frontend and canvas signals |
| Source deletion while recording | High | Strict reference ownership and `remove` handling |
| OBS shutdown while recording | High | Controlled teardown on `OBS_FRONTEND_EVENT_EXIT`, plus session markers |
| A plugin crash takes down OBS and the stream | High | No exceptions across callbacks, defensive handle checks, fault injection and long-run tests |
| User forgets to hide Recording only sources from the stream | Medium | Live-visibility warnings (NFR-008), on by default |
| OBS API compatibility | Medium | Pinned build dependencies and version tests |
| Encoder compatibility | Medium | Runtime encoder discovery |
| GPU overhead | Medium | Render profiling and a preview that costs nothing while closed |
| Vertical Canvas internal changes | Medium | Rely only on public OBS canvas APIs and test every Vertical release |
| Encoder sessions with dual output | Medium | Capacity check before starting, with clear errors and a software fallback (NFR-007) |
| Shared audio tracks between streams | Low | Affects only the user's stream setup, not the recording. Detected and explained (NFR-006). |
