# 0003. How `recording_only` removes a source from the stream

- **Status:** Proposed. Needs a decision before Phase 2 is complete.
- **Date:** 2026-11-17

## Context

The specification has two requirements that pull against each other:

- `recording_only` and `off` must keep a source **out of the stream**.
- The plugin must not modify the live scene, and it must keep the normal OBS streaming workflow intact.

The stream renders OBS's main canvas and mixes audio from OBS's tracks. The plugin owns neither. So a source can only be excluded from the stream if something in OBS's own state changes.

## Options

**A. The plugin manages stream exclusion (recommended).**
For video, the plugin hides the original scene item. For audio, it removes the source from the tracks used by the stream. The plugin records every change it makes, re-applies it as needed, and reverts it when the plugin is disabled or the route changes. The private mirror ignores the original's visibility for these items.

- ✅ Matches what users expect from "recording only". There's one control and it just works.
- ⚠️ The plugin changes OBS state. This must be reversible, and the dock must show it clearly.
- ⚠️ The plugin has to deal with users toggling the same item by hand.

**B. The user manages stream exclusion.**
The user hides the source in OBS. The mirror shows it in the recording anyway, and the dock warns the user when OBS state doesn't match the route.

- ✅ The plugin never writes to OBS state.
- ❌ Each route takes two steps, and it's easy to get wrong.

**C. Replace the stream output.**
Render a second private composition just for the stream.

- ❌ Doubles the rendering cost and takes over OBS's core streaming path. Out of scope.

## Decision

Pending. Option A is recommended.
