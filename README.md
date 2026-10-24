# Auralis by DJ Maksy — audio visualizer (VST3 + Windows app)

**English** · [Русский](README.ru.md)

Put it last on the master bus in any DAW: it turns your mix into living visuals and renders them
frame by frame into an MP4 with your audio, up to 4K. A standalone app works without a DAW.

## Download

Latest Windows build: **[Releases → latest](https://github.com/djmaksy/auralis/releases/latest)**

- `Auralis-<version>-Setup.exe` — installer (app + VST3 plug-in)
- `Auralis-<version>-Windows-x64.zip` — `Auralis.exe` and `Auralis.vst3` without an installer

Windows SmartScreen may warn about an unknown publisher because the build is not code-signed:
click **More info → Run anyway**. Close your DAW before installing an update.

## Features

| Feature | Details |
|---|---|
| VST3 plug-in + standalone app | JUCE 8, one codebase for both |
| Sound analysis: 64 spectrum bands, lows/mids/highs, transients, stereo width, host tempo and bar position | `Source/Core/Analyzer.*`: 4096-point FFT, onset detector, smoothing |
| **Layers**: BACKGROUND — liquid lake, nebulae, flight, northern lights, retro grid | GLSL shaders, `Source/Render/Shaders.h` |
| SPECTRUM — polar halo, neon line, hex cells | shaders |
| 3D — particle morph, curl flow, strange attractors (Lorenz / Aizawa / Thomas), spectrum terrain, shattering sphere | up to 43,000 particles, `ParticleSims.*` |
| TEXT — title and lyrics made of particles in 5 scripts (Latin, Cyrillic, Armenian, Georgian, Arabic right-to-left with joined letters) | Noto fonts embedded, word-level layout, `TextLayout.*` |
| Text appearance: particles fly in / fade in / instant | TEXT layer → Appearance |
| ORNAMENT — arevakhach, khachkar, borjgali | procedural, `Ornaments.cpp` |
| OPTICS — bloom, bokeh, chromatic aberration, vignette, exposure | post-processing |
| VIDEO — your own clip or image, full frame or framed, colour grading, grain, scanlines, aberration, slow motion, beat-synced jumps | Media Foundation (Windows) |
| Modulate any slider from lows/mids/highs/hits/loudness/beat and bar LFOs | right-click a slider |
| 62 factory presets + your own | your presets live in `%APPDATA%\Auralis\Presets` |
| Lyrics: paste the text → auto-sync to the vocal (17 languages + auto-detect), or load an .lrc | whisper.cpp + word alignment, `Lyrics.*`, `Recognizer.*` |
| Separate colours for regular and currently-sung words; backing panel follows scene brightness | TEXT layer |
| Export: 16:9, 9:16, 1:1; 720p / 1080p / 1440p / 4K; 24–60 fps; selectable video bitrate (auto or 4–120 Mbps); geometry measured by the short side, so verticals are not cropped | `ExportJob.*` |
| **Export from the DAW without "Create Video"**: just export the audio and the plug-in builds the MP4 itself | detects the host's offline render |
| Frame-accurate, time locked to samples: re-exporting gives identical frames | covered by a test |
| Drag & drop: track, video, image, .txt, .lrc, preset | anywhere on the window |
| Full screen — **D** key (or double-click the picture) | |
| RU / EN interface, window scales up to 2× | |
| Standalone app: frameless window, audio device and ASIO in the gear menu, microphone input | |
| Command-line render | `Auralis.exe --render song.wav --out video.mp4 --preset 5 --res 1080 --aspect 9:16 --lyrics song.lrc --bitrate 16` |

## How to use

1. Insert Auralis **last** on the master bus.
2. Pick a preset with the arrows in the header, switch layers on/off on the right and move the
   sliders (double-click resets, right-click adds modulation).
3. Lyrics: **Lyrics…** → paste the words → **Apply and sync to voice**. The first time, the app
   offers to download the Whisper model (~470 MB, once). Audio for recognition comes from a
   track dropped onto the window, or otherwise from what played through the plug-in. You can
   also load a ready `.lrc`.
4. Aspect, resolution and fps are in the header; bitrate, output folder and file name are in the
   **Export** menu. In a DAW just export the audio — the video appears in `Videos\Auralis`.
   In the app: **Export → Render loaded track to MP4**.

Video is encoded with Windows' built-in Media Foundation (H.264 + AAC) — nothing extra to install.
If `ffmpeg.exe` sits next to the app it is used as a fallback encoder (for example for vertical 4K,
which some GPU encoders refuse).

## Building from source

Requirements: Windows 10/11 x64, Visual Studio 2022 (C++ workload), CMake 3.22+, Git.

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target Auralis_VST3 Auralis_Standalone
```

Output:
- `build\Auralis_artefacts\Release\Standalone\Auralis.exe` — the app
- `build\Auralis_artefacts\Release\VST3\Auralis.vst3` — copy to `C:\Program Files\Common Files\VST3\`

JUCE and whisper.cpp are downloaded automatically on the first CMake configure.

CMake options:
- `-DAURALIS_ASIO_SDK=path\to\asiosdk` — ASIO in the app (free SDK from Steinberg).
- `-DAURALIS_WHISPER_VULKAN=ON` — run lyrics recognition on the GPU (needs the Vulkan SDK).
- `-DAURALIS_WITH_WHISPER=OFF` — build without recognition.

Every push to `main` is built by GitHub Actions (`.github/workflows/build-windows.yml`), which
publishes the installer and zip as a release named after `VERSION`. See [CHANGELOG.md](CHANGELOG.md).

## Project layout

```
Source/Core       sound analysis, parameters, 62 presets, modulation (no dependencies, unit-tested)
Source/Render     OpenGL 3.2: shaders, particles, text, ornaments, optics
Source/Engine     sound → frame pipeline, audio thread → graphics bridge
Source/Lyrics     LRC/TXT, word alignment, recognition (whisper.cpp)
Source/Export     offline render, Media Foundation / ffmpeg
Source/UI         user interface
Source/Standalone standalone app and command-line renderer
tests/            core unit tests
```

## Third-party licences

- JUCE 8 — AGPLv3 or a commercial JUCE licence. Closed-source distribution or sale requires a commercial JUCE licence.
- whisper.cpp — MIT. Whisper models — MIT (OpenAI).
- Noto Sans fonts (Latin/Cyrillic, Armenian, Georgian, Arabic) — SIL Open Font License 1.1.
- ASIO — Steinberg licence (not included in the sources).
