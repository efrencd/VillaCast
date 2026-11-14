# Local validation — VillaCast 0.8.4

- Build and CTest passed (3/3).
- Portable OBS and MediaMTX 1.21.3 published a synthetic tone assigned only to mix 2. Chrome received a nonzero audio signal and zero video tracks at `?audio_track=2`.
- Track 1 returned audio only, disabled track 3 displayed an unavailable message, and the regular link played at 1280 px width with both media tracks.

# Local validation — VillaCast 0.8.2 (2026-11-07)

- All numeric settings use wheel-safe spin boxes; all selectors use wheel-safe combo boxes.
- OBS portable: wheel over Web port kept 18889 and moved the Settings scrollbar.
- Wheel over the encoder selector kept NVIDIA NVENC H.264 and moved the Settings scrollbar to 120.
- Arrow buttons, keyboard entry and opened dropdown selection remain available.
- Local update only; not published to GitHub.

# Local validation — VillaCast 0.8.1 (2026-11-07)

- CMake build and CTest 3/3 passed, including bundled favicon HTTP serving.
- OBS portable: one Start/Stop button; cancellable startup, temporarily disabled while stopping, green opacity animation while live.
- New settings without a saved viewer port display 8000; saved ports are retained. An occupied port is reported without disrupting another instance.
- Real browser test on isolated ports: 1280x720, 45 frames in 1.5 seconds (30 FPS), favicon HTTP 200 / 19,245 bytes.
- Wheel input over Initial volume scrolls the enclosing panel and leaves the value unchanged.
- Generated PNG logo, multiresolution ICO and lightweight favicon included; Windows DLL icon resource compiled.
- Local build only; not published to GitHub.

# VillaCast 0.8.0 — Validation scope

## Environment

Windows x64, OBS Studio 33.0.1, MediaMTX 1.21.3, local browser receivers.
Builds link against the OBS 31.1.1 SDK and Qt 6.11.1. Tests use an isolated OBS
portable profile and separate ports. No Linux/macOS build is provided.

## Verified

- CMake configuration and RelWithDebInfo compilation.
- CTest: mDNS wire behavior, English/Spanish localization, and web server/report handling.
- Web-player test coverage includes escaping embedded configuration, telemetry origin/token checks, numeric validation and report lifecycle.
- Dock registration before OBS restores layout; automatic start after OBS finishes loading.
- Independent H.264/Opus output and stopping/restarting the managed server.
- OBS output resolution following: 1920x1080 canvas with 1280x720 output received at 1280x720.
- Real browser playback at 30 FPS; decoded image sampled in four quadrants.
- NVENC late-join failure reproduced with zero decoded frames; enabling repeat_headers restored playback and recovery after restart.
- x264 playback and concurrent main recording were also checked in earlier local revisions.
- Viewer title/background/fit, autoplay and sound fallback, pause/live behavior and manual/automatic reconnection.
- Real double-click entering/exiting fullscreen with the control bar enabled and disabled.
- SVG buttons, accessible labels and three-second control hiding/reveal behavior.
- Autosave from the native panel to disk and the next viewer response without a Save button.
- Client API pagination, separate browser reports, external player rows and row removal.
- Unauthenticated local monitoring API requests rejected; listener bound to loopback.
- Row numbering/network-name columns, asynchronous resolver results and separate LOG tab inspected in OBS.
- Blue Stop button and scrollable Stream/Settings tabs inspected.
- Clean shutdown of isolated OBS test sessions with zero reported libobs memory leaks.

## Packaging and compatibility

Windows packaging produces English/Spanish MSI files, a portable ZIP, bilingual
manuals and SHA-256 sums. Installers share a release ProductCode across wizard
languages and retain the previous UpgradeCode. The internal DLL/module/config/dock
identifiers remain obs-mediamtx for upgrades; the visible product is VillaCast.

## Limits

- MSI generation and payload checks do not replace a full elevated installer lifecycle test on a clean machine. The repository includes an isolated lifecycle test script; no clean-machine certification is claimed.
- MediaMTX WinGet installation/update has not been tested on a clean PC without MediaMTX already installed.
- Physical classroom devices, school VLANs/mDNS policies and large receiver counts need deployment-specific testing.
- Audio tests used a synthetic silent source; audible speech synchronization was not evaluated.
- Network names depend on the operating system's resolver and can be missing or reflect local DNS/hosts aliases.
- RTT is not end-to-end latency. Detailed statistics need VillaCast's player; external players have basic information only.
- Browser autoplay rules and tab/device suspension still apply.
- No Linux/macOS binaries, Internet/TURN setup, integrated RTSP/HLS output modes or code signing are provided in this release.
