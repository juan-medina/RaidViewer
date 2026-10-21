// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#pragma once

// clang-format off
#include <windows.h>
#include <shlwapi.h>
// clang-format on

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

// --- RAII Resource Wrappers ---
struct bitmap_deleter {
    void operator()(HBITMAP bmp) const noexcept {
        if(bmp != nullptr) {
            DeleteObject(bmp);
        }
    }
};

using unique_bitmap = std::unique_ptr<std::remove_pointer_t<HBITMAP>, bitmap_deleter>;

class find_file_handle {
public:
    explicit find_file_handle(HANDLE handle) noexcept: handle_(handle) {}

    ~find_file_handle() {
        if(handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr) {
            FindClose(handle_);
        }
    }

    find_file_handle(const find_file_handle &) = delete;
    auto operator=(const find_file_handle &) -> find_file_handle & = delete;
    find_file_handle(find_file_handle &&) = delete;
    auto operator=(find_file_handle &&) -> find_file_handle & = delete;

    [[nodiscard]] auto valid() const noexcept -> bool {
        return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr;
    }

    [[nodiscard]] auto get() const noexcept -> HANDLE {
        return handle_;
    }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

class file_handle {
public:
    explicit file_handle(HANDLE handle) noexcept: handle_(handle) {}

    ~file_handle() {
        if(handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr) {
            CloseHandle(handle_);
        }
    }

    file_handle(const file_handle &) = delete;
    auto operator=(const file_handle &) -> file_handle & = delete;
    file_handle(file_handle &&) = delete;
    auto operator=(file_handle &&) -> file_handle & = delete;

    [[nodiscard]] auto valid() const noexcept -> bool {
        return handle_ != INVALID_HANDLE_VALUE && handle_ != nullptr;
    }

    [[nodiscard]] auto get() const noexcept -> HANDLE {
        return handle_;
    }

private:
    HANDLE handle_ = INVALID_HANDLE_VALUE;
};

// --- Error Reporting & Path Utilities ---
void show_error_dialog(const wchar_t *message) noexcept;
void report_win32_error(std::wstring_view action, DWORD error_code = GetLastError());
[[nodiscard]] auto join_path(std::wstring_view base, std::wstring_view child) -> std::wstring;
[[nodiscard]] auto resolve_path(std::wstring_view base_dir, std::wstring_view target_path) -> std::wstring;
[[nodiscard]] auto directory_exists(std::wstring_view path) -> bool;
[[nodiscard]] auto is_dot_entry(std::wstring_view name) noexcept -> bool;
[[nodiscard]] auto get_exe_directory(std::wstring &out_dir) -> bool;
