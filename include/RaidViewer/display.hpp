// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#pragma once

// clang-format off
#include <windows.h>
// clang-format on

#include <cstddef>
#include <vector>

// --- Window Geometry & Styles ---
struct window_placement {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    DWORD style = 0;
    DWORD ex_style = 0;
};

// --- Display & Monitor Operations ---
[[nodiscard]] auto init_dpi_awareness() noexcept -> bool;
[[nodiscard]] auto get_all_monitor_rects(std::vector<RECT> &out_rects) -> bool;
[[nodiscard]] auto select_monitor_index(std::size_t monitor_count, int configured_monitor) noexcept -> std::size_t;
[[nodiscard]] auto compute_window_placement(const RECT &mon_rect, bool is_windowed) noexcept -> window_placement;
