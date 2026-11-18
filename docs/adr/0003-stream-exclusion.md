# 0003. Preserve the live composition; stream exclusion is user-managed

- **Status:** Accepted, amended by [0008](0008-streamlabs-style-placements.md) (the Streamlabs-style buttons hide a Recording only placement in OBS)
- **Date:** 2026-11-17

## Context

The routing model lets users mark a source as **Recording only** or **Off**. Both imply the source should not be in the stream. But the stream renders OBS's canvases and mixes audio from OBS's tracks, and the plugin owns none of them. Keeping a source out of the stream therefore means changing OBS state that viewers see.

We considered three options:

- **A. Plugin-managed exclusion.** The plugin hides the original scene items and removes sources from the stream's audio tracks.
- **B. User-managed exclusion.** The plugin controls only its own recordings. The user hides sources from the stream in OBS, and the plugin warns when what OBS is doing doesn't match the user's intent.
- **C. Replace the stream output** with a plugin-rendered composition.

## Decision

**Option B.** The plugin never modifies the live composition of any canvas.

The plugin controls only what goes into its own recordings. It must never:

- Hide, show, reorder, transform or remove scene items in any original scene, on any canvas.
- Change a source's audio mixer tracks, volume, mute, monitoring or filters.
- Create, modify, start or stop any output it didn't create. That includes OBS's stream, recording, replay buffer and virtual camera, and the stream, recording and replay buffer of Aitum Vertical or any other plugin.
- Rename, reconfigure or remove a canvas it didn't create.

This applies to every canvas, including the main canvas and Aitum Vertical.

### What each route means

The route tells the plugin two things: whether the source goes into the recording, and what the user *intends* for the stream. The plugin acts only on the first. It uses the second only to decide when to warn.

| Route | Recording (plugin-controlled) | Stream (user-controlled in OBS) | Warning when |
| --- | --- | --- | --- |
| **Both** | Included | Unchanged | Never |
| **Stream only** | Excluded | Unchanged | Never |
| **Recording only** | Included | User should hide it | It's still live on that canvas |
| **Off** | Excluded | User should hide it | It's still live on that canvas |

"Live on a canvas" means:

- **Video:** the source has a visible scene item, either directly or through a visible group or nested scene, in a scene on that canvas that is on air.
- **Audio:** the source is enabled on at least one mixer track that feeds an active or configured stream output for that canvas, and it isn't muted.

The plugin reads this state but never changes it.

### Wording

The interface and documentation must never say that the plugin removes a source from the stream. Each route must be described by its effect on the recording. Where a route implies the user should hide the source from the stream, the interface must say that's done in OBS.

## Consequences

- Enabling, disabling or misconfiguring the plugin can't change what viewers see. A recording failure can't affect the broadcast through anything this plugin owns.
- Aitum Vertical and other plugins keep full control of their canvases and outputs.
- **Recording only** and **Off** take two steps: set the route in the dock, then hide the source in OBS. The warning system ([SPECIFICATION](../SPECIFICATION.md), NFR-008) makes it hard to miss the second step.
- **Shared audio tracks** limit the stream, not the recording. The recording audio mix is built from each source directly ([ADR-0004](0004-independent-audio-mix.md)), so it never depends on OBS's tracks. If two streams share a track, the user can't remove a source's audio from one stream without removing it from the other. That's a limit of OBS's track setup, not of selective recording. The plugin detects it and explains it (NFR-006).
- The warning checks need read-only access to scene item visibility, mixer flags, and each stream output's audio encoders. This is in scope for Phase 1 (inventory) and Phase 6 (dock warnings).
