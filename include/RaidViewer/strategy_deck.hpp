// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#pragma once

#include "RaidViewer/platform.hpp"

#include <string>
#include <string_view>
#include <vector>

// --- Strategy Folder & Slides Model ---
struct strat_folder {
    std::wstring name;
    std::wstring path;
    std::vector<unique_bitmap> slides;
    bool loaded = false;
};

// --- Strategy Deck Management & Slide Baking ---
[[nodiscard]] auto is_image_file(std::wstring_view filename) -> bool;
[[nodiscard]] auto bake_slide_to_screen_bitmap(const std::wstring &file_path, int screen_w, int screen_h)
    -> unique_bitmap;
[[nodiscard]] auto load_folder_slides(strat_folder &folder, int screen_w, int screen_h) -> bool;
[[nodiscard]] auto discover_folders(std::wstring_view images_dir, std::vector<strat_folder> &out_folders) -> bool;
void invalidate_loaded_slides(std::vector<strat_folder> &folders) noexcept;
