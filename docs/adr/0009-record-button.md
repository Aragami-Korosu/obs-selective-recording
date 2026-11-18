# 0009. OBS's Record button records with Selective Recording

- **Status:** Accepted
- **Date:** 2026-11-18
- **Amends:** [0003](0003-stream-exclusion.md)

## Context

In Streamlabs Desktop, the main Record button *is* the selective recording. In this plugin, selective recordings were started from the dock, and OBS's own Record button kept making OBS's normal recording. That recording shows exactly what's live, so a Recording only source (hidden from the stream) is missing from it. In testing, the project owner pressed OBS's Record button, found the source missing, and reasonably concluded Recording only didn't work.

The options were:

- **A.** OBS's Record button starts Selective Recording too, alongside OBS's normal recording (two files).
- **B.** OBS's Record button records with Selective Recording instead of OBS's normal recording, like Streamlabs.
- **C.** Keep them separate.

The project owner chose **B**.

## Decision

By default, OBS's Record button (`recordButton` in the Controls dock) starts and stops the selective recordings of every enabled canvas. The plugin catches the button's clicks before OBS sees them, so **OBS's own recording is never started** and its output is never touched. The button shows the selective recordings' state ("Start Recording" / "Stop Recording").

- While OBS's own recording is running, for example after its hotkey, the button behaves normally so it can be stopped.
- OBS's Start Recording hotkey, obs-websocket's `StartRecord`, and other plugins still use OBS's normal recording.
- The setting **Settings → Interface → OBS's Record button records with Selective Recording** turns this off.
- If OBS's layout changes and the button can't be found, it keeps OBS's normal behaviour.

## Consequences

- One button, like Streamlabs. A Recording only source is in the file you get from the Record button.
- OBS's recording output still belongs to OBS: the plugin never starts, stops or reconfigures it. It only decides what the button does.
- OBS's status bar timer and pause button follow OBS's own recording, so they don't run during a selective recording. The dock shows the selective recordings' time and has its own pause.
- This relies on OBS's internal widget name and is checked by hand on each OBS update; the dock and hotkeys always work.
