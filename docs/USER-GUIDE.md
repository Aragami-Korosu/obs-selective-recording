# User Guide

## The idea in one minute

Your stream and your recording don't have to show the same thing. For every source, you choose what goes into the **recording**:

| Route | In the recording | Your stream |
| --- | :---: | --- |
| **Both** | ✅ | Unchanged |
| **Stream only** | — | Unchanged |
| **Rec only** | ✅ | Hide it in OBS yourself |
| **Off** | — | Hide it in OBS yourself |

Video and audio are set separately. The plugin **never changes your stream**. It records its own copy of your scenes, so anything you want kept *off the stream* still has to be hidden in OBS, and the dock warns you when something you meant to hide is still live.

## Buttons in the Sources list

Each source in OBS's **Sources** list has a round button next to its eye icon:

| Button | Route |
| :---: | --- |
| Half blue, half red | **Both** |
| Blue | **Stream** (left out of the recording) |
| Red | **Recording** (recording only) |
| Grey ring | **Off** |
| White centre | Video and audio are set differently (change them in the dock's Advanced view) |

- **Click** to cycle Both → Stream → Recording. One click sets the source's video and audio together, on every canvas.
- **Right-click** for Off, to open the dock, or to hide these buttons. You can also turn them off in Settings → Interface.

The buttons only change what goes into the recording. They never change OBS: the eye icon is still how you show or hide things on stream.

> OBS doesn't offer plugins an official way to add buttons to its Sources list, so these attach to it directly. If a future OBS version changes that list, the buttons will simply stop appearing until the plugin is updated. The dock always works.

## The dock

On first run, the **Selective Recording** dock opens as a floating window. You can attach it to the side of OBS like any other dock. If you close it, reopen it from **Docks → Selective Recording**.

- **Enabled**: when unchecked, recordings include everything exactly as it appears live.
- **Profile**: switch between saved sets of routes. Use **⋯** to create, duplicate, rename, delete, import or export profiles. Profiles are saved with the scene collection.
- **Tabs**: one for each canvas. With Aitum Vertical installed, you'll see **Main** and **Aitum Vertical**.

### Simple view

By default, each tab shows the **current scene** with the same one-click buttons as the Sources list. On the Main tab, that's the scene selected in OBS (the preview scene in Studio Mode). On the Aitum Vertical tab, it's the vertical scene on air. Right-click a button for Off or to switch to Advanced.

### Advanced view: setting routes

Click **Advanced** to see every scene on the canvas, with separate **Video** and **Audio** buttons:

- **Click** a button to cycle through Both → Stream only → Rec only → Off.
- **Right-click** for more options:
  - **Every canvas** (the normal choice)
  - **Only on this canvas**, for example to keep a webcam out of the vertical recording only
  - **Only this placement**, when the same source appears in several scenes
  - **Recording volume…**, which adds gain to the recording only (audio)
  - **Reset to default**
- **Italic** buttons show the profile default. A **•** marks an override for one canvas or one placement.
- Select several rows and right-click anywhere in the list to change them all at once.

Search and the filter menu help in big setups: *Left out of recording*, *Recording only*, *Has warnings*.

### Warnings ⚠

A ⚠ appears when a source is set to **Rec only** or **Off** but is still reaching your stream:

- **Still visible live:** hide it in the scene (the eye icon).
- **Still on the stream's audio track:** open **Edit → Advanced Audio Properties** and untick the track your stream uses.
- **Shared track:** your horizontal and vertical streams use the same audio track, so removing the source from it would remove it from both. Give each stream its own track.

Hover the ⚠ for exactly what to change. Warnings can be turned off in Settings.

### Recording

- **Start recording / Stop recording** for this canvas, or **Start all recordings** at the bottom.
- **Pause / Resume**: paused time is left out of the file.
- **Preview** shows exactly what's being recorded. It only uses resources while it's open.
- The status line shows duration, size and dropped frames. Hover it for encoder and audio buffering details. **Open folder** shows the file.

Each canvas records on its own. If one fails (for example its disk fills up), the other keeps recording and your streams aren't affected.

## Settings

**General** (all canvases):
- **Container:** MKV is recommended because it survives crashes. Remux to MP4 later with **File → Remux Recordings**.
- **Audio encoder and bitrate**, and the **filename format** (same codes as OBS).
- **Fallback to x264** if a hardware encoder can't start. Streaming on two canvases while recording both can exceed your graphics card's encoder limit.
- **Warnings** and the **recovery check**.

**One tab per canvas:**
- **Allow selective recording on this canvas**
- **Resolution:** the canvas size, or a custom size
- **Frame rate:** the same as OBS, or a half, third or quarter of it
- **Video encoder:** Automatic picks NVENC, AMF or QuickSync if available, otherwise x264
- **Rate control:** constant quality (lower number = better) or constant bitrate
- **Recording folder:** empty means OBS's recording folder
- **Filename suffix:** vertical recordings get " Aitum Vertical" added by default

Changes apply to the next recording.

## Hotkeys

In **Settings → Hotkeys** (OBS's settings), search for "selective":
- **Start/stop selective recording (canvas)**
- **Pause/resume selective recording (canvas)**
- **Start/stop all selective recordings**

## After a crash

If OBS stops unexpectedly while recording, the dock shows the interrupted files the next time it starts. MKV files play as they are. **Show files** opens the folder, and **Dismiss** clears the notice.

## Automation

Stream Deck and other obs-websocket tools can control the plugin. See [API.md](API.md).
