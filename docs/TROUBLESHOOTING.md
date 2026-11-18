# Troubleshooting

First, check the OBS log (**Help → Log Files → View Current Log**) for lines starting with `[obs-selective-recording]`.

## The dock is missing

Open **Docks → Selective Recording**. If it isn't listed, the plugin didn't load. Look in the log for:

```
[obs-selective-recording] loaded (version 1.0.0)
```

If that line is missing, check that:
- The files are in `%ProgramData%\obs-studio\plugins\obs-selective-recording\` (installer) or in OBS's own `obs-plugins` folder (portable OBS).
- Your OBS is 31.1 or newer.
- The plugin isn't turned off in **Tools → Plugin Manager** (OBS 32 and newer).

## A source I set to "Recording only" or "Off" still shows on stream

- **Set with a Sources-list button (or the dock's simple view):** Recording only hides that placement in OBS for you. If it's back on stream, its eye icon was turned on again. Click the button through to Recording only again, or turn the eye off.
- **Set in the dock's Advanced view:** routes there never change OBS ([ADR-0003](adr/0003-stream-exclusion.md)). Hide the source in OBS (the eye icon), and for audio, untick the stream's track in **Edit → Advanced Audio Properties**. The ⚠ in the dock tells you exactly what's still live.

## The Aitum Vertical tab doesn't appear

- Vertical Canvas 1.6.0 or newer is required. Older versions don't use OBS canvases.
- The log should show `canvas 'Aitum Vertical' (1080x1920) available`. The plugin checks for new canvases every two seconds, so the tab appears shortly after Vertical's canvas does.

## "The video encoder stopped working" or a hardware encoder fails

Graphics cards limit how many hardware encodes can run at once, and every stream and recording counts. With dual streaming plus two recordings you can reach that limit. Either:
- Leave **Settings → If a hardware encoder can't start, use x264 instead** on (the default), or
- Choose x264 or a different encoder for one canvas.

## Recording audio is missing a source

- The source's **Audio** route must be Both or Rec only.
- The source must be on air: in the program scene (live or in the recording) or one of the global audio devices.
- On OBS 31–32, sources set to **Monitor Only** are left out unless you set their route explicitly.
- Check the OBS volume fader. The recording follows it.

## Audio and video are out of sync

Recording audio uses the same timing rules as OBS, including each source's **Sync Offset** in Advanced Audio Properties. If OBS's own recording is in sync and this one isn't, please report it with your log.

## "Couldn't create the recording folder" / "There isn't enough free disk space"

Choose a folder that exists on a drive with space in **Settings → (canvas) → Recording folder**. The plugin won't start a recording with less than 200 MB free.

## The log says "Attempted to add Scene without specifying a canvas!"

On OBS 33 this appears once for each scene when a selective recording or preview starts. It's harmless: OBS logs it whenever a plugin creates a private scene ([ADR-0006](adr/0006-private-canvas-rendering.md)).

## After a crash

The dock lists interrupted recordings at the next start. MKV files play as they are. Use **File → Remux Recordings** to convert them to MP4.

## Reporting a bug

Open an issue with your OBS version, the plugin version, your Aitum Vertical version (if any) and a log uploaded through **Help → Log Files → Upload Current Log File**.
