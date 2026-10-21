// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#pragma once

#include "RaidViewer/config.hpp"
#include "RaidViewer/strategy_deck.hpp"

#include <cstdint>
#include <string>
#include <vector>

// --- View Mode ---
enum class view_mode : std::uint8_t {
    folder_list,
    image_viewer,
};

// --- Application Navigation & Runtime State ---
struct app_state {
    view_mode mode = view_mode::folder_list;
    display_mode win_mode = display_mode::fullscreen;
    bool in_size_move = false;
    bool is_resizing = false;
    bool images_dir_exists = true;
    std::vector<strat_folder> folders;
    int selected_folder_index = 0;
    int current_slide_index = 0;
    int scroll_offset = 0;
    int mon_width = 0;
    int mon_height = 0;
    std::wstring images_dir;
    std::vector<global_hotkey_binding> global_hotkeys;
    std::vector<window_key_binding> window_keys;
    std::wstring footer_legend;
    std::wstring close_keys_label;
};

// --- Navigation Operations ---
[[nodiscard]] auto get_visible_row_count(const app_state &app) noexcept -> int;
void ensure_visible(app_state &app) noexcept;
void select_next(app_state &app) noexcept;
void select_previous(app_state &app) noexcept;
[[nodiscard]] auto open_selected_folder(app_state &app) -> bool;
[[nodiscard]] auto handle_close_request(app_state &app) noexcept -> bool;
[[nodiscard]] auto apply_client_resize(HWND hwnd, app_state &app, int new_w, int new_h) -> bool;
