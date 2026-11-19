# 0010. A source's audio follows its video

- **Status:** Accepted
- **Date:** 2026-11-19
- **Amends:** [0004](0004-independent-audio-mix.md), [0008](0008-streamlabs-style-placements.md)

## Context

Each source had a separate video route and audio route, and the dock's Advanced view had a Video column and an Audio column. In testing, the project owner found this surprising in two ways:

- Setting Game Capture to **Stream only** left its picture out of the recording but kept its game audio in.
- Choosing **Recording only** in the Advanced view didn't take the source off the stream. That view never changed OBS ([ADR-0003](0003-stream-exclusion.md)); only the Sources-list buttons hid a Recording only placement ([ADR-0008](0008-streamlabs-style-placements.md)).

The project owner asked for the Audio column to go, with audio following video, and for Recording only to work everywhere.

## Decision

- **One route per source or placement covers video and audio.** The recording takes a source's audio exactly when the source is visible in the recording, meaning it's active in the recording's private canvas ([ADR-0006](0006-private-canvas-rendering.md)). The independent audio mix ([ADR-0004](0004-independent-audio-mix.md)) is unchanged; only its inclusion rule changes.
- **Audio devices** (Settings → Audio: mic, desktop audio) aren't in any scene, so they keep a route of their own, shown under *Audio devices* in the Advanced view.
- **Recording only works the same everywhere.** Changing a source's or canvas's route (the Advanced view, or `SetRoute`) applies the placement rule from ADR-0008 to every placement whose route changed. Recording only hides the placement in OBS. Leaving Recording only shows it again, but only if the plugin hid it.
- **Off** still leaves the stream to the user, as before.
- **Older settings.** A saved audio route is folded in when a scene collection loads. An audio-only source (mic, desktop audio, application audio) keeps it as its one route. For a source with video, the video route wins. `SetRoute` still accepts `audio`, as a name for the same route.

## Consequences

- One click sets what a source contributes, picture and sound together, as in Streamlabs.
- A source can no longer be in the recording silently, or heard but not seen. The recording volume (gain) stays per source.
- OBS mutes a hidden scene item on the stream, so hiding a Recording only placement also takes its sound off the stream. An audio device set to Recording only still has to be removed from the stream's track by the user; the dock warns until it is.
- When the same source is in several places on air at once, its audio is recorded if any of those placements is in the recording.
