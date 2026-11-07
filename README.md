# VillaCast

<img src="data/villacast.png" width="128" alt="VillaCast logo: video distributed to networked screens">

**Turn OBS into a live TV channel for your local network.**

[Español](README.es.md) · [Download installers](https://github.com/efrencd/VillaCast/releases/latest) · [User manual](docs/MANUAL.en.md)

VillaCast is a native OBS Studio plugin that sends your program to web browsers over
WebRTC. Start the server and an independent stream from a single dock, share the
viewer link with classrooms or other devices, and monitor connections from OBS.
MediaMTX handles media delivery; VillaCast brings its setup and everyday controls
into OBS.

Originally developed for the **Villaverde school TV project in Fuerteventura,
Canary Islands**, by **Efren C.D.**

## What it does

- Independent H.264/Opus WebRTC output: continue streaming or recording with OBS's main outputs.
- One Start/Stop button controls server and stream together, with a softly pulsing green live indicator; optional automatic start when OBS opens.
- Install/update MediaMTX with WinGet or select an existing installation.
- Use OBS output resolution and frame rate, or choose custom dimensions; x264 and NVIDIA NVENC support.
- Share an IP link or an optional stable `.local` name on compatible networks.
- Customize browser title, background, fit, audio, autoplay, live mode and reconnection.
- Icon controls that hide after inactivity; double-click fullscreen, even without the control bar.
- Client list with row numbers, available network names and receiver statistics.
- Separate LOG tab, compact dock layout, automatic settings saving and English/Spanish UI.

## Download and install — Windows x64

Current version: **0.8.1**, an early release. Download from
[GitHub Releases](https://github.com/efrencd/VillaCast/releases/latest):

| File | Purpose |
| --- | --- |
| `VillaCast-0.8.1-windows-x64-en-US.msi` | Installer with an English wizard |
| `VillaCast-0.8.1-windows-x64-es-ES.msi` | Installer with a Spanish wizard |
| `VillaCast-0.8.1-windows-x64-portable.zip` | Extracted plugin files and installation script |
| `VillaCast-0.8.1-windows-x64-SHA256SUMS.txt` | SHA-256 download checksums |

Both MSI files contain the same bilingual plugin: **install only one**. Close OBS,
run the installer with administrator rights, reopen OBS and select **Docks → VillaCast**.
For a portable OBS installation, follow the [manual](docs/MANUAL.en.md#installation).
Installers are currently unsigned. MediaMTX is installed separately through the dock.

**Requirements:** Windows x64, OBS Studio 31.1 or later with WHIP/Opus support,
and MediaMTX 1.21 or later. Local testing uses OBS 33.0.1 and MediaMTX 1.21.3.
NVENC requires a compatible NVIDIA GPU and driver. Linux/macOS builds are not yet available.

## First broadcast

1. Open **Docks → VillaCast** and install or select MediaMTX.
2. In **Settings**, choose your encoder and network interface. Automatic resolution follows OBS's video output.
3. Keep an audio source active in OBS audio mix 1, even if silent.
4. Click **Start**: VillaCast starts both the server and the independent stream.
5. Click **Open video** to test, or **Copy link** and open it on other devices.
6. Use **Clients** to inspect receivers and **LOG** to troubleshoot. Click **Stop** to end delivery.

The player port defaults to 8000 for new installations; saved ports are preserved.

The browser may require a click to enable sound or start playback. Settings save
automatically; reload existing viewer pages after changing player options. Closing
OBS stops the server started by VillaCast. Automatic start applies only when OBS
opens; it does not add anything to Windows startup.

## Network and monitoring

Defaults: viewer TCP **8000**, WebRTC signaling TCP **8889**, media UDP/TCP **8189**,
monitoring API **127.0.0.1:9997**. The API uses a random credential per server start.
The optional `.local` address depends on mDNS support; VLAN separation, wireless
client isolation or firewall policy can prevent access. VillaCast does not change
router, firewall or school DNS configuration. This release targets local networks;
Internet/TURN deployment is outside its current setup flow.

Detailed client statistics come from small periodic reports sent by VillaCast's web
player, plus MediaMTX's local control API. External WebRTC players show basic session
information. Reports stay in RAM and expire. Network names use cached asynchronous
system resolution and may be unavailable. RTT measures network round trips, not
camera-to-screen delay. See the [manual](docs/MANUAL.en.md) for every column and control.

## Updating from OBS MediaMTX

Install VillaCast over the old plugin with OBS closed. Existing settings, dock identity,
MSI upgrade identity and internal `obs-mediamtx` paths are retained. Custom browser
titles stay unchanged; the old default title becomes VillaCast. Do not install a
second copy under a different folder. Uninstalling VillaCast preserves personal
settings and the separately installed MediaMTX server.

## Build

Use Visual Studio 2022 C++ Build Tools, Windows SDK, Git and CMake 3.28+:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure
./scripts/Package-Windows.ps1
```

The packaging script restores WiX SDK/UI/Util 6.0.2 with MSBuild and creates MSI files,
a ZIP containing the manuals, and checksums. Releases are built manually; GitHub
Actions workflows are not included. See [validation notes](VALIDATION.md) for tested
behavior and limitations. License: [GPL-2.0-or-later](LICENSE).
