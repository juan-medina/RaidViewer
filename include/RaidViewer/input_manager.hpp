// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#pragma once

#include "RaidViewer/app_state.hpp"

// clang-format off
#include <windows.h>
// clang-format on

#include <vector>

// --- Global Hotkey Management ---
[[nodiscard]] auto register_hotkeys(HWND hwnd, const std::vector<global_hotkey_binding> &bindings) -> bool;
void unregister_hotkeys(HWND hwnd, const std::vector<global_hotkey_binding> &bindings) noexcept;

// --- Window & Modifier Matching ---
[[nodiscard]] auto get_active_modifiers() noexcept -> UINT;
[[nodiscard]] auto match_window_key(const app_state &app, WPARAM w_param, action_id &out_action) noexcept -> bool;

// --- Action Routing ---
[[nodiscard]] auto dispatch_action(HWND hwnd, app_state &app, action_id action) -> bool;
[[nodiscard]] auto dispatch_hotkey(HWND hwnd, app_state &app, WPARAM w_param) -> bool;
