// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#include "RaidViewer/platform.hpp"

#include <array>
#include <string>
#include <string_view>

void show_error_dialog(const wchar_t *message) noexcept {
    MessageBoxW(nullptr, message, L"RaidViewer Fatal Error", MB_OK | MB_ICONERROR | MB_TOPMOST);
}

void report_win32_error(std::wstring_view action, DWORD error_code) {
    const std::wstring msg = std::wstring(action) + L" failed (Win32 error " + std::to_wstring(error_code) + L").";
    show_error_dialog(msg.c_str());
}

[[nodiscard]] auto join_path(std::wstring_view base, std::wstring_view child) -> std::wstring {
    if(base.empty()) {
        return std::wstring(child);
    }
    if(base.back() == L'\\' || base.back() == L'/') {
        return std::wstring(base) + std::wstring(child);
    }
    return std::wstring(base) + L"\\" + std::wstring(child);
}

[[nodiscard]] auto is_dot_entry(std::wstring_view name) noexcept -> bool {
    return name == L"." || name == L"..";
}

[[nodiscard]] auto get_exe_directory(std::wstring &out_dir) -> bool {
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), MAX_PATH);
    if(length == 0 || length >= MAX_PATH) {
        report_win32_error(L"GetModuleFileNameW");
        return false;
    }
    const std::wstring_view full_path(buffer.data(), length);
    const auto last_slash = full_path.find_last_of(L"\\/");
    if(last_slash == std::wstring_view::npos) {
        show_error_dialog(L"Executable parent directory is empty.");
        return false;
    }
    out_dir = std::wstring(full_path.substr(0, last_slash));
    return true;
}
