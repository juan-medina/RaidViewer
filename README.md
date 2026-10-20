# RaidViewer

[![License: MIT](https://img.shields.io/badge/License-MIT-478CBF.svg?style=for-the-badge)](https://opensource.org/licenses/MIT)
[![Conduct](https://img.shields.io/badge/Conduct-Covenat%202.0-478CBF.svg?style=for-the-badge)](https://www.contributor-covenant.org/version/2/0/code_of_conduct/)
[![Made with C++20](https://img.shields.io/badge/C%2B%2B-20-478CBF?style=for-the-badge&logo=c%2B%2B&logoColor=white)](https://isocpp.org/)

---

**RaidViewer** is an ultra-minimal second-monitor raid strategy viewer for Windows.

Built in pure C++20 using native Win32 and GDI (`~80 KB` executable, zero external dependencies), it requires **no OpenGL, Direct3D, or Vulkan**. It sleeps at 0% CPU/GPU usage between inputs and uses global hotkeys so you can switch raid diagrams mid-pull without ever alt-tabbing out of your game.

---

## Features

- **Zero Gameplay Impact:** Pure GDI `BitBlt` rendering and an event-driven message loop—no GPU context, no background timers, and no frame-pacing interference.
- **No Alt-Tabbing:** Global hotkeys (`F13`–`F16`, ideal for macro keys, mouse buttons, or a Stream Deck) work while your game stays in focus.
- **Second-Monitor Native:** Automatically opens borderless fullscreen on your secondary monitor (or primary if only one display is connected) with Per-Monitor V2 DPI scaling.
- **Tiny & Portable:** Standalone `~80 KB` executable using only standard Windows system libraries.

---

## Usage

1. Place your raid strategy subfolders inside the `images/` directory next to `RaidViewer.exe` (e.g., `images/M12S - Phase 1/`).
2. Add your slide images (`.png`, `.jpg`, `.jpeg`, `.bmp`) inside each subfolder—files are ordered naturally (`1.png`, `2.png`, `10.png`).
3. Launch `RaidViewer.exe` and control it in either of two ways:
   - **While playing (no focus needed):** Use `F13`–`F16` (bind them to macro keys, mouse buttons, or a Stream Deck).
   - **When RaidViewer is focused:** Use standard keyboard keys (`Arrows`, `Enter`, `Space`, `Esc`).

| Action | Anywhere (No Focus Needed) | When Window Is Focused |
| :--- | :--- | :--- |
| **Previous Strategy / Slide** | `F13` | `Up` / `Left` |
| **Next Strategy / Slide** | `F14` | `Down` / `Right` |
| **Open Strategy** | `F15` | `Enter` / `Space` |
| **Back to List / Close App** | `F16` | `Esc` |

---

## Building

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

Feel free to open issues or pull requests. For code changes, please follow our [Coding Standards](CODING_STANDARDS.md) and keep changes focused.

---

## Contact

Author: Juan Medina (copyright 2026)
