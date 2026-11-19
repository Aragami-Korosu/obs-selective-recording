# Changelog

What's new in each version. The format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and versions follow [Semantic Versioning](https://semver.org/).

## [1.0.0] — first release

### ✨ What you can do

- **Choose where every source goes** with a Streamlabs-style button in OBS's Sources list:
  - <img src="docs/images/both.svg" width="16"> stream and recording
  - <img src="docs/images/stream.svg" width="16"> stream only
  - <img src="docs/images/recording.svg" width="16"> recording only
- **Record with OBS's own Record button.** It makes your selective recording, just like Streamlabs. You can switch this off in Settings.
- **Recording only works in one click.** The plugin hides the source from your stream and keeps it in your recording.
- **Set each placement separately.** The same source in two scenes can be set differently.
- **Dual output:** use separate selective recordings for your horizontal and [Aitum Vertical](https://github.com/Aitum/obs-vertical-canvas) canvases, at the same time. The buttons also appear in the Vertical Sources dock.
- **Sound goes with the source**, as in Streamlabs: a Stream only source's sound stays out of the recording, and a Recording only source's sound is in it. Mic and desktop audio have their own button, and any source can have its own recording volume.
- **A dock** with a simple view of the current scene, and an Advanced view for audio devices, per-canvas settings and **Off**. Recording only works the same there as with the Sources-list buttons.
- **Recording settings per canvas:** encoder (NVENC, AMD, QuickSync or x264), quality or bitrate, resolution, frame rate, folder, and MKV, MP4 or MOV.
- **Pause and resume, hotkeys, and a live preview** of exactly what's being recorded.
- **Warnings** when a source you meant to keep off the stream is still live.
- **Crash recovery:** interrupted MKV recordings stay playable and are listed the next time OBS starts.
- **Profiles** of settings, saved with each scene collection, with import and export.
- **Automation** through obs-websocket, for Stream Deck and scripts.

### 📁 Folders

- Folders (OBS groups) have no button and simply follow their eye icon, like in Streamlabs. Set the sources inside them.
- **Optional:** turn on *folder buttons* in Settings to set every source in a folder with one click.

### 💿 Installer

- A signed Windows installer that checks your OBS version and can also install Aitum Vertical Canvas.
- It won't run while OBS Studio is open. Streamlabs Desktop can stay open.

### 🔒 Safe by design

- Your stream settings, audio tracks and other plugins' outputs are never changed.
- The only change to your live scene is the one you ask for: hiding a source when you choose **Recording only**.
- If a recording fails, the other canvas's recording and your streams keep going.
