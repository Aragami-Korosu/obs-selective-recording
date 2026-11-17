# OBS Selective Recording

Record a different mix of sources than you stream. You decide, for every source and separately for video and audio, what goes into the recording. Your stream stays exactly as you built it. It works on every canvas, including [Aitum Vertical](https://github.com/Aitum/obs-vertical-canvas), so horizontal and vertical can each have their own selective recording.

## What it does

Most streamers want their recording to be a little different from what goes live. You might drop alerts and chat overlays from the YouTube upload, keep copyrighted music off the VOD, or record a camera you don't show on stream. Selective Recording adds a dock where you choose what goes into the recording for each source:

| Route | In the recording | Your stream |
| --- | :---: | --- |
| **Both** | ✅ | Unchanged |
| **Stream only** | — | Unchanged |
| **Rec only** | ✅ | Hide it in OBS. The plugin warns you if it's still live. |
| **Off** | — | Hide it in OBS. The plugin warns you if it's still live. |

> **Your stream is never touched.** The plugin only controls its own recordings. It never hides, mutes or changes anything in your live scenes, your streams or other plugins' outputs, so a recording problem can't change what your viewers see.

### Works with dual output

Streamlabs turns off Selective Recording when Dual Output is on. This plugin is built for that setup: with Aitum Vertical Canvas you get a selective recording for the **horizontal** canvas and one for the **vertical** canvas. You can run either or both while both streams are live. If one recording fails, the other keeps going.

## Features

- A Streamlabs-style button on every source in OBS's own Sources list, plus a simple dock view of the current scene
- An advanced view with every scene and separate video and audio routing
- Routes for every canvas, for one canvas only, or for one placement of a source
- Warnings when a source you meant to keep off the stream is still live, including audio still on the stream's track
- A recording composition that follows your scenes, nested scenes, groups, transforms, transitions and Studio Mode automatically
- Independent recording audio built per source, with OBS's fader, mute and sync offset, plus per-source recording volume
- Independent recording settings for each canvas: encoder (NVENC, AMF, QuickSync, x264), resolution, frame rate, quality or bitrate, folder and container (MKV, MP4, MOV)
- Pause and resume, hotkeys, and a live preview of exactly what's being recorded
- Routing profiles saved with your scene collection, plus import and export
- Crash recovery: interrupted MKV recordings stay playable and are listed when OBS starts again
- Automation through obs-websocket (Stream Deck and others)
- No accounts, services or Streamlabs required

## Requirements

| | |
| --- | --- |
| **OS** | Windows 10 or 11 (x64). macOS and Linux builds are produced but not yet tested. |
| **OBS Studio** | 31.1 or newer (tested on 33.0.1) |
| **Aitum Vertical Canvas** *(optional)* | 1.6.0 or newer (tested on 1.6.7) |

## Installation

### Installer (recommended)

1. Close OBS.
2. Download `obs-selective-recording-<version>-windows-x64-installer.exe` from [Releases](../../releases) and run it.
3. Start OBS. The **Selective Recording** dock opens on first run (afterwards it's under **Docks**), and a recording button appears next to each source in the Sources list.

The installer checks your OBS version and won't run while OBS is open. To uninstall, use **Settings → Apps → Installed apps → OBS Selective Recording**. Your settings are kept in case you reinstall.

### ZIP (manual or portable OBS)

Extract `obs-selective-recording-<version>-windows-x64.zip` into `C:\ProgramData\obs-studio\plugins\`, so the DLL ends up at `...\plugins\obs-selective-recording\bin\64bit\obs-selective-recording.dll`.

For a portable OBS, copy `bin\64bit\*` into OBS's `obs-plugins\64bit` folder and `data\*` into `data\obs-plugins\obs-selective-recording`.

## Quick start

Like Streamlabs, every source in OBS's **Sources** list gets a small button next to its eye icon:

| Symbol | Meaning |
| --- | --- |
| Blue radio waves around a red dot | **Both**: stream and recording |
| Blue radio waves (broadcast) | **Stream**: left out of the recording |
| Red record button | **Recording**: in the recording only (hide it in OBS to keep it off the stream) |
| Grey slashed circle | **Off** |

1. Click the button next to your alerts or chat overlay until it shows the **broadcast** symbol. It's now left out of the recording.
2. To record a source that isn't on stream, click until it shows the **record** symbol, then hide it with the eye icon.
3. In the **Selective Recording** dock, click **Start recording**, or **Start all recordings** to record every canvas.

Right-click a button for **Off** and more. The dock shows the same buttons for the current scene, plus a tab for Aitum Vertical. Its **Advanced** view has separate video and audio controls and per-canvas options.

The [user guide](docs/USER-GUIDE.md) covers everything else.

## Documentation

| Document | Contents |
| --- | --- |
| [User guide](docs/USER-GUIDE.md) | The dock, routes, warnings, settings and hotkeys |
| [Troubleshooting](docs/TROUBLESHOOTING.md) | Common problems and fixes |
| [Automation API](docs/API.md) | obs-websocket requests and events |
| [Specification](docs/SPECIFICATION.md) | Requirements and known limitations |
| [Architecture](docs/ARCHITECTURE.md) | How it works inside |
| [Configuration](docs/CONFIGURATION.md) | Where settings are stored, and their format |
| [Roadmap](docs/ROADMAP.md) | Status and what's next |
| [Building](docs/BUILDING.md) | Building and testing from source |
| [Decision records](docs/adr/) | Key design decisions and why |

## License

Licensed under the [GNU General Public License v2.0 or later](LICENSE), the same license as OBS Studio.

This project is not affiliated with the OBS Project, Streamlabs or Aitum.
