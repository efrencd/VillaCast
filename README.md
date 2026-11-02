# OBS MediaMTX

[Español](README.es.md) · [Downloads](https://github.com/efrencd/OBS-MediaMTX/releases)

Current version: **0.6.1 (development preview)**.

Native OBS plugin with an independent WebRTC output and a MediaMTX server control dock.

## Windows installation

Close OBS and run `obs-mediamtx-VERSION-windows-x64-en-US.msi` as administrator.
Use the `-es-ES.msi` download for a Spanish installation wizard. Both installers contain
the same bilingual plugin: install only one. A portable ZIP is also available.
Requires a regular 64-bit OBS installation, OBS 31.1 or later. Current validation uses OBS 33.0.1.

Open **Docks > MediaMTX · WebRTC**. Install MediaMTX from the panel using WinGet,
or select an existing executable (MediaMTX 1.21 or later). Click **Start** to start the
server and independent stream. **Stop** stops both. The main OBS streaming and recording
outputs remain independent. Resolution defaults to OBS's video output; custom GPU scaling
and x264/NVENC H.264 are supported. Audio uses mix 1 and Opus; keep an audio source active.

**Start automatically when OBS opens** is optional and disabled by default. It never
adds OBS or MediaMTX to Windows startup. Closing OBS stops the server started by the plugin.

## Language

The plugin uses Spanish when OBS or the primary system UI language is Spanish, including regional
variants. Otherwise it uses English. Detection runs when OBS opens. This does not change
OBS's own language. The panel, plugin messages and web player follow this choice.
Original MediaMTX logs and operating system messages retain their original language.

## Local network

The panel provides a reconnecting browser player (TCP 8891 by default), WebRTC signaling
(TCP 8889), and media over UDP/TCP 8189. Optional mDNS provides a stable `.local` name on
compatible local networks. Multicast isolation, VLANs or firewall rules may require the
network administrator's help. The plugin does not change firewall, router or DNS settings.
Publication is restricted to the local machine; viewers can be restricted to local/private networks.

## Updates and removal

Close OBS before installing an update. Windows Apps supports uninstalling the MSI;
personal OBS/plugin settings and the separately installed MediaMTX server are preserved.
The current installer is unsigned. MSI lifecycle tests requiring elevation are still pending;
see [VALIDATION.md](VALIDATION.md) for the precise validation scope.

## Build and distribution

Use Visual Studio 2022 C++ Build Tools, Windows SDK, Git and CMake 3.28+:

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64
ctest --test-dir build_x64 -C RelWithDebInfo --output-on-failure
./scripts/Package-Windows.ps1
```

The packaging script restores WiX SDK/UI/Util 6.0.2 from NuGet using MSBuild and generates
English/Spanish MSI packages, a portable ZIP and SHA-256 checksums. Releases are built manually. GitHub Actions workflows are not included. The initial public release is a development preview.
Linux/macOS ports are planned and currently disabled in CI. License: GPL-2.0-or-later.


## Author and origin

Author: **Efren C.D.** Originally developed for the Villaverde school TV project in
**Fuerteventura, Canary Islands**. The About tab shows the current build version and
the project link: https://github.com/efrencd/OBS-MediaMTX.
