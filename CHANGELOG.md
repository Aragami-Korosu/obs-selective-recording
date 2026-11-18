# Changelog

All notable changes to this project are documented here.

The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project uses [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0]

First release.

### Added

- Selective recording for every OBS canvas, including Aitum Vertical Canvas 1.6+, with horizontal and vertical recordings able to run at the same time.
- Routes for video and audio: Both, Stream only, Recording only and Off. They can be set for every canvas, for one canvas, or for one placement of a source.
- A recording composition built from a private copy of your scenes. It follows nested scenes, groups, transforms, crops, blend modes, scene switches, animated transitions, Studio Mode (including Duplicate Scene) and source deletion.
- Independent recording audio built per source, with OBS's fader, mute, push-to-talk and sync offset, plus a per-source recording volume.
- Recording settings for each canvas: encoder (automatic, NVENC, AMF, QuickSync or x264), constant quality or bitrate, resolution, frame-rate divisor, folder and filename suffix. Shared settings: container, audio encoder, bitrate and filename format.
- A fallback to x264 when a hardware encoder can't start.
- Each canvas handles its own failures, with clear messages. A failure never affects another canvas or a stream.
- Warnings when a source meant to stay off the stream is still visible live or still on the stream's audio track, including tracks shared between two streams.
- Streamlabs-style buttons on each source in OBS's Sources list and Aitum Vertical's Vertical Sources dock, cycling Both, Stream and Recording. They can be turned off.
- A dock with a tab for each canvas. The simple view (the default) shows the current scene with one button per source. The Advanced view has separate video and audio controls, per-canvas and per-placement routes, search, filters and bulk changes. Also: profiles (with import and export), preview, pause and resume, status, and an open-folder link. The dock opens on first run.
- Hotkeys for each canvas, plus start/stop all.
- Crash recovery: interrupted recordings are listed at the next start.
- An obs-websocket vendor (`selective-recording`) for automation.
- A Windows installer that checks the OBS version, won't run while OBS is open, and adds an uninstaller.
- Integration tests that drive a portable OBS and check recordings pixel by pixel and by frequency, plus a soak test.
- Documentation: user guide, troubleshooting, automation API, specification, architecture, configuration, roadmap and decision records.

### Never changed

- Live scenes, streams and other plugins' outputs are never modified ([ADR-0003](docs/adr/0003-stream-exclusion.md)).
