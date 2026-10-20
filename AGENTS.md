# AGENTS.md

Guidance for AI coding agents working in **RaidViewer**.

**Start here:** Read [`CODING_STANDARDS.md`](CODING_STANDARDS.md) before modifying any file. All C++20 style, naming, formatting, static analysis, and Git commit rules live there and are mandatory.

---

## Product Thesis

**RaidViewer** is an ultra-minimal second-monitor raid strategy viewer for Windows. Players use it to view raid diagrams on a secondary display and flip through slides while actively playing a game on their primary monitor.

Every design choice serves one constraint: **zero impact on the running game.**

- **No GPU APIs (No OpenGL, Direct3D, or Vulkan):** Rendering uses pure Win32 GDI (`BitBlt`) and GDI+ (cold-path image decoding only). It never creates a 3D device context, never competes for VRAM or GPU scheduling, and never triggers driver overlays or frame-pacing hooks.
- **Zero External Dependencies (~60 KB binary):** Built exclusively against libraries shipped natively with Windows (`user32`, `gdi32`, `gdiplus`, `shlwapi`), with RTTI and C++ exceptions disabled and native Win32 file APIs (`FindFirstFileW` / `FindNextFileW`) instead of `<filesystem>`. Never add third-party libraries, package managers, or UI frameworks.
- **No Alt-Tabbing (Global Hotkeys):** Controlled via `RegisterHotKey` (`F13`–`F16`, typically mapped to macro keys, mouse buttons, or a Stream Deck) so the player can navigate folders and slides without taking focus away from the game.

---

## Architectural Invariants (Do Not Revert)

1. **Event-Driven 0% Idle Loop (`GetMessageW`, never `PeekMessageW`):**
   The process blocks on `GetMessageW` and consumes zero CPU cycles when no input or paint event is occurring. Never introduce polling loops, busy waits, or continuous frame timers.
2. **Pre-Baked Screen Bitmaps on Cold Path (`bake_slide_to_screen_bitmap`):**
   When a strategy folder is opened, GDI+ decodes each image and bicubic-scales it once into a screen-sized, black-letterboxed `HBITMAP`. `WM_PAINT` only performs a `BitBlt(..., SRCCOPY)` from the pre-baked bitmap (or double-buffered folder list) and `WM_ERASEBKGND` returns `1`. Never decode, scale, or allocate images inside `WM_PAINT`.
3. **Per-Monitor V2 DPI & Monitor Selection:**
   `SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)` runs before any window or monitor query so bitmaps are baked at exact 1:1 physical pixels. The window targets monitor index `1` (secondary) when available and falls back to `0` (primary), using `WS_POPUP | WS_VISIBLE` and `WS_EX_TOPMOST | WS_EX_TOOLWINDOW`.
4. **Natural Sort Order (`StrCmpIW`):**
   Strategy folders and slide images are sorted with `StrCmpIW` (`shlwapi.h`) with a native filename tie-breaker so numbered phases and slides (`Phase 2` vs. `Phase 10`) sort in human-expected order.
5. **Lazy Slide Baking:**
   Startup only scans folder names (`discover_folders`). Slide bitmaps for a folder are loaded on demand when the folder is first opened (`open_selected_folder`), keeping launch instant and memory usage proportional to viewed folders.

---

## Build & Verification Commands (PowerShell)

The user operates on Windows with PowerShell as the default shell.

```powershell
# Configure (Ninja Multi-Config -> build/compile_commands.json)
cmake --preset default

# Build both configurations (warnings are treated as errors)
cmake --build --preset debug
cmake --build --preset release

# Format and lint touched files
clang-format -i src/main.cpp
clang-tidy -p build src/main.cpp
```

---

## Agent Behavioral Constraints

1. **Zero Tolerance for Unverified Code:** Never declare a coding task complete without running `clang-format`, `clang-tidy`, and building both `debug` and `release` presets with zero warnings.
2. **Do Not Modify Code When Asked to Investigate:** If asked to explain, debug, or analyze behavior without a request to fix it, use read-only tools only and stop to report findings.
3. **Strict Scope & Binary Footprint Discipline:** Do not propose unsolicited features, config files, animations, or abstractions. Any change that significantly increases binary size, memory footprint, or background CPU/GPU activity violates the product thesis.
