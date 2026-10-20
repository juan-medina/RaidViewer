# Coding Standards & Philosophy

This document defines the coding standards, C++20 conventions, and engineering philosophy for **RaidViewer**, aligned with our broader C++ and multi-language projects.

---

## 1. Guiding Philosophy

- **Understand Before Changing:** Read the surrounding code and architecture before modifying a file.
- **YAGNI (You Aren't Gonna Need It):** Implement only what is required for the current scope. Do not introduce speculative abstractions, premature interfaces, or unused configuration layers.
- **Self-Documenting Code:** Prioritize clear, expressive identifiers over comments. Comments should explain *why* (Win32/GDI+ constraints, invariants, non-obvious math), never *what* the code is doing or changelog history (Git commits hold that).
- **Minimal, Focused Diffs & The Boy Scout Rule:** Keep changes focused on a single feature or bug fix. Do not refactor unrelated files, but every file you touch must be left 100% clean under `clang-format` and `clang-tidy`.
- **Git Commit Messages:**
  - Single line only (no multi-line body or bullet points).
  - Start with a lowercase letter, maximum 150 characters.
  - Do **not** use Conventional Commits prefixes (`feat:`, `fix:`, `chore:`).

---

## 2. File Headers & Structure

- **SPDX License Header:** Every source file (`.cpp`, `.hpp`, `.h`) and build/config file (`CMakeLists.txt`, `.editorconfig`) must begin with the SPDX copyright header:
  ```cpp
  // SPDX-FileCopyrightText: 2026 Juan Medina
  // SPDX-License-Identifier: MIT
  ```
- **Header Guards:** Always use `#pragma once` in header files.
- **Section Dividers:** Group related declarations or logical phases using concise section comments (e.g., `// --- Hotkey Configuration ---` or `// ====...` bars).
- **Internal Linkage:** In `.cpp` files, place all file-local types, constants, and helper functions inside an anonymous `namespace { ... }` rather than marking individual items `static`.

---

## 3. Build System & Compiler Strictness

- **Language Standard:** Strict **C++20** (`CMAKE_CXX_STANDARD 20`, `CMAKE_CXX_STANDARD_REQUIRED ON`, `CXX_EXTENSIONS OFF`).
- **Zero-Warning Policy:** Warnings are treated as errors in all configurations:
  - **MSVC:** `/W4 /WX /permissive- /utf-8`
  - **GCC / Clang:** `-Wall -Wextra -pedantic -Werror`
- **Compile Commands:** `CMAKE_EXPORT_COMPILE_COMMANDS ON` is enabled by default via `CMakePresets.json` (`Ninja Multi-Config`) so `clangd` and `clang-tidy` always have an up-to-date `build/compile_commands.json`.
- **Win32 Configuration:** Define `UNICODE`, `_UNICODE`, `WIN32_LEAN_AND_MEAN`, and `NOMINMAX` on targets, and use standard `auto main() -> int` with `/ENTRY:mainCRTStartup`.

---

## 4. Naming Conventions

Naming is strictly enforced by [`.clang-tidy`](.clang-tidy) (`readability-identifier-naming`):

| Construct | Convention | Example |
| :--- | :--- | :--- |
| **Classes / Structs / Unions** | `lower_case` (`snake_case`) | `struct app_state`, `class gdiplus_session` |
| **Functions & Methods** | `lower_case` (`snake_case`) | `bake_slide_to_screen_bitmap()`, `run_app()` |
| **Variables & Parameters** | `lower_case` (`snake_case`) | `selected_folder_index`, `file_path` |
| **Private Member Variables** | `lower_case_` (trailing `_`) | `ULONG_PTR token_ = 0;` |
| **Public Struct Members** | `lower_case` (no suffix) | `app.mon_width`, `folder.slides` |
| **Constants (`constexpr`)** | `lower_case` (`k_` prefix at file scope) | `k_hotkeys`, `k_window_class_name` |
| **Enums & Enumerators** | `lower_case` (`snake_case`) | `enum class view_mode : std::uint8_t { folder_list }` |
| **Namespaces & Type Aliases** | `lower_case` (`snake_case`) | `using unique_bitmap = ...;`, `namespace fs = ...;` |
| **Template Parameters** | `Camel_Snake_Case` | `template<typename Value_Type>` |
| **Preprocessor Macros** | `UPPER_CASE` | `WIN32_LEAN_AND_MEAN` (avoid custom macros) |

---

## 5. Modern C++20 Idioms & Type Safety

- **Trailing Return Types:** Use trailing return types (`auto func(...) -> ReturnType`) for functions that return a value:
  ```cpp
  [[nodiscard]] auto get_visible_row_count(const app_state &app) noexcept -> int;
  ```
  Functions returning nothing may use `void func(...)` or `auto func(...) -> void`.
- **Attributes:**
  - Mark getters, queries, and pure functions `[[nodiscard]]`.
  - Mark functions that cannot throw `noexcept` (especially state updates, destructors, and `wnd_proc`).
  - Use `[[likely]]` and `[[unlikely]]` on performance-critical branches when appropriate.
- **Scoped & Sized Enums:** Always use `enum class` with an explicit underlying integer type (`std::uint8_t`, etc.) when representing IDs, flags, or compact state.
- **Designated Initializers:** Use C++20 designated initializers when constructing aggregates and structs:
  ```cpp
  app.folders.push_back(strat_folder{
      .name = entry.path().filename().wstring(),
      .path = entry.path(),
      .slides = {},
      .loaded = false,
  });
  ```
- **Type Casts & Pointer Qualification:**
  - Never use C-style casts.
  - Use `static_cast<T>` for arithmetic and explicit value conversions.
  - Use `std::bit_cast<T>` (instead of `reinterpret_cast`) when converting between Win32 `LPARAM`/`LONG_PTR` integers and pointers.
  - Always qualify `auto` with `*` or `&` (`auto *hdc = ...;`, `const auto &entry : dir_it`).
- **RAII & Resource Management:**
  - Never leak raw OS, GDI, or GDI+ handles.
  - Wrap lifetime-managed handles in RAII classes (`gdiplus_session`) or custom-deleter smart pointers (`unique_bitmap` via `std::unique_ptr` with a C++20 stateless lambda deleter).
  - Explicitly `= delete` copy and move constructors/assignment operators on non-copyable RAII resource guards.
- **Standard Algorithms & Ranges:** Prefer `<algorithm>` ranges (`std::ranges::sort`, `std::ranges::any_of`), `std::string_view` / `std::wstring_view`, `std::array` for fixed-size buffers, and `<utility>` safe integer comparisons (`std::cmp_greater_equal`).

---

## 6. Formatting & Include Rules

Formatting is enforced by [`.clang-format`](.clang-format) (LLVM-based, C++20):

- **Line Length:** Maximum **120 columns**.
- **Indentation:** Width of 4 columns (`IndentWidth: 4`, `TabWidth: 4`).
- **Control Statements & Braces:**
  - **No space before parentheses:** `if(cond)`, `for(const auto &item: list)`, `switch(msg)`, `catch(const std::exception &ex)`.
  - Always use braces `{ ... }` for control blocks.
  - Pointer and reference qualifiers bind to the right with a space before the identifier (`const fs::path &path`, `const char *message`).
- **Include Directives:**
  - Include every header you use directly (IWYU mindset).
  - Use quotes (`"RaidViewer/..."`) for project headers and angle brackets (`<...>`) for standard library and system headers.
  - Wrap order-sensitive Win32/GDI+ headers (`<windows.h>`, `<objidl.h>`, `<gdiplus.h>`, `<shlwapi.h>`) in `// clang-format off` / `// clang-format on` so `clang-format` does not reorder them.

---

## 7. Static Analysis (`.clang-tidy`)

[`.clang-tidy`](.clang-tidy) enforces checks across:
- `cppcoreguidelines-*`
- `modernize-*`
- `google-*`
- `bugprone-*`
- `misc-*`
- `performance-*`
- `readability-*`

All `clang-tidy` diagnostics must be resolved before committing. Avoid `NOLINT` suppressions unless required by an external Win32/C API contract.

---

## 8. Error Handling & Performance

- **No Silent Failures:** Never ignore errors or silently skip broken states (`if(ptr == nullptr) return;` when `ptr` is required).
- **Boundary Validation & Clean Unwinding:**
  - Use `std::error_code` overloads for `std::filesystem` operations and check errors immediately with descriptive context.
  - Release any temporary GDI objects (`SelectObject`, `DeleteObject`, `DeleteDC`, `ReleaseDC`, `EndPaint`) before throwing or returning on error.
  - Catch fatal exceptions at top-level entry points (`main` and `wnd_proc`), surface a clear error dialog (`show_error_dialog`), and terminate with `EXIT_FAILURE`.
- **Cold-Path Baking vs. Hot-Path Rendering:**
  - Keep `WM_PAINT` (`handle_paint`) fast and allocation-free where possible: decode and scale images into screen-sized `HBITMAP` slides once when a folder is opened, then render via `BitBlt` double-buffering (`WM_ERASEBKGND` returning `1`).

---

## 9. Verification Workflow

Before considering any code change complete:

1. Format all modified files with `clang-format`:
   ```powershell
   clang-format -i src/main.cpp
   ```
2. Configure and build both `Debug` and `Release` presets with zero warnings:
   ```powershell
   cmake --preset default
   cmake --build --preset debug
   cmake --build --preset release
   ```
3. Run `clang-tidy` (or verify zero diagnostics via `clangd` in VS Code) against `build/compile_commands.json`.
