// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#pragma once

// clang-format off
#include <windows.h>
// clang-format on

#include <cstdint>
#include <string>
#include <vector>

// --- Configuration Identifiers & Modes ---
enum class action_id : std::uint8_t {
    prev = 0,
    next = 1,
    selection = 2,
    close = 3,
};

enum class display_mode : std::uint8_t {
    fullscreen = 0,
    window = 1,
};

struct key_combo {
    UINT mod = 0;
    UINT vk = 0;
};

struct global_hotkey_binding {
    int id = 0;
    action_id action = action_id::prev;
    key_combo combo{};
};

struct window_key_binding {
    action_id action = action_id::prev;
    key_combo combo{};
};

// --- Configuration Constants ---
constexpr auto k_images_dir_name = L"images";
constexpr auto k_config_file_name = L"RaidViewer.ini";

struct app_config {
    display_mode win_mode = display_mode::fullscreen;
    int monitor_number = 2;
    std::wstring images_folder = k_images_dir_name;
    std::vector<global_hotkey_binding> global_hotkeys;
    std::vector<window_key_binding> window_keys;
    std::wstring footer_legend;
    std::wstring close_keys_label;
};

// --- Configuration Loader ---
[[nodiscard]] auto load_config(const std::wstring &config_path, app_config &out_config) -> bool;
