// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#include "RaidViewer/input_manager.hpp"

#include "RaidViewer/platform.hpp"

#include <algorithm>
#include <string>
#include <vector>

[[nodiscard]] auto dispatch_action(HWND hwnd, app_state &app, action_id action) -> bool {
    switch(action) {
    case action_id::close:
        if(handle_close_request(app)) {
            DestroyWindow(hwnd);
            return true;
        }
        break;
    case action_id::next:
        select_next(app);
        break;
    case action_id::prev:
        select_previous(app);
        break;
    case action_id::selection:
        if(app.mode != view_mode::folder_list) {
            return true;
        }
        if(!open_selected_folder(app)) {
            return false;
        }
        break;
    }

    InvalidateRect(hwnd, nullptr, FALSE);
    UpdateWindow(hwnd);
    return true;
}

[[nodiscard]] auto dispatch_hotkey(HWND hwnd, app_state &app, WPARAM w_param) -> bool {
    const auto hotkey_id = static_cast<int>(w_param);
    for(const auto &binding: app.global_hotkeys) {
        if(binding.id == hotkey_id) {
            return dispatch_action(hwnd, app, binding.action);
        }
    }

    const std::wstring msg = L"Received unknown hotkey ID: " + std::to_wstring(w_param);
    show_error_dialog(msg.c_str());
    return false;
}

[[nodiscard]] auto get_active_modifiers() noexcept -> UINT {
    UINT mods = 0;
    if((GetKeyState(VK_CONTROL) & 0x8000) != 0) {
        mods |= MOD_CONTROL;
    }
    if((GetKeyState(VK_MENU) & 0x8000) != 0) {
        mods |= MOD_ALT;
    }
    if((GetKeyState(VK_SHIFT) & 0x8000) != 0) {
        mods |= MOD_SHIFT;
    }
    if((GetKeyState(VK_LWIN) & 0x8000) != 0 || (GetKeyState(VK_RWIN) & 0x8000) != 0) {
        mods |= MOD_WIN;
    }
    return mods;
}

[[nodiscard]] auto match_window_key(const app_state &app, WPARAM w_param, action_id &out_action) noexcept -> bool {
    const auto vkey = static_cast<UINT>(w_param);
    const UINT active_mod = get_active_modifiers();
    for(const auto &binding: app.window_keys) {
        if(binding.combo.vk == vkey && binding.combo.mod == active_mod) {
            out_action = binding.action;
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto register_hotkeys(HWND hwnd, const std::vector<global_hotkey_binding> &bindings) -> bool {
    return std::ranges::all_of(bindings, [hwnd](const auto &binding) -> bool {
        if(RegisterHotKey(hwnd, binding.id, binding.combo.mod | MOD_NOREPEAT, binding.combo.vk) == FALSE) {
            const DWORD err_code = GetLastError();
            DestroyWindow(hwnd);
            report_win32_error(L"RegisterHotKey (ID " + std::to_wstring(binding.id) + L")", err_code);
            return false;
        }
        return true;
    });
}

void unregister_hotkeys(HWND hwnd, const std::vector<global_hotkey_binding> &bindings) noexcept {
    for(const auto &binding: bindings) {
        UnregisterHotKey(hwnd, binding.id);
    }
}
