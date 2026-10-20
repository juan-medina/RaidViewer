// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#include "RaidViewer/app_state.hpp"

#include <algorithm>
#include <vector>

[[nodiscard]] auto get_visible_row_count(const app_state &app) noexcept -> int {
    const int available_h = app.mon_height - 270;
    if(available_h <= 0) {
        return 1;
    }
    return std::max(1, available_h / 72);
}

void ensure_visible(app_state &app) noexcept {
    const int visible_rows = get_visible_row_count(app);
    if(visible_rows <= 0) {
        return;
    }
    if(app.selected_folder_index < app.scroll_offset) {
        app.scroll_offset = app.selected_folder_index;
    } else if(app.selected_folder_index >= app.scroll_offset + visible_rows) {
        app.scroll_offset = app.selected_folder_index - visible_rows + 1;
    }
}

void select_next(app_state &app) noexcept {
    if(app.mode == view_mode::folder_list) {
        if(!app.folders.empty()) {
            app.selected_folder_index = (app.selected_folder_index + 1) % static_cast<int>(app.folders.size());
            ensure_visible(app);
        }
    } else if(app.mode == view_mode::image_viewer) {
        if(!app.folders.empty()) {
            const auto &slides = app.folders[app.selected_folder_index].slides;
            if(!slides.empty()) {
                app.current_slide_index = (app.current_slide_index + 1) % static_cast<int>(slides.size());
            }
        }
    }
}

void select_previous(app_state &app) noexcept {
    if(app.mode == view_mode::folder_list) {
        if(!app.folders.empty()) {
            const auto count = static_cast<int>(app.folders.size());
            app.selected_folder_index = (app.selected_folder_index - 1 + count) % count;
            ensure_visible(app);
        }
    } else if(app.mode == view_mode::image_viewer) {
        if(!app.folders.empty()) {
            const auto &slides = app.folders[app.selected_folder_index].slides;
            if(!slides.empty()) {
                const auto count = static_cast<int>(slides.size());
                app.current_slide_index = (app.current_slide_index - 1 + count) % count;
            }
        }
    }
}

[[nodiscard]] auto open_selected_folder(app_state &app) -> bool {
    if(app.mode == view_mode::folder_list && !app.folders.empty()) {
        auto &folder = app.folders[app.selected_folder_index];
        if(!folder.loaded) {
            if(!load_folder_slides(folder, app.mon_width, app.mon_height)) {
                return false;
            }
        }
        app.current_slide_index = 0;
        app.mode = view_mode::image_viewer;
    }
    return true;
}

[[nodiscard]] auto handle_close_request(app_state &app) noexcept -> bool {
    if(app.mode == view_mode::image_viewer) {
        app.mode = view_mode::folder_list;
        return false;
    }
    return true;
}

[[nodiscard]] auto apply_client_resize(HWND hwnd, app_state &app, int new_w, int new_h) -> bool {
    if(new_w <= 0 || new_h <= 0) {
        return true;
    }
    app.mon_width = new_w;
    app.mon_height = new_h;
    invalidate_loaded_slides(app.folders);
    ensure_visible(app);

    if(app.mode == view_mode::image_viewer && !app.folders.empty()) {
        auto &folder = app.folders[app.selected_folder_index];
        if(!load_folder_slides(folder, app.mon_width, app.mon_height)) {
            return false;
        }
    }

    InvalidateRect(hwnd, nullptr, FALSE);
    return true;
}
