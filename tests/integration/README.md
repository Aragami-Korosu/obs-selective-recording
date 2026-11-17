# Integration tests

End-to-end tests that run the real plugin inside a disposable, portable OBS. They build scenes from solid colours and test tones, record through the plugin over obs-websocket, and then check the recorded files with ffmpeg:

- **Video:** the colour of specific pixels at specific times, and the resolution
- **Audio:** the level of specific frequencies
- **Sync:** offsets measured against OBS's own recording
- **Shutdown:** OBS's leak report after closing

## What's covered

| Area | Checks |
| --- | --- |
| Routing | Every route for video and audio; nested scenes; groups; recording gain; disabled mode |
| Stream protection | Original scene items unchanged on both canvases; live screenshots unchanged |
| Dual output | Aitum Vertical at 1080×1920; both canvases recording at once; canvas recreated after a collection switch |
| Following OBS | Scene switches; mid-fade blending; Studio Mode with Duplicate Scene; deleting a source while recording; scene collection switch while recording |
| Recording | Pause and resume; hotkeys; failure on one canvas while the other keeps recording |
| Quality | A/V offset within one frame of OBS's own recording; no drift; no skipped or dropped frames; audio buffering |
| Lifecycle | Clean exit while recording finalises files; crash recovery; zero memory leaks; no deprecation warnings |

`soak.py` records both canvases for a long time and checks drift, duration accuracy, dropped frames and memory growth.

## Setup (Windows)

1. Copy an OBS Studio 31.1+ installation to a scratch folder and create an empty `portable_mode.txt` next to its `bin` folder.
2. Install Aitum Vertical Canvas 1.6+ into that copy (`obs-plugins\64bit` and `data\obs-plugins\vertical-canvas`).
3. In the copy's `config\obs-studio\plugin_config\obs-websocket\config.json`, enable the server on port 4466 with authentication off.
4. Install Python packages: `pip install websocket-client numpy pillow`.
5. Put `ffmpeg` and `ffprobe` on `PATH`.

## Running

```powershell
.\run.ps1 -Obs C:\scratch\obs-portable -Plugin C:\path\to\obs-selective-recording -WorkDir C:\scratch\sr-tests
```

`-Plugin` is the extracted Windows package from CI (the folder that contains `bin\64bit` and `data`). Each run:
- resets the test OBS's scene collection and plugin settings,
- installs the plugin into the copy,
- starts OBS itself and runs every test,
- closes OBS and checks the log.

For a soak test, run it after a normal run (it reuses the scenes):

```powershell
python soak.py --obs-dir C:\scratch\obs-portable --workdir C:\scratch\sr-tests --minutes 30
```

## Notes

- The tests start, kill and restart the test OBS. They never touch any other OBS.
- If OBS hangs on exit, the runner makes it crash on purpose, so OBS's crash handler writes every thread's stack to `config\obs-studio\crashes`. The path is printed.
