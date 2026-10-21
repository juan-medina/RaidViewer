// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#include "RaidViewer/strategy_deck.hpp"

// clang-format off
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlwapi.h>
// clang-format on

#include <algorithm>
#include <array>
#include <cwctype>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

[[nodiscard]] auto is_image_file(std::wstring_view filename) -> bool {
    static constexpr std::array k_extensions{
        std::wstring_view{L".png"},
        std::wstring_view{L".bmp"},
        std::wstring_view{L".jpg"},
        std::wstring_view{L".jpeg"},
    };
    const auto dot_pos = filename.find_last_of(L'.');
    if(dot_pos == std::wstring_view::npos) {
        return false;
    }
    std::wstring ext(filename.substr(dot_pos));
    for(auto &chr: ext) {
        chr = static_cast<wchar_t>(std::towlower(chr));
    }
    return std::ranges::any_of(k_extensions, [&](auto valid_ext) -> bool { return ext == valid_ext; });
}

[[nodiscard]] auto bake_slide_to_screen_bitmap(const std::wstring &file_path, int screen_w, int screen_h)
    -> unique_bitmap {
    if(screen_w <= 0 || screen_h <= 0) {
        show_error_dialog(L"Invalid screen dimensions for slide baking.");
        return nullptr;
    }

    Gdiplus::Bitmap src(file_path.c_str());
    const auto status = src.GetLastStatus();
    if(status != Gdiplus::Ok) {
        const std::wstring msg = L"GDI+ failed to load image '" + file_path + L"' (Gdiplus::Status "
                                 + std::to_wstring(static_cast<int>(status)) + L").";
        show_error_dialog(msg.c_str());
        return nullptr;
    }
    if(src.GetWidth() == 0 || src.GetHeight() == 0) {
        const std::wstring msg = L"Image has zero width or height: '" + file_path + L"'.";
        show_error_dialog(msg.c_str());
        return nullptr;
    }

    auto *hdc_screen = GetDC(nullptr);
    if(hdc_screen == nullptr) {
        report_win32_error(L"GetDC(nullptr)");
        return nullptr;
    }

    auto *hdc_mem = CreateCompatibleDC(hdc_screen);
    if(hdc_mem == nullptr) {
        const DWORD err_code = GetLastError();
        ReleaseDC(nullptr, hdc_screen);
        report_win32_error(L"CreateCompatibleDC", err_code);
        return nullptr;
    }

    unique_bitmap bitmap(CreateCompatibleBitmap(hdc_screen, screen_w, screen_h));
    if(!bitmap) {
        const DWORD err_code = GetLastError();
        DeleteDC(hdc_mem);
        ReleaseDC(nullptr, hdc_screen);
        report_win32_error(L"CreateCompatibleBitmap", err_code);
        return nullptr;
    }

    auto *old_bmp = SelectObject(hdc_mem, bitmap.get());
    if(old_bmp == nullptr) {
        const DWORD err_code = GetLastError();
        DeleteDC(hdc_mem);
        ReleaseDC(nullptr, hdc_screen);
        report_win32_error(L"SelectObject(bitmap)", err_code);
        return nullptr;
    }

    const RECT full_rect{0, 0, screen_w, screen_h};
    auto *black_brush = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    if(black_brush == nullptr || FillRect(hdc_mem, &full_rect, black_brush) == 0) {
        const DWORD err_code = GetLastError();
        SelectObject(hdc_mem, old_bmp);
        DeleteDC(hdc_mem);
        ReleaseDC(nullptr, hdc_screen);
        report_win32_error(L"FillRect", err_code);
        return nullptr;
    }

    const auto scale =
        std::min(static_cast<double>(screen_w) / src.GetWidth(), static_cast<double>(screen_h) / src.GetHeight());

    const auto dest_w = static_cast<int>(src.GetWidth() * scale);
    const auto dest_h = static_cast<int>(src.GetHeight() * scale);
    const auto dest_x = (screen_w - dest_w) / 2;
    const auto dest_y = (screen_h - dest_h) / 2;

    Gdiplus::Graphics graphics(hdc_mem);
    if(graphics.GetLastStatus() != Gdiplus::Ok) {
        SelectObject(hdc_mem, old_bmp);
        DeleteDC(hdc_mem);
        ReleaseDC(nullptr, hdc_screen);
        show_error_dialog(L"Failed to create Gdiplus::Graphics context.");
        return nullptr;
    }
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    const auto draw_status = graphics.DrawImage(&src, dest_x, dest_y, dest_w, dest_h);

    SelectObject(hdc_mem, old_bmp);
    DeleteDC(hdc_mem);
    ReleaseDC(nullptr, hdc_screen);

    if(draw_status != Gdiplus::Ok) {
        const std::wstring msg = L"Gdiplus::Graphics::DrawImage failed for '" + file_path + L"' (Gdiplus::Status "
                                 + std::to_wstring(static_cast<int>(draw_status)) + L").";
        show_error_dialog(msg.c_str());
        return nullptr;
    }

    return bitmap;
}

[[nodiscard]] auto load_folder_slides(strat_folder &folder, int screen_w, int screen_h) -> bool {
    folder.slides.clear();
    std::vector<std::wstring> image_names;

    WIN32_FIND_DATAW find_data{};
    const auto search_pattern = join_path(folder.path, L"*");
    const find_file_handle find_handle(FindFirstFileW(search_pattern.c_str(), &find_data));
    if(!find_handle.valid()) {
        const DWORD err_code = GetLastError();
        if(err_code != ERROR_FILE_NOT_FOUND) {
            report_win32_error(L"FindFirstFileW in '" + folder.path + L"'", err_code);
            return false;
        }
        folder.loaded = true;
        return true;
    }

    while(true) {
        const std::wstring_view entry_name(static_cast<const wchar_t *>(find_data.cFileName));
        if(!is_dot_entry(entry_name)) {
            const bool is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            if(!is_dir && is_image_file(entry_name)) {
                image_names.emplace_back(entry_name);
            }
        }
        if(FindNextFileW(find_handle.get(), &find_data) == FALSE) {
            break;
        }
    }

    const DWORD iter_err = GetLastError();
    if(iter_err != ERROR_NO_MORE_FILES) {
        report_win32_error(L"FindNextFileW in '" + folder.path + L"'", iter_err);
        return false;
    }

    std::ranges::sort(image_names, [](const auto &lhs, const auto &rhs) -> bool {
        const auto cmp = StrCmpIW(lhs.c_str(), rhs.c_str());
        if(cmp != 0) {
            return cmp < 0;
        }
        return lhs < rhs;
    });

    for(const auto &name: image_names) {
        auto slide = bake_slide_to_screen_bitmap(join_path(folder.path, name), screen_w, screen_h);
        if(!slide) {
            return false;
        }
        folder.slides.push_back(std::move(slide));
    }
    folder.loaded = true;
    return true;
}

[[nodiscard]] auto discover_folders(std::wstring_view images_dir, std::vector<strat_folder> &out_folders) -> bool {
    out_folders.clear();
    bool has_loose_images = false;

    WIN32_FIND_DATAW find_data{};
    const auto search_pattern = join_path(images_dir, L"*");
    const find_file_handle find_handle(FindFirstFileW(search_pattern.c_str(), &find_data));
    if(!find_handle.valid()) {
        const DWORD err_code = GetLastError();
        if(err_code != ERROR_FILE_NOT_FOUND && err_code != ERROR_PATH_NOT_FOUND) {
            report_win32_error(L"FindFirstFileW in '" + std::wstring(images_dir) + L"'", err_code);
            return false;
        }
    } else {
        while(true) {
            const std::wstring_view entry_name(static_cast<const wchar_t *>(find_data.cFileName));
            if(!is_dot_entry(entry_name)) {
                const bool is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                if(is_dir) {
                    out_folders.push_back(strat_folder{
                        .name = std::wstring(entry_name),
                        .path = join_path(images_dir, entry_name),
                        .slides = {},
                        .loaded = false,
                    });
                } else if(!has_loose_images && is_image_file(entry_name)) {
                    has_loose_images = true;
                }
            }
            if(FindNextFileW(find_handle.get(), &find_data) == FALSE) {
                break;
            }
        }

        const DWORD iter_err = GetLastError();
        if(iter_err != ERROR_NO_MORE_FILES) {
            report_win32_error(L"FindNextFileW in '" + std::wstring(images_dir) + L"'", iter_err);
            return false;
        }
    }

    std::ranges::sort(out_folders, [](const auto &lhs, const auto &rhs) -> bool {
        const auto cmp = StrCmpIW(lhs.name.c_str(), rhs.name.c_str());
        if(cmp != 0) {
            return cmp < 0;
        }
        return lhs.name < rhs.name;
    });

    if(out_folders.empty() && has_loose_images) {
        out_folders.push_back(strat_folder{
            .name = L"Default (images/)",
            .path = std::wstring(images_dir),
            .slides = {},
            .loaded = false,
        });
    }

    return true;
}

void invalidate_loaded_slides(std::vector<strat_folder> &folders) noexcept {
    for(auto &folder: folders) {
        folder.slides.clear();
        folder.loaded = false;
    }
}
