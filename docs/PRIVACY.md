# Privacy Policy

**Effective date: November 24, 2026**

This policy describes how the OBS Selective Recording plugin handles information. It applies to the plugin itself, not to OBS Studio, OBS plugins or services, operating systems, or websites that you use with it; those products have their own privacy policies.

## Information the plugin handles

The plugin does not require an account and does not include analytics, advertising, tracking, or automatic crash-report uploads. It does not send information to Aragami Korosu or an analytics service.

To provide its features, the plugin processes information locally in OBS, which may include:

- Your recording settings, such as output folders, file-name formats, encoder choices, and canvas settings.
- Your routing profiles, including scene and source identifiers and the choices you make for where sources appear.
- Recording state and recovery information. While a recording is active, a local recovery marker can contain its canvas, output file path, and start time.
- The audio and video from OBS sources that you choose to include in a selective recording. OBS writes recordings to the location configured by you.

Settings are stored in OBS configuration and scene-collection files on your device. Recovery markers are stored locally and normally removed when a recording stops cleanly; a marker for an interrupted recording remains until you recover or dismiss it. OBS logs may also contain plugin diagnostics, including recording paths or encoder errors. OBS and your operating system control where those logs are kept.

## OBS WebSocket

If OBS WebSocket is installed and available, the plugin registers a `selective-recording` vendor API. The plugin does not enable or open the OBS WebSocket server. If you have enabled that server, authorized clients that can connect to it may request or receive plugin information such as canvas and scene names, source identifiers, recording state, warnings, and recording file paths. OBS WebSocket's connection, authentication, and network settings determine which clients can access it. The plugin does not independently transmit this information to its publisher.

## Storage, retention, and deletion

The plugin does not maintain a remote copy of the information described above. Settings and recovery markers remain on your device as described in [Configuration](CONFIGURATION.md). Recordings remain in the output folders you choose, and OBS logs follow OBS's log-retention behavior.

You can remove recordings and OBS logs using the normal tools for your device. You can remove the plugin's settings and recovery data from its OBS plugin configuration folder; deleting scene-collection route data requires editing or removing the relevant collection data while OBS is closed. Uninstalling the plugin does not automatically delete your recordings or all OBS configuration.

## Third-party software

The plugin runs inside OBS Studio and can integrate with OBS WebSocket and Aitum Vertical Canvas. Those products are separate software and may handle information according to their own policies. Visiting linked sites, downloading software, or using other OBS plugins is also subject to those providers' practices.

## Changes and contact

If this policy changes, the updated version will be published with the plugin. For privacy questions about this plugin, contact **Aragami Korosu** at [ak@aragamikorosu.live](mailto:ak@aragamikorosu.live).
