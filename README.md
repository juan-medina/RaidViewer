# RaidViewer

[![License: MIT](https://img.shields.io/badge/License-MIT-478CBF.svg?style=for-the-badge)](https://opensource.org/licenses/MIT)
[![Conduct](https://img.shields.io/badge/Conduct-Covenant%202.0-478CBF.svg?style=for-the-badge)](https://www.contributor-covenant.org/version/2/0/code_of_conduct/)
[![Made with C++20](https://img.shields.io/badge/C%2B%2B-20-478CBF?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)
[![CI](https://img.shields.io/github/actions/workflow/status/juan-medina/RaidViewer/ci.yml?branch=main&style=for-the-badge&logo=github&label=CI)](https://github.com/juan-medina/RaidViewer/actions/workflows/ci.yml)

**RaidViewer** is an ultra-lightweight second-monitor strategy viewer for Windows. Display raid diagrams, boss mechanics, and maps on your secondary display and flip slides mid-combat without alt-tabbing or dropping frames.

---

## Quick Start

1. **Download:** Get the latest release from [Releases](https://github.com/juan-medina/RaidViewer/releases) and extract the zip anywhere.
2. **Add diagrams:** Put strategy folders and images (`.png`, `.jpg`, `.jpeg`, `.bmp`) inside `images/`:
   ```text
   images/
   ├── Boss Phase 1/
   │   ├── 1-spread.png
   │   └── 2-stack.png
   └── Boss Phase 2/
       ├── 1-tether.png
       └── 2-enrage.png
   ```
3. **Launch:** Run `RaidViewer.exe`. It automatically opens borderless fullscreen on your second monitor.

---

## Controls

Switch slides during combat without taking focus away from your game:

| Action | In-Game (Global Hotkeys) | When Window Focused |
| :--- | :--- | :--- |
| **Previous Slide / Strat** | `F13` | `Up` / `Left` |
| **Next Slide / Strat** | `F14` | `Down` / `Right` |
| **Open Selected Strat** | `F15` | `Enter` / `Space` |
| **Back to List / Exit** | `F16` or `Control+Alt+Q` | `Esc` |

> **Tip:** Bind `F13`–`F16` to mouse buttons, keyboard macro keys, or Stream Deck buttons for quick in-combat paging.

---

## Configuration (`RaidViewer.ini`)

`RaidViewer.ini` is generated automatically next to `RaidViewer.exe` on first launch. Edit it with Notepad to customize your setup:

```ini
[Display]
Monitor = 2             ; 1 = primary, 2 = second monitor
Mode    = Fullscreen    ; Fullscreen or Window

[Images]
Folder  = images        ; Relative path or full path (e.g. D:\RaidStrats)

[GlobalHotkeys]
Previous  = F13
Next      = F14
Selection = F15
Close     = F16, Control+Alt+Q

[WindowKeys]
Previous  = Up, Left
Next      = Down, Right
Selection = Enter, Space
Close     = Esc
```

- **Modifiers:** `Control` (or `Ctrl`), `Alt`, `Shift`, `Win` (e.g. `Control+Alt+Q`).
- **Keys:** `F1`–`F24`, letters, numbers, arrow keys, `NumPad0`–`NumPad9`, `Space`, `Enter`, `Esc`, etc.
- **Multiple bindings:** Separate combinations with commas (e.g. `F16, Control+Alt+Q`).

---

## Design Philosophy: Zero Game Impact

Most overlay tools and viewer apps run on Chromium/Electron, DirectX, or game hooks that compete for GPU time, cause micro-stutters, or trigger anti-cheat warnings. RaidViewer was engineered specifically to avoid this:

- **0% Idle CPU & GPU:** Sleeps on the OS event queue (`GetMessageW`). When you are not pressing a hotkey, it consumes zero processor cycles.
- **No 3D GPU APIs:** Uses pure Win32 GDI (`BitBlt`) and GDI+ cold-path decoding. It never creates a Direct3D, OpenGL, or Vulkan context, never touches VRAM scheduling, and never hooks game render pipelines.
- **Pre-Baked Bitmaps:** When opening a strategy folder, images decode and scale once to your monitor's exact physical pixel dimensions. Page flips are instantaneous 1:1 memory blits.
- **Tiny & Self-Contained:** Built entirely with native Windows APIs (`user32`, `gdi32`, `gdiplus`, `shlwapi`) with C++ exceptions and RTTI disabled. Under 100 KB with zero external runtimes or DLL installers.
- **Per-Monitor V2 DPI:** Renders crisp diagrams at native 1:1 display resolution across mixed-DPI multi-monitor setups.

---

## Building from Source

Requires a C++20 compiler (MSVC or Clang), CMake 3.25+, and Ninja:

```powershell
cmake --preset default
cmake --build --preset release
```

---

## License

This project is licensed under the MIT License — see [`LICENSE`](LICENSE) for details.

---

## Contributing

Feel free to open issues or pull requests. For code changes, please check our [`CODING_STANDARDS.md`](CODING_STANDARDS.md).

---

## Contact

Author: Juan Medina (copyright 2026)

