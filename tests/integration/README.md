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
| Live stream (optional) | A real stream stays connected, with no reconnects or dropped frames, while recordings start, stop and fail; warnings read the active stream's tracks |

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

### Live stream test

This test is skipped unless you provide a stream key in an environment variable. Use a **throwaway test channel**: the test scenes (solid colours and test tones) are broadcast live. Before going live, the test checks that the setup contains no microphone, desktop audio, screen, window or browser capture, and refuses to stream if it finds any.

```powershell
$env:SR_TEST_STREAM_KEY = '<test channel key>'               # Twitch by default
# $env:SR_TEST_STREAM_SERVICE = 'YouTube - RTMPS'           # optional: another OBS service name
.\run.ps1 -Obs C:\scratch\obs-portable -Plugin C:\path\to\obs-selective-recording -WorkDir C:\scratch\sr-tests
Remove-Item Env:SR_TEST_STREAM_KEY
```

The key is only passed to OBS and is never printed. When the test finishes, it restores the test OBS's previous stream settings.

### Soak test

For a soak test, run it after a normal run (it reuses the scenes):

```powershell
python soak.py --obs-dir C:\scratch\obs-portable --workdir C:\scratch\sr-tests --minutes 30
```

## Notes

- The tests start, kill and restart the test OBS, always with `--portable`, so it never loads plugins installed in ProgramData. They never touch any other OBS.
- Before starting, the runner closes any other OBS running from the test folder, because two instances would share its settings and websocket port.
- **If the Streamlabs plugin is installed on your main OBS**, its `sl-browser-service` watches for processes named `obs64.exe`. It installs its plugin into the test OBS (`plugins\sl-browser-plugin`, plus `sl-browser*.exe` in `core\obs-browser`) and restarts it without `--portable`, so the tests end up driving a different OBS. To prevent that:
  1. Copy `bin\64bit\obs64.exe` to `bin\64bit\obs64-sr-test.exe` in the test copy. The runner uses that name when it exists, and the service ignores it.
  2. Rename or remove `obs64.exe` in the test copy, so the service has nothing to restart.
  3. Remove or rename anything it already added. Its plugin folder is created with administrator rights, so rename `plugins` if you can't delete it.
- If OBS hangs on exit, the runner makes it crash on purpose, so OBS's crash handler writes every thread's stack to `config\obs-studio\crashes`. The path is printed.
