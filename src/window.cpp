// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#include "RaidViewer/window.hpp"

#include "RaidViewer/app_state.hpp"
#include "RaidViewer/config.hpp"
#include "RaidViewer/display.hpp"
#include "RaidViewer/input_manager.hpp"
#include "RaidViewer/platform.hpp"
#include "RaidViewer/renderer.hpp"
#include "RaidViewer/strategy_deck.hpp"

// clang-format off
#include <windows.h>
// clang-format on

#include <bit>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

[[nodiscard]] auto get_app_state(HWND hwnd) noexcept -> app_state * {
    auto *app = std::bit_cast<app_state *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if(app == nullptr) {
        show_error_dialog(L"Window userdata (app_state*) is null.");
    }
    return app;
}

[[nodiscard]] auto handle_nc_create(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) noexcept -> LRESULT {
    auto *create_struct = std::bit_cast<CREATESTRUCTW *>(l_param);
    if(create_struct == nullptr || create_struct->lpCreateParams == nullptr) {
        show_error_dialog(L"WM_NCCREATE received null create params.");
        PostQuitMessage(EXIT_FAILURE);
        return FALSE;
    }
    const auto user_data = std::bit_cast<LONG_PTR>(create_struct->lpCreateParams);
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, user_data);
    return DefWindowProcW(hwnd, msg, w_param, l_param);
}

void handle_wm_hotkey(HWND hwnd, WPARAM w_param) noexcept {
    auto *app = get_app_state(hwnd);
    if(app == nullptr || !dispatch_hotkey(hwnd, *app, w_param)) {
        PostQuitMessage(EXIT_FAILURE);
    }
}

[[nodiscard]] auto handle_wm_keydown(HWND hwnd, WPARAM w_param) noexcept -> bool {
    auto *app = get_app_state(hwnd);
    if(app == nullptr) {
        PostQuitMessage(EXIT_FAILURE);
        return true;
    }
    action_id action = action_id::prev;
    if(!match_window_key(*app, w_param, action)) {
        return false;
    }
    if(!dispatch_action(hwnd, *app, action)) {
        PostQuitMessage(EXIT_FAILURE);
    }
    return true;
}

void handle_wm_getminmaxinfo(LPARAM l_param) noexcept {
    auto *mmi = std::bit_cast<MINMAXINFO *>(l_param);
    if(mmi != nullptr) {
        mmi->ptMinTrackSize.x = 480;
        mmi->ptMinTrackSize.y = 320;
    }
}

void handle_wm_entersizemove(HWND hwnd) noexcept {
    auto *app = get_app_state(hwnd);
    if(app == nullptr) {
        PostQuitMessage(EXIT_FAILURE);
        return;
    }
    app->in_size_move = true;
}

void handle_wm_size(HWND hwnd, WPARAM w_param, LPARAM l_param) noexcept {
    if(w_param == SIZE_MINIMIZED) {
        return;
    }
    auto *app = get_app_state(hwnd);
    if(app == nullptr) {
        PostQuitMessage(EXIT_FAILURE);
        return;
    }
    const int new_w = static_cast<int>(LOWORD(l_param));
    const int new_h = static_cast<int>(HIWORD(l_param));
    if(new_w <= 0 || new_h <= 0) {
        return;
    }
    if(app->in_size_move) {
        if(new_w != app->mon_width || new_h != app->mon_height || app->is_resizing) {
            app->is_resizing = true;
            app->mon_width = new_w;
            app->mon_height = new_h;
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return;
    }
    if(new_w != app->mon_width || new_h != app->mon_height) {
        if(!apply_client_resize(hwnd, *app, new_w, new_h)) {
            PostQuitMessage(EXIT_FAILURE);
        }
    }
}

void handle_wm_exitsizemove(HWND hwnd) noexcept {
    auto *app = get_app_state(hwnd);
    if(app == nullptr) {
        PostQuitMessage(EXIT_FAILURE);
        return;
    }
    app->in_size_move = false;
    if(app->is_resizing) {
        app->is_resizing = false;
        if(!apply_client_resize(hwnd, *app, app->mon_width, app->mon_height)) {
            PostQuitMessage(EXIT_FAILURE);
        }
    }
}

void handle_wm_paint(HWND hwnd) noexcept {
    auto *app = get_app_state(hwnd);
    if(app == nullptr || !handle_paint(hwnd, *app)) {
        PostQuitMessage(EXIT_FAILURE);
    }
}

void handle_wm_destroy(HWND hwnd) noexcept {
    SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
    PostQuitMessage(0);
}

auto CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) noexcept -> LRESULT {
    if(msg == WM_NCCREATE) {
        return handle_nc_create(hwnd, msg, w_param, l_param);
    }

    switch(msg) {
    case WM_HOTKEY:
        handle_wm_hotkey(hwnd, w_param);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if(handle_wm_keydown(hwnd, w_param)) {
            return 0;
        }
        break;

    case WM_GETMINMAXINFO:
        handle_wm_getminmaxinfo(l_param);
        return 0;

    case WM_ENTERSIZEMOVE:
        handle_wm_entersizemove(hwnd);
        return 0;

    case WM_SIZE:
        handle_wm_size(hwnd, w_param, l_param);
        return 0;

    case WM_EXITSIZEMOVE:
        handle_wm_exitsizemove(hwnd);
        return 0;

    case WM_ERASEBKGND:
        return 1; // Double-buffered / BitBlt prevents flicker

    case WM_PAINT:
        handle_wm_paint(hwnd);
        return 0;

    case WM_DESTROY:
        handle_wm_destroy(hwnd);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, w_param, l_param);
}

[[nodiscard]] auto run_message_loop(HWND hwnd, const std::vector<global_hotkey_binding> &bindings) -> int {
    MSG msg{};
    while(true) {
        const BOOL get_result = GetMessageW(&msg, nullptr, 0, 0);
        if(get_result < 0) {
            const DWORD err_code = GetLastError();
            unregister_hotkeys(hwnd, bindings);
            DestroyWindow(hwnd);
            report_win32_error(L"GetMessageW", err_code);
            return EXIT_FAILURE;
        }
        if(get_result == 0) {
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    unregister_hotkeys(hwnd, bindings);
    return static_cast<int>(msg.wParam);
}

} // namespace

auto run_app() -> int {
    auto *const h_instance = GetModuleHandleW(nullptr);
    if(h_instance == nullptr) {
        report_win32_error(L"GetModuleHandleW(nullptr)");
        return EXIT_FAILURE;
    }

    // 1. Enable Per-Monitor V2 DPI Awareness
    if(!init_dpi_awareness()) {
        report_win32_error(L"SetProcessDpiAwarenessContext");
        return EXIT_FAILURE;
    }

    // 2. Ensure images directory exists
    std::wstring exe_dir;
    if(!get_exe_directory(exe_dir)) {
        return EXIT_FAILURE;
    }
    const auto images_dir = join_path(exe_dir, k_images_dir_name);
    if(CreateDirectoryW(images_dir.c_str(), nullptr) == FALSE) {
        const DWORD err_code = GetLastError();
        if(err_code != ERROR_ALREADY_EXISTS) {
            report_win32_error(L"CreateDirectoryW for '" + images_dir + L"'", err_code);
            return EXIT_FAILURE;
        }
    }

    // 3. Start GDI+ session
    const gdiplus_session gdi_session;
    if(gdi_session.status() != Gdiplus::Ok) {
        const std::wstring msg =
            L"GdiplusStartup failed with status " + std::to_wstring(static_cast<int>(gdi_session.status())) + L".";
        show_error_dialog(msg.c_str());
        return EXIT_FAILURE;
    }

    // 4. Load configuration
    const auto config_path = join_path(exe_dir, k_config_file_name);
    app_config config{};
    if(!load_config(config_path, config)) {
        return EXIT_FAILURE;
    }

    // 5. Query monitors and compute window placement
    std::vector<RECT> monitors;
    if(!get_all_monitor_rects(monitors)) {
        return EXIT_FAILURE;
    }
    const std::size_t target_index = select_monitor_index(monitors.size(), config.monitor_number);
    const auto mon_rect = monitors[target_index];
    const bool is_windowed = (config.win_mode == display_mode::window);
    const auto placement = compute_window_placement(mon_rect, is_windowed);

    if(placement.width <= 0 || placement.height <= 0) {
        const std::wstring msg = L"Invalid monitor dimensions detected: " + std::to_wstring(placement.width) + L"x"
                                 + std::to_wstring(placement.height) + L".";
        show_error_dialog(msg.c_str());
        return EXIT_FAILURE;
    }

    // 6. Discover strategy folders
    std::vector<strat_folder> folders;
    if(!discover_folders(images_dir, folders)) {
        return EXIT_FAILURE;
    }

    app_state app{
        .mode = view_mode::folder_list,
        .win_mode = config.win_mode,
        .in_size_move = false,
        .is_resizing = false,
        .folders = std::move(folders),
        .selected_folder_index = 0,
        .current_slide_index = 0,
        .scroll_offset = 0,
        .mon_width = placement.width,
        .mon_height = placement.height,
        .images_dir = images_dir,
        .global_hotkeys = std::move(config.global_hotkeys),
        .window_keys = std::move(config.window_keys),
        .footer_legend = std::move(config.footer_legend),
        .close_keys_label = std::move(config.close_keys_label),
    };

    // 7. Register window class and create window
    auto *cursor = LoadCursor(nullptr, IDC_ARROW);
    if(cursor == nullptr) {
        report_win32_error(L"LoadCursor(IDC_ARROW)");
        return EXIT_FAILURE;
    }

    WNDCLASSEXW wnd_class{};
    wnd_class.cbSize = sizeof(WNDCLASSEXW);
    wnd_class.lpfnWndProc = wnd_proc;
    wnd_class.hInstance = h_instance;
    wnd_class.hCursor = cursor;
    wnd_class.lpszClassName = k_window_class_name;
    if(RegisterClassExW(&wnd_class) == 0) {
        report_win32_error(L"RegisterClassExW");
        return EXIT_FAILURE;
    }

    auto *hwnd = CreateWindowExW(placement.ex_style,
                                 wnd_class.lpszClassName,
                                 k_window_title,
                                 placement.style,
                                 placement.x,
                                 placement.y,
                                 placement.width,
                                 placement.height,
                                 nullptr,
                                 nullptr,
                                 h_instance,
                                 &app);

    if(hwnd == nullptr) {
        report_win32_error(L"CreateWindowExW");
        return EXIT_FAILURE;
    }

    ShowWindow(hwnd, SW_SHOW);
    SetForegroundWindow(hwnd);

    // 8. Register hotkeys and run loop
    if(!register_hotkeys(hwnd, app.global_hotkeys)) {
        return EXIT_FAILURE;
    }
    return run_message_loop(hwnd, app.global_hotkeys);
}
