# VillaCast 0.8.3 — User manual

For audio only, add `?audio=1` to the viewer link, for example `http://studio:8000/?audio=1`. Tap the sound control if the browser asks for a gesture.

[Español](MANUAL.es.md) · [Project](../README.md) · [Downloads](https://github.com/efrencd/VillaCast/releases/latest)

VillaCast sends the OBS **program** (not the preview in Studio Mode) and audio mix 1
to browsers through an independent WebRTC output. OBS's main streaming and recording
buttons control separate outputs. Currently the stream type selector contains WebRTC
only; RTSP/HLS are not integrated output modes in this release.

## Installation

1. Install a regular Windows x64 OBS Studio 31.1+ with WHIP and Opus support.
2. Close every OBS instance and download one VillaCast MSI from Releases. Choose the
   wizard language; the plugin itself supports both English and Spanish.
3. Run the MSI with administrator permissions. Reopen OBS and enable **Docks → VillaCast**.
4. In VillaCast, install MediaMTX with WinGet, or select an existing MediaMTX 1.21+ executable.
5. Configure your scene and keep an audio source active in audio mix 1, even if silent.
6. Start VillaCast, open the viewer locally, then test the shared link on another device.

For an OBS portable installation, close it, extract the ZIP and copy
`obs-mediamtx/bin/64bit/obs-mediamtx.dll` to `obs-plugins/64bit/obs-mediamtx.dll`, and
the contents of `obs-mediamtx/data` to `data/obs-plugins/obs-mediamtx/` inside the OBS
portable folder. The ZIP's `Install-Plugin.ps1`, run as administrator, installs into
the standard system plugin directory instead. Do not keep duplicate plugin copies.
The internal `obs-mediamtx` name is retained for compatibility with previous releases.

## Saving, language and dock layout

Changes save automatically: checkboxes immediately, typed/numeric/selection values
after about 0.5 seconds of inactivity. Invalid background values show an error;
correct them to `#RRGGBB`. **Reload open viewer pages** to apply player changes.
Network and encoder settings require stopping VillaCast first; disabled controls
become available once stopped. The mouse wheel scrolls the panel without changing numeric fields or closed dropdowns,
even when they have focus. Use arrow buttons/keys, typed values or an opened dropdown
to change settings. There is no independent FPS setting: change
**OBS Settings → Video → FPS** while the OBS outputs are stopped.

The UI uses Spanish when OBS or the primary OS UI language is Spanish; otherwise
English. Detection happens when OBS opens. MediaMTX/OS log messages keep their own
language. Dock VillaCast like other OBS panels; tabs and scroll areas allow shorter
layouts. The log is separate from Stream.

## Stream tab — every control

| Control | Function |
| --- | --- |
| Status above tabs | Shows stopped, starting, connecting, streaming, stopping or an error. |
| Stream type | Selects the independent output protocol. WebRTC is currently the only option. |
| Start automatically when OBS opens | Starts the server and stream with saved settings when OBS opens. Off by default; does not configure Windows startup. |
| Resolution summary | Shows the effective width/height and whether OBS output or custom dimensions are used. |
| Install MediaMTX | Uses WinGet to download/install the separately distributed server. The confirmation explains package/source agreements; progress appears in LOG. |
| Update MediaMTX | Uses WinGet to update the server, not the VillaCast plugin. Stop the stream first. Update VillaCast itself with its MSI. |
| Start / Stop (one blue button) | When stopped, starts the server and independent stream. During startup/live operation, changes to Stop to end both. During stopping it is temporarily disabled. Other OBS outputs and unrelated servers are not stopped. A green dot pulses softly beside it while this output is active. |
| Main link | Read-only viewer address. Uses the advertised `.local` name when available, otherwise an IP. |
| mDNS status | Shows the advertised address/interface, a name conflict, or an mDNS error. |
| Alternative IP link | Read-only address for the selected network interface; useful if `.local` resolution fails. |
| Open video | Opens the main link in the default browser. |
| Copy link | Copies the main link to the clipboard. To use the alternative IP address, select/copy that field. |
| Open configuration | Opens the plugin configuration folder, containing saved settings and generated `mediamtx.yml`. Generated server settings are rewritten by the plugin. |

Closing OBS stops the managed output and server. If another application already uses
a configured port, VillaCast reports the conflict instead of closing that application.

## Settings tab — server and encoding

| Control | Function |
| --- | --- |
| MediaMTX path | Path to the server executable; installed copies are detected when possible. |
| Select mediamtx.exe… | Opens a file chooser for an existing server installation. |
| Stream name | Media path shared by publisher/reader, default `mystream`; 1–64 letters, digits, hyphens or underscores. |
| Web port | MediaMTX HTTP port used for WebRTC WHIP/WHEP signaling, default 8889. Not the custom viewer's port. |
| ICE port (UDP/TCP) | Media transport port, default 8189. Network policy must allow the connection. |
| Player port | VillaCast's custom HTTP web player, default 8000. Share this address with viewers. |
| Allow viewing from the local network | Allows local/private-network readers. When off, links/bindings are restricted to the local PC. Not an Internet publication switch. |
| Advertise a .local name (mDNS) | Advertises an optional name so viewers can avoid typing a changing IP on compatible networks. Requires LAN viewing. |
| Name (without .local) | 1–63 lowercase letters, digits or hyphens; no leading/trailing hyphen. Enter `school-tv` to advertise `school-tv.local`. |
| Network interface for the link | Chooses which interface's address is shared/advertised. Automatic selects an eligible interface. It does not reconfigure the network adapter. |
| Independent encoder | x264 uses CPU; NVIDIA NVENC H.264 uses a compatible NVIDIA GPU. This does not change the main OBS encoder. |
| Video (kbps) | Target encoded video bitrate, default 4000, range 250–50000. Each viewer needs bandwidth; multiple viewers multiply server outbound traffic. |
| Resolution | Automatic follows OBS output resolution. Custom enables width/height and GPU scaling for this output. |
| Width / Height | Custom dimensions; width 160–7680, height 90–4320. Both must be even. Disabled in automatic mode. |

The WebRTC audio codec is Opus at 128 kbps, taken from OBS audio mix 1. These are not
separate controls in this version. The three public ports must differ; the monitoring
port also needs to be free. No firewall/router rules are created automatically. The 8000 default only applies
to installations without a saved player port; existing ports are kept.

## Settings tab — web player

| Control | Function |
| --- | --- |
| Browser title | Text shown in the browser tab/title bar; up to 120 characters. Defaults to VillaCast. |
| Background (#RRGGBB) | Color around the video, for example `#111111`. |
| Image fit: Entire image | Keeps the full picture and aspect ratio; empty borders can appear. |
| Image fit: Fill window (crops) | Fills the available space while retaining aspect ratio; picture edges may be cropped. |
| Sound enabled by default | Requests initial unmuted playback. Browsers can require a user click regardless of this setting. |
| Initial volume | Initial player volume from 0–100%; independent of OBS mix level and device volume. Mouse-wheel input scrolls the panel without changing this value; use the arrows, keyboard or typed value. |
| Autoplay | Requests playback as soon as the stream arrives. If disabled, a central Play button starts it. |
| Show control bar | Shows one complete icon bar with sound, volume and fullscreen. Play/pause is included only when live mode is off. No separate volume-bar setting. |
| Live mode: prevent pause | Hides normal pause controls and attempts to resume pauses. Cannot stop browser/device suspension or force a closed tab to play. |
| Show connection status | Shows connecting/waiting/reconnecting messages over the video. |
| Automatic reconnection | Retries an interrupted connection. When off, a Reconnect button is offered instead. |

The bar hides after 3 seconds of inactivity. Pointer, touch or keyboard activity
reveals it; using its controls keeps it visible. Double-click the video to enter/leave
fullscreen even when the bar is disabled. Required start/sound/reconnect prompts can
still appear with the bar disabled. Their icon buttons have accessible labels/tooltips.

## Settings tab — client monitoring

| Control | Function |
| --- | --- |
| Show clients and collect statistics | Enables MediaMTX session queries and receiver reports from VillaCast pages. Disable while stopped if monitoring is not wanted. |
| Local monitoring port | MediaMTX control API on loopback only, default 9997; authenticated with a new random credential per server start. |
| Update interval | Refresh/report interval, 1–30 seconds; default 2. Existing web players require reload to use a changed interval. |
| Autosave message | Confirms saved changes or explains an invalid background value. It is not a button. |

The OBS-side API queries stay on the server PC. VillaCast browser clients do send
additional small periodic statistics reports over the LAN. This is much smaller than
video traffic. External players do not supply those detailed reports. Reports are
kept in RAM, expire after missed reports and are cleared when the server stops.

## Clients tab — every column

| Column | Meaning |
| --- | --- |
| No. | Current row number starting at 1. Changes as rows appear/disappear; not a persistent device ID. |
| Network name | Name returned by asynchronous system resolution, or — if unavailable. Local readers use this PC's name. Names are cached: 5 minutes for results, 1 minute for failures. |
| Client | Short connection/page identifier. Multiple browsers/tabs on one computer can create separate rows. |
| Address | Receiver address; external-player sessions may include a source port. |
| Connected since | Local date/time of session creation or first page report. |
| Duration | Elapsed time since that timestamp. |
| State | Connecting, connected, playing, waiting/paused, failed, disconnected or closed, according to available data. |
| Resolution | Encoded video dimensions received by the player, not the physical screen size. |
| FPS | Received/decoded video frame rate reported by the browser. |
| Video (kbps) | Recent incoming video bitrate; not total network traffic including audio/overhead. |
| Loss (%) | Recent interval packet-loss estimate, not a lifetime average. |
| Jitter (ms) | Variation in packet arrival timing; lower and steadier is generally better. |
| RTT (ms) | Network round-trip estimate when available; not end-to-end video delay. |
| Dropped frames | Cumulative dropped-video-frame count reported by the current browser connection. |

`Clients: N` is a count of rows, not necessarily N distinct devices or N active
classrooms. Waiting pages may appear. The same device can open several tabs. Name
resolution depends on the network's naming services; no manual aliases are stored.
A dash means the measurement/name is unavailable, not zero. Use horizontal scrolling
to view all columns in a narrow dock. If the API cannot be read, the tab shows the
available player reports and an explanatory message.

## LOG and About tabs

**LOG** contains server/plugin events and installation progress. Date and time use
separate colors to distinguish entries. It is read-only, selectable/copyable and
limited to the most recent 1500 text blocks. Original server messages may be English.

**About** shows VillaCast's compiled version, author Efren C.D., the Villaverde school
TV project origin in Fuerteventura, Canary Islands, and a clickable GitHub project link.

## Troubleshooting and updates

- **No stream available:** start VillaCast and check LOG for a connected publisher.
- **Sound is silent:** use the sound activation button and check OBS mix 1/device volume.
- **Picture edges missing:** select Entire image rather than Fill window; verify OBS's program/output.
- **No video with NVENC:** use VillaCast 0.7.2+ (included here), which repeats H.264 headers; verify the GPU/driver and compare x264 if necessary.
- **Local works, classroom fails:** try the alternative IP address and check network isolation/firewall with the administrator.
- **.local fails or network name is —:** the network may not provide the relevant naming service; use the IP link.
- **Settings seem unchanged:** reload the player; stop/start VillaCast for network/encoding changes.
- **Port busy:** stop a manually launched conflicting server or choose distinct free ports.

Close OBS before upgrading. Install the new MSI over the old version; the UpgradeCode,
internal paths and dock ID remain compatible. Custom browser titles are preserved;
the old default OBS MediaMTX title becomes VillaCast. Removing the plugin preserves
personal settings and the separately installed MediaMTX server. Installers are unsigned.
Linux/macOS, Internet/TURN setup and RTSP/HLS output controls are not available in this release.
