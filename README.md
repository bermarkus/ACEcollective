# ACEcollective

[![C++](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS-lightgrey.svg)](#requirements)
[![Status](https://img.shields.io/badge/status-early%20development-orange.svg)](#known-limitations--roadmap)

**ACEcollective** computes and visualizes **real-time inter-brain coherence** (hyperscanning) across multiple simultaneous [ACEapp](https://github.com/bermarkus/ACEapp_multi) sessions. It receives each participant's EEG as its own [Lab Streaming Layer (LSL)](https://labstreaminglayer.org/) stream, cross-correlates every pair of participants' channels in the frequency domain, and renders the result live in five different views.

It is deliberately narrow in scope: signal quality, raw EEG display, filtering, and acquisition all stay in ACEapp. ACEcollective's only job is the cross-subject layer on top of the LSL streams ACEapp already publishes.

---

## Table of Contents

- [Features](#features)
- [Requirements](#requirements)
- [Building from Source](#building-from-source)
- [Usage](#usage)
- [Architecture](#architecture)
- [Known Limitations & Roadmap](#known-limitations--roadmap)
- [Related Projects](#related-projects)

---

## Features

### Session management
- Discovers `ACEapp-*` EEG streams live on the network and lets you manually pick 2–6 to form a group session
- Auto-drops a participant within a few seconds if their ACEapp instance disconnects (staleness-based, not dependent on the LSL SDK throwing an error)

### Connectivity engine
- **Magnitude-squared coherence** and **WPLI** implemented; PLV and wavelet transform coherence (WTC) are declared in the metric list but not yet implemented (shown disabled in Settings until they land)
- Configurable frequency band — Theta/Alpha/Beta/Gamma presets or an exact custom range
- Configurable analysis window (128 / 256 / 512 / 1024 samples per update)
- **Full Matrix** mode (every channel of A against every channel of B) or **Channel-to-Channel** mode (only matching electrode labels, e.g. Fp1↔Fp1 — requires participants to share a channel montage)
- Each connected participant's channel data is FFT'd once per update cycle and reused across every pair they appear in, rather than being redundantly recomputed per pair

### Visualization — five view modes, switchable live in Settings
| View | What it shows |
|---|---|
| **Pair Detail** | One pair at a time: a bipartite two-column layout (A's channels on the left, B's on the right) plus an at-a-glance overview grid when more than 2 participants are present |
| **Block Matrix** | Every participant's channels on one shared axis, rendered as a single heatmap — all channels, all pairs, at once |
| **Circular** | A Circos-style ring with one arc segment per participant; hovering a channel isolates its connections and reports its strongest link |
| **Small Multiples** | One mini heatmap tile per participant pair, tiled in a grid; click a tile to jump into Pair Detail on that pair |
| **Force-Directed** | Every channel is a node in a live spring simulation — coherent cross-brain pairs pull together, revealing clusters |

### Recording & publishing
- Records each pair's mean coherence to a CSV timeseries
- Can republish the computed coherence as its own outgoing LSL stream (`ACEcollective-Coherence`) so tools like ACErecorder can log it alongside the raw EEG

---

## Requirements

- **CMake 3.16+**
- **Windows:** Visual Studio 2022 (or another MSVC toolchain CMake can find)
- **macOS:** the CMake project targets it (mirroring ACEapp_multi's dual-platform setup), but it has not yet been built/tested on macOS
- **[liblsl](https://github.com/sccn/liblsl/releases)** — required, not optional. ACEcollective's entire purpose depends on receiving LSL streams.
- **[GLFW](https://github.com/glfw/glfw)** source, for the windowing backend

liblsl and GLFW are fetched locally rather than committed to this repo (see [Building from Source](#building-from-source)) — same convention ACEapp_multi uses.

## Building from Source

1. Clone this repo.
2. Download GLFW's source and extract it to `vendor/glfw/` (so `vendor/glfw/CMakeLists.txt` exists).
3. Download a prebuilt [liblsl release](https://github.com/sccn/liblsl/releases) for your platform and extract:
   - the `lib/` contents (e.g. `lsl.lib`/`lsl.dll` on Windows, `liblsl.dylib` on macOS) to `lib/lsl/lib/`
   - the `include/` contents to `lib/lsl/inc/`
4. Build:
   ```bash
   # Windows
   build.bat

   # Any platform, manually
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64   # or your platform's generator
   cmake --build build --config Release
   ```
5. The executable lands at `build/Release/ACEcollective.exe` (Windows) or the equivalent for your generator.

**Use a Release build.** The prebuilt liblsl SDK is Release-mode only; a Debug build of ACEcollective mixes Debug and Release CRT heaps on Windows and will abort on startup.

## Usage

1. Have 2–6 participants each running ACEapp with LSL streaming enabled.
2. Launch ACEcollective. In the **Connect** section, click **Refresh** to discover their streams, then **Add** each one you want in the session.
3. Click **Start Session** once at least 2 participants are added.
4. Adjust the connectivity metric, frequency band, analysis window, and view mode in **Settings** as needed.
5. Optionally enable recording or LSL republishing, also in Settings.

## Architecture

ACEcollective mirrors [ACEapp_multi](https://github.com/bermarkus/ACEapp_multi)'s project structure and look-and-feel deliberately, since it's part of the same application family:

- `include/` / `src/` split by concern (`processing/`, `ui/panels/`, `lsl/`, `utils/`, `storage/`), same as ACEapp
- `ui/theme.h` — shared color/spacing/rounding constants, ported near-verbatim from ACEapp so both apps read as one visual family
- `PanelBase` / `PanelManager` / `ApplicationInterface` — same panel architecture as ACEapp
- Dear ImGui + GLFW + OpenGL3, vendored the same way (ImGui committed under `vendor/imgui/`; GLFW and liblsl fetched locally, not committed)
- `processing/interbrain_connectivity_calculator.*` generalizes ACEapp's own single-subject `ConnectivityCalculator` to cross-participant channel sets

Where ACEcollective's needs are genuinely smaller than ACEapp's (no BrainFlow, no multi-format file recording, no headset config UI), the code is intentionally simpler rather than copying complexity that doesn't apply — e.g. `Application` is one file instead of split across a dozen `application_*.cpp` files.

## Known Limitations & Roadmap

- **PLV and WTC** are not implemented yet (WPLI and magnitude-squared coherence are)
- **No auto-reconnect** — if a participant's ACEapp instance comes back after dropping out, you need to re-Add it
- **CSV only for recording**, not HDF5 — matching [ACEanalysis](https://github.com/bermarkus/ACEanalysis)'s offline HDF5 schema in C++ would mean vendoring a new cross-platform dependency neither app in the family currently has
- **Only pair means are recorded/republished**, not full per-channel-pair matrices
- **Untested on macOS** — the CMake project targets it, but this has only been built and run on Windows so far
- Uses a naive O(N²) DFT (same approach ACEapp's own connectivity code uses) rather than a real FFT library — fine at current window sizes, worth revisiting if profiling ever shows it as a bottleneck with several large-channel-count participants active at once

## Related Projects

Part of the ACE family of tools for EEG hyperscanning research:

- **[ACEapp](https://github.com/bermarkus/ACEapp_multi)** — real-time single-subject EEG acquisition, signal processing, and visualization; publishes the LSL streams ACEcollective consumes
- **[ACEanalysis](https://github.com/bermarkus/ACEanalysis)** — offline (Python/MNE) analysis, including single-subject coherence and spectral parameterization
