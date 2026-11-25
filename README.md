<h1 align="center">
  <img src="extras/livearea/pic0.png" alt="Crash Nitro Kart 3D PS Vita Banner" width="700"><br>
  Crash Nitro Kart 3D · PS Vita Port
</h1>

<p align="center">
  <b>Native port of Crash Bandicoot Nitro Kart 3D (Polarbit / Activision) for PlayStation Vita and PlayStation TV.</b>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/Platform-PS%20Vita%20%7C%20PS%20TV-003791.svg?style=flat-square&logo=playstation" alt="Platform" />
  <img src="https://img.shields.io/badge/Origin-iOS-000000.svg?style=flat-square&logo=apple" alt="Origin" />
  <img src="https://img.shields.io/badge/Title%20ID-PSVCNK001-ff69b4.svg?style=flat-square" alt="Title ID" />
  <img src="https://img.shields.io/badge/Engine-Polarbit%20Fuse-brightgreen.svg?style=flat-square" alt="Engine" />
  <img src="https://img.shields.io/badge/Renderer-vitaGL%20%28GLES%201.1%29-orange.svg?style=flat-square" alt="Renderer" />
  <img src="https://img.shields.io/badge/Status-Playable%20%28beta%29-yellowgreen.svg?style=flat-square" alt="Status" />
</p>

<p align="center">
  <a href="#-description">Description</a> •
  <a href="#-features">Features</a> •
  <a href="#-controls">Controls</a> •
  <a href="#-installation">Installation</a> •
  <a href="#-known-issues">Known Issues</a> •
  <a href="#-building-from-source">Build</a> •
  <a href="#-credits">Credits</a>
</p>

---

## 📖 Description

An unofficial, native **iOS-to-PS Vita wrapper port** of *Crash Bandicoot Nitro Kart 3D* (iOS 1.0, originally developed by **Polarbit** on its Fuse engine). There is no `.so` here: the port ships its own **Mach-O loader**, which maps the original 32-bit ARMv6 iOS executable (`iFuse`) into Vita RAM and replaces the Apple frameworks it uses (Foundation/UIKit, OpenGL ES 1.1, AudioQueue) with native implementations built on **vitaGL** and the Vita system libraries. The game engine runs unmodified.

---

## 🎮 Status & Playability

**Status: Playable (beta) (v01.00)**

On physical hardware the game boots, shows the language menu and the front-end menus, and runs races.

- [x] **Mach-O loader**: fixed base `MACHO_SLIDE = 0x98000000`, **2,907** rebase words (2,819 relocated at load), **171** external relocations bound. *Verified on console.*
- [x] **Graphics**: GLES 1.1 → vitaGL (`SOFTFP_ABI=1`, native `GL_FIXED` arrays). The game draws into a 320×480 portrait FBO, which is rotated onto the 960×544 screen. *Verified on console.*
- [x] **Screen modes**: fullscreen stretch (default) or the original 3:2 image with black side bars, switchable in the controls menu.
- [x] **Touch & Controls**: front touch is forwarded as iPhone touches. Buttons inject virtual touches on the HUD. The left stick simulates the iPhone accelerometer for steering.
- [x] **Controls menu**: START+SELECT opens an in-game remapping menu, modeled on the Zenonia 2 port. Settings are saved to `ux0:data/crashnitro1/config.txt`.
- [x] **Options menu extras (new, not yet tested on console)**: bilinear / pixel filter, FPS counter, 30 FPS limit, and **L + R** to hide the game's touch controls during races (they still respond). Hidden pieces are logged as `HUD hidden` for calibration.
- [x] **Audio**: the game's AudioQueue output (22,050 Hz, mono, 16-bit) goes to the Vita audio port.
- [x] **Saves**: progress is written to `ux0:data/crashnitro1/saves/` (the iOS `Documents/` folder).
- [x] **Performance**: CPU 444 MHz / GPU 222 MHz, frame pacing on vsync (60 FPS cap). `game_run` takes about 3–5 ms per frame in races. The log prints `frame N: … fps=` every 300 frames.
- [ ] **Intro video**: `MPMoviePlayerController` is not implemented. The movie is skipped and the game continues.

---

## ✨ Features

| Feature | Status | Notes |
| :--- | :---: | :--- |
| Boots to gameplay | ✅ | Boots, shows the language menu and front-end menus, runs races on hardware |
| Graphics / renderer | ✅ | GLES 1.1 → vitaGL; 320×480 portrait FBO rotated to 960×544 |
| Audio & music | ✅ | AudioQueue output (22,050 Hz, mono, 16-bit) to the Vita audio port |
| Video / cutscenes | ❌ | Intro video skipped (`MPMoviePlayerController` not implemented) |
| Physical buttons | ✅ | Cross, Square, Circle, Triangle and Start (see [Controls](#-controls)) |
| Analog sticks / camera | ✅ | Left stick steers via a simulated accelerometer tilt |
| Touch screen | ✅ | Front touch forwarded as iPhone touches; HUD buttons inject virtual touches |
| Button remapping / port menu | 🧪 | START+SELECT menu and the options extras not yet tested on console |
| Save data | 🧪 | Written to `ux0:data/crashnitro1/saves/`; not marked as verified on console |
| Native trophies | ❌ | Not implemented |
| Languages | 🟡 | Language menu shown on hardware; per-language testing not documented |
| Performance | 🧪 | 60 FPS cap on vsync; about 3–5 ms per frame in races per the log notes |
| Online / IAP / ads | ❔ <!-- TODO: verify --> | Not documented in this repo |
| PS TV | 🧪 | Same target as Vita per the description; not separately verified |

Legend: `✅ Working (verified on console) · 🧪 Implemented, not yet verified on console · 🟡 Partial · ❌ Not working · ➖ Not applicable · ❔ Not documented yet`

---

## 🎯 Controls

| Input | Action | Status |
| :---: | :--- | :---: |
| **Left Stick** | Steer (simulated accelerometer tilt) | ✅ Working |
| **Cross (✖)** | Accelerate | ✅ Working |
| **Square (◼) / Circle (●)** | Brake / Drift | ✅ Working |
| **Triangle (▲)** | Use item | ✅ Working |
| **Start** | Pause | ✅ Working |
| **Start + Select** | PS Vita controls menu (pauses the game) | ⚠️ Not yet verified on console |
| **L + R** | Show / hide the touch controls in races | ⚠️ Not yet tested on console |
| **Touchscreen** | Menus and HUD (iPhone touches) | ✅ Working |

> [!TIP]
> All the buttons above can be remapped in the **Start + Select** menu: Cross replaces a binding, Square adds a button, Triangle unbinds, Circle saves and closes. The same menu sets steering direction (normal / inverted), stick deadzone, screen mode, filter (bilinear / pixel), touch controls (visible / hidden in race), FPS counter and FPS limit (game pace / 30 FPS). You can also edit `ux0:data/crashnitro1/config.txt` by hand (`gas=CROSS,R`).

### Display Modes & Limitations

| Mode | Image | Borders |
| :--- | :--- | :--- |
| **Fullscreen** (default) | The game's 480×320 image stretched to 960×544 (slightly wider than the original aspect) | None |
| **Original 3:2** | 816×544, centered | Black bars of 72 px on each side |

---

## 📋 Prerequisites

To run this port on your PS Vita or PS TV, you will need:

1. A PS Vita / PS TV console running Custom Firmware (**Enso 3.60 / 3.65** recommended) with `kubridge` in `ur0:tai/config.txt`:
   ```ini
   *KERNEL
   ur0:tai/kubridge.skprx
   ```
2. [**libshacccg.suprx**](https://github.com/Rinnegatamante/ShaRKBR33D/releases/latest) installed in `ur0:data/` (extract it with **ShaRKBR33D**).
3. A legally obtained, decrypted copy of *Crash Bandicoot Nitro Kart 3D* **1.0** `.ipa` (FairPlay `cryptid 0`).

---

## 📦 Installation

1. Install `crashnitro1.vpk` on your console using **VitaShell**.
2. Game files: open the decrypted `.ipa` as a ZIP and extract `Payload/iFuse.app/`.
3. Copy to the Vita (`ux0:data/crashnitro1/`):
   ```
   ux0:data/crashnitro1/
     iFuse.app/    the .app folder from the IPA, untouched
     rebase.bin    generated with tools/macho_prep.py (see Build Instructions)
     config.txt    created automatically — controls and screen settings
     saves/        created automatically — your save games
     logs/         created automatically — crashnitro1_001.log … crashnitro1_999.log, one per run
   ```

---

## 🐛 Known Issues

- **Intro video skipped**: there is no movie player yet. The game treats the video as finished.
- **Button positions are fixed**: each button is a virtual touch at a fixed HUD position (480×320 space). If a button has no effect on some screen, the game's button there is at a different place.
- **Opening the menu may also pause the race**: Start is bound to Pause, and Start is usually pressed a moment before Select.
- **Steering feel**: the left stick is translated into an accelerometer tilt (up to about 35°). There is no sensitivity setting other than the deadzone.
- **Crash dumps** are written by the system to `ux0:data/` (not to the game folder). Game logs are in `ux0:data/crashnitro1/logs/`.

---

## 🔨 Building from Source

### Prerequisites

- [VitaSDK](https://vitasdk.org/) (`$VITASDK`). vitaGL is vendored in `vendor/vitaGL` and CMake builds it automatically with `SOFTFP_ABI=1`.
- CMake 3.14+, Python 3 (for the tools).
- A decrypted `iFuse.app` bundle, linked as `game_app` in the repository root (never committed).

### Compilation Commands

```bash
git clone https://github.com/MetalSyntax/Crash-Nitro-Kart-iOS-vita.git
cd Crash-Nitro-Kart-iOS-vita
ln -s /path/to/Payload/iFuse.app game_app

# rebase.bin from the decrypted executable
python3 -I tools/macho_prep.py game_app/iFuse ux0_data/crashnitro1/rebase.bin \
    --report docs/macho_report.txt

# Stage ux0_data/crashnitro1/ (clean .app + rebase.bin)
sh tools/stage_data.sh

cmake -S . -B build
cmake --build build
```

### Deploy & Debug

```bash
# Game data over FTP (VitaShell, port 1337); saves/ is never overwritten
python3 -I tools/deploy_data.py --ip <vita-ip>

# Or with psvita-toolkit
psvita-toolkit deploy --eboot --yes        # eboot.bin only (fast iteration)
psvita-toolkit analyze logs/<dump>.psp2dmp # analyze a crash dump
```

To map a guest address from a dump to the original binary, subtract the guest base `0x98000000` and look it up in `otool -tV game_app/iFuse`. Architecture and progress notes: [`PORTING_PLAN.md`](PORTING_PLAN.md) and [`port_progress.md`](port_progress.md) (in Spanish).

Project rules (see `CLAUDE.md`): **one bug per console run, driven by logs**; nothing from the IPA goes into git.

## 📁 Project Structure

- `source/`: loader and runtime (Mach-O loader, GLES → vitaGL, AudioQueue, input and HUD, controls menu).
- `tools/`: `macho_prep.py` (rebase table generation), `stage_data.sh`, `deploy_data.py` (FTP).
- `vendor/vitaGL/`: vitaGL built from source (softfp).
- `extras/livearea/`: LiveArea assets (`icon0.png`, `bg0.png`, `pic0.png`, `startup.png`, `template.xml`).
- `docs/macho_report.txt`: Mach-O analysis report.
- `game_app`: local symlink to the decrypted `iFuse.app` (not committed).
- `logs/`: console logs from test runs.
- `PORTING_PLAN.md`: living plan (Spanish). `port_progress.md`: bug-by-bug log (Spanish).

---

## 📜 Disclaimer

**Crash Bandicoot** and **Crash Nitro Kart** are registered trademarks of **Activision Publishing, Inc.** The work presented in this repository is unofficial and is not produced, endorsed, or sanctioned by Activision, Polarbit or any affiliated entities.

This software does NOT contain original game code, binaries, copyrighted graphics, or game assets (see `.gitignore`). The authors do not promote or condone piracy in any way. To play, you must provide your own legally obtained, **decrypted** copy of the `.ipa`.

---

## ☕ Support My Work

This project is **free** and will stay free. If you enjoy it, a tip from **$1** helps me keep porting games to the PS Vita:

<p align="center">
  <a href="https://ko-fi.com/metalsyntax"><img src="https://ko-fi.com/img/githubbutton_sm.svg" alt="Support me on Ko-fi"></a>
</p>

- **Ko-fi:** [ko-fi.com/metalsyntax](https://ko-fi.com/metalsyntax)
- **PayPal:** [paypal.me/MetalSyntax](https://paypal.me/MetalSyntax)
- **Bitcoin (BTC):** `133HdCsaWxdjTfmwpgGVykByhNBV338uRc`

**Ko-fi supporters get:**
- **Early access** to new ports before their public release (test builds of upcoming projects).
- **Data preparation scripts and step-by-step guides** to set up your own game files for each new port.
- A say in which games I port next.

> Game files are never distributed. You always need your own legally obtained copy of the game.

---

## 👥 Credits

- **Crash Bandicoot Nitro Kart 3D** is intellectual property of **Activision**, built by **Polarbit** on the Fuse engine.
- **vitaGL** graphics library by [Rinnegatamante](https://github.com/Rinnegatamante/vitaGL).
- **kubridge** plugin by [Bythos](https://github.com/bythos14/kubridge).
- Controls menu design from the [Zenonia 2 PS Vita port](https://github.com/MetalSyntax/Zenonia2-psvita-port). Mach-O loader from the [Skater Nation PS Vita port](https://github.com/MetalSyntax/Skater-Nation-vita).
- 8×8 menu font from the PSP/Vita debugScreen (BSD license).
- Special thanks to the PS Vita homebrew and porting community.

---

## 📄 License

No `LICENSE` file is present in this repository. <!-- TODO: add a LICENSE file and state the loader license. -->
