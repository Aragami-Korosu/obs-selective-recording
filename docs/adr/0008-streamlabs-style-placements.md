# 0008. Streamlabs-style buttons set placements and hide Recording only from the stream

- **Status:** Accepted
- **Date:** 2026-11-18
- **Amends:** [0003](0003-stream-exclusion.md)

## Context

The project owner wants the Sources-list buttons to work like Streamlabs Desktop's selective recording. Streamlabs Desktop's source code shows how that works:

- The setting belongs to one **scene item**, a source in one scene (`streamVisible` and `recordingVisible`), not to the source everywhere.
- It covers **video only**. Audio is handled separately, with tracks.
- A click cycles **Both → Stream only → Recording only → Both**. There's no Off, and a locked item can't be changed.
- **Recording only takes the item off the stream by itself.** The eye icon isn't involved.

Streamlabs can hide an item from the stream alone because it runs a modified libobs with separate stream and recording visibility. Standard OBS has one visibility per scene item. So the only way to give Recording only the same one-click result is to hide the scene item in OBS, which [ADR-0003](0003-stream-exclusion.md) forbids.

## Decision

1. The Sources-list buttons, the dock's simple view and the `SetItemRoute` request set **placement routes**: the video of one scene item, stored as an `item_overrides` entry. They no longer set the source everywhere, and they don't change audio.
2. The cycle, the right-click choices and the tooltips match Streamlabs. Off stays available in the dock's Advanced view. A locked scene item's button is disabled.
3. **Exception to ADR-0003:** when the user chooses Recording only for a placement, the plugin hides that scene item in OBS, so it leaves the stream at once. It marks the item as hidden by the plugin (`selective_recording_hidden` in the item's private settings). When the placement leaves Recording only, the plugin shows it again, but only if it carries that mark. A placement the user hid themselves is never revealed.

Everything else in ADR-0003 stands:

- The plugin changes the live composition only in direct response to the user choosing Recording only, or leaving it, for that one placement.
- It never changes anything when routes are set in the dock's Advanced view or through `SetRoute`.
- It never changes outputs, audio tracks, filters, transforms or other canvases' settings.

## Consequences

- One click does what it does in Streamlabs, with no second step in OBS.
- The plugin can now change what viewers see, but only through that explicit action, only for that placement, and in the same way as clicking the eye icon. The wording rule in ADR-0003 is relaxed for these buttons: their tooltips say a Recording only placement is hidden in OBS.
- **Studio Mode:** OBS's "Duplicate Scene" puts a private copy of a scene on air, and that copy's items have new IDs. A placement with a route therefore carries its key in its scene item's private settings (`selective_recording_placement`). libobs copies those settings into the copy, so the copy's items find the original's route. The same applies to a scene duplicated with OBS's **Duplicate** command: its placements start with the original's settings until they're changed.
- **Sound:** a Recording only placement is hidden live, so OBS mutes it on the stream, but the recording still includes its sound, because the recording mix takes audio from each source directly ([ADR-0004](0004-independent-audio-mix.md)). Stream only and Both don't change audio; a source's audio route is set in the dock's Advanced view, as in Streamlabs.
- Placement routes are stored per scene item, so the same source in two scenes, or twice in one scene, can be set differently.
