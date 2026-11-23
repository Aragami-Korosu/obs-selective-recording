# OBS Selective Recording

Choose which sources go to your **stream**, which go to your **recording**, and which go to both. This applies to video and audio separately, and the scene you stream stays exactly as you built it.

## What it does

Most streamers want their recording to differ slightly from their live output. You might drop alerts and chat overlays from a YouTube upload, or keep copyrighted music off the VOD while your stream still hears it. OBS Selective Recording adds a dock where you choose what goes into the recording for each source:

| Route | In the recording | Your stream |
| --- | :---: | --- |
| **Both** | ✅ | Unchanged |
| **Stream only** | — | Unchanged |
| **Recording only** | ✅ | Hide it in OBS. The plugin warns you if it's still live. |
| **Off** | — | Hide it in OBS. The plugin warns you if it's still live. |

You set video and audio routes separately. For example, your game capture can appear in the recording while its audio is left out.

> **Your stream is never touched.** The plugin only controls its own recordings. It never hides, mutes or changes anything in your live scenes, your stream, or other plugins' outputs. That way, a recording problem can't change what your viewers see.

### Works with dual output

Streamlabs turns off Selective Recording when Dual Output is enabled. This plugin is designed for that setup. With [Aitum Vertical Canvas](https://github.com/Aitum/obs-vertical-canvas), you get a separate selective recording for the **horizontal** and the **vertical** canvas. You can run either one, or both at once, while both streams stay live. If one recording fails, the other keeps going.

## Planned features

- A dockable panel listing every source, with separate video and audio routing.
- A private recording composition that follows your scenes, transforms, filters and transitions automatically.
- An independent recording audio mix built per source, separate from your OBS track setup.
- Dual-output support: a separate selective recording for each canvas (horizontal and vertical), with routing rules shared between them and per-canvas exceptions.
- Independent recording settings: encoder, resolution, frame rate, bitrate and container.
- A live preview of the recording composition.
- Routing profiles, hotkeys and settings that persist across restarts.
- Recovery: interrupted recordings stay playable and are flagged the next time OBS starts.
- No external services, accounts or Streamlabs dependency.

See the [specification](docs/SPECIFICATION.md) for the full requirements.

## Requirements

| | |
| --- | --- |
| **OS** | Windows 10 or 11 (x64). Linux and macOS are planned. |
| **OBS Studio** | 31.1 or newer |
| **Aitum Vertical Canvas** *(optional)* | 1.6.0 or newer |

## Installation

No releases yet. Once version 1.0 ships, installers will be published on the [Releases](../../releases) page.

To build from source, see [docs/BUILDING.md](docs/BUILDING.md).

## Documentation

| Document | Contents |
| --- | --- |
| [Specification](docs/SPECIFICATION.md) | Functional requirements and routing behaviour |
| [Architecture](docs/ARCHITECTURE.md) | How the plugin works internally |
| [Roadmap](docs/ROADMAP.md) | Development phases and the v1.0 release checklist |
| [Configuration](docs/CONFIGURATION.md) | Settings schema and persistence rules |
| [Building](docs/BUILDING.md) | Compiling and installing from source |
| [Decision records](docs/adr/) | Key design decisions and why they were made |

## License

Licensed under the [GNU General Public License v2.0 or later](LICENSE), the same license as OBS Studio.

This project is not affiliated with OBS Project, Streamlabs or Aitum.
