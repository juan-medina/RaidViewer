// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

// clang-format off
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
#include <shlwapi.h>
// clang-format on

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cwctype>
#include <exception>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace fs = std::filesystem;

namespace {

// --- Hotkey Configuration ---
enum class hotkey_id : std::uint8_t {
    prev = 1,
    next = 2,
    selection = 3,
    close = 4,
};

struct hotkey_binding {
    hotkey_id id;
    UINT mod;
    UINT vk;
};

constexpr std::array<hotkey_binding, 4> k_hotkeys = {{
    {.id = hotkey_id::prev, .mod = MOD_NOREPEAT, .vk = VK_F13},
    {.id = hotkey_id::next, .mod = MOD_NOREPEAT, .vk = VK_F14},
    {.id = hotkey_id::selection, .mod = MOD_NOREPEAT, .vk = VK_F15},
    {.id = hotkey_id::close, .mod = MOD_NOREPEAT, .vk = VK_F16},
}};

// --- Constants ---
constexpr auto k_window_class_name = L"RaidViewerWindow";
constexpr auto k_window_title = L"RaidViewer";
constexpr auto k_images_dir_name = L"images";

// Custom deleter for HBITMAP
using unique_bitmap = std::unique_ptr<std::remove_pointer_t<HBITMAP>, decltype([](HBITMAP bmp) -> void {
                                          if(bmp != nullptr) {
                                              DeleteObject(bmp);
                                          }
                                      })>;

enum class view_mode : std::uint8_t {
    folder_list,
    image_viewer,
};

struct strat_folder {
    std::wstring name;
    fs::path path;
    std::vector<unique_bitmap> slides;
    bool loaded = false;
};

auto to_utf8(std::wstring_view wide) -> std::string {
    if(wide.empty()) {
        return {};
    }
    const int size_needed =
        WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if(size_needed <= 0) {
        return "<encoding error>";
    }
    std::string result(static_cast<std::size_t>(size_needed), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), result.data(), size_needed, nullptr, nullptr);
    return result;
}

auto format_win32_error(std::string_view action, DWORD error_code = GetLastError()) -> std::string {
    return std::string(action) + " failed (Win32 error " + std::to_string(error_code) + ").";
}

void show_error_dialog(const char *message) noexcept {
    MessageBoxA(nullptr, message, "RaidViewer Fatal Error", MB_OK | MB_ICONERROR | MB_TOPMOST);
}

// Get the directory where RaidViewer.exe lives
auto get_exe_directory() -> fs::path {
    std::array<wchar_t, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), MAX_PATH);
    if(length == 0 || length >= MAX_PATH) {
        throw std::runtime_error(format_win32_error("GetModuleFileNameW"));
    }
    const auto exe_dir = fs::path(buffer.data()).parent_path();
    if(exe_dir.empty()) {
        throw std::runtime_error("Executable parent directory is empty.");
    }
    return exe_dir;
}

auto is_image_file(const fs::path &path) -> bool {
    static constexpr std::array k_extensions{
        std::wstring_view{L".png"},
        std::wstring_view{L".bmp"},
        std::wstring_view{L".jpg"},
        std::wstring_view{L".jpeg"},
    };
    auto ext = path.extension().wstring();
    for(auto &chr: ext) {
        chr = static_cast<wchar_t>(std::towlower(chr));
    }
    return std::ranges::any_of(k_extensions, [&](auto valid_ext) -> bool { return ext == valid_ext; });
}

// Loads PNG/BMP/JPG, scales to screen size conserving aspect ratio, fills
// letterbox with black
auto bake_slide_to_screen_bitmap(const fs::path &file_path, int screen_w, int screen_h) -> unique_bitmap {
    if(screen_w <= 0 || screen_h <= 0) {
        throw std::runtime_error("Invalid screen dimensions for slide baking.");
    }

    Gdiplus::Bitmap src(file_path.c_str());
    const auto status = src.GetLastStatus();
    if(status != Gdiplus::Ok) {
        throw std::runtime_error("GDI+ failed to load image '" + to_utf8(file_path.wstring()) + "' (Gdiplus::Status "
                                 + std::to_string(static_cast<int>(status)) + ").");
    }
    if(src.GetWidth() == 0 || src.GetHeight() == 0) {
        throw std::runtime_error("Image has zero width or height: '" + to_utf8(file_path.wstring()) + "'.");
    }

    auto *hdc_screen = GetDC(nullptr);
    if(hdc_screen == nullptr) {
        throw std::runtime_error(format_win32_error("GetDC(nullptr)"));
    }

    auto *hdc_mem = CreateCompatibleDC(hdc_screen);
    if(hdc_mem == nullptr) {
        ReleaseDC(nullptr, hdc_screen);
        throw std::runtime_error(format_win32_error("CreateCompatibleDC"));
    }

    unique_bitmap bitmap(CreateCompatibleBitmap(hdc_screen, screen_w, screen_h));
    if(!bitmap) {
        const DWORD err_code = GetLastError();
        DeleteDC(hdc_mem);
        ReleaseDC(nullptr, hdc_screen);
        throw std::runtime_error(format_win32_error("CreateCompatibleBitmap", err_code));
    }

    auto *old_bmp = SelectObject(hdc_mem, bitmap.get());
    if(old_bmp == nullptr) {
        const DWORD err_code = GetLastError();
        DeleteDC(hdc_mem);
        ReleaseDC(nullptr, hdc_screen);
        throw std::runtime_error(format_win32_error("SelectObject(bitmap)", err_code));
    }

    const RECT full_rect{0, 0, screen_w, screen_h};
    auto *black_brush = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    if(black_brush == nullptr || FillRect(hdc_mem, &full_rect, black_brush) == 0) {
        const DWORD err_code = GetLastError();
        SelectObject(hdc_mem, old_bmp);
        DeleteDC(hdc_mem);
        ReleaseDC(nullptr, hdc_screen);
        throw std::runtime_error(format_win32_error("FillRect", err_code));
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
        throw std::runtime_error("Failed to create Gdiplus::Graphics context.");
    }
    graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
    const auto draw_status = graphics.DrawImage(&src, dest_x, dest_y, dest_w, dest_h);

    SelectObject(hdc_mem, old_bmp);
    DeleteDC(hdc_mem);
    ReleaseDC(nullptr, hdc_screen);

    if(draw_status != Gdiplus::Ok) {
        throw std::runtime_error("Gdiplus::Graphics::DrawImage failed for '" + to_utf8(file_path.wstring())
                                 + "' (Gdiplus::Status " + std::to_string(static_cast<int>(draw_status)) + ").");
    }

    return bitmap;
}

void load_folder_slides(strat_folder &folder, int screen_w, int screen_h) {
    folder.slides.clear();
    std::error_code err;
    std::vector<fs::path> image_files;
    const fs::directory_iterator dir_it(folder.path, err);
    if(err) {
        throw std::runtime_error("Failed to read strategy folder '" + to_utf8(folder.path.wstring())
                                 + "': " + err.message());
    }

    for(const auto &entry: dir_it) {
        const bool is_reg = entry.is_regular_file(err);
        if(err) {
            throw std::runtime_error("Failed to inspect file '" + to_utf8(entry.path().wstring())
                                     + "': " + err.message());
        }
        if(is_reg && is_image_file(entry.path())) {
            image_files.push_back(entry.path());
        }
    }

    std::ranges::sort(image_files, [](const auto &lhs, const auto &rhs) -> bool {
        const auto cmp = StrCmpIW(lhs.filename().c_str(), rhs.filename().c_str());
        if(cmp != 0) {
            return cmp < 0;
        }
        return lhs.filename().native() < rhs.filename().native();
    });

    for(const auto &file: image_files) {
        folder.slides.push_back(bake_slide_to_screen_bitmap(file, screen_w, screen_h));
    }
    folder.loaded = true;
}

// --- Application State ---
struct app_state {
    view_mode mode = view_mode::folder_list;
    std::vector<strat_folder> folders;
    int selected_folder_index = 0;
    int current_slide_index = 0;
    int scroll_offset = 0;
    int mon_width = 0;
    int mon_height = 0;
    fs::path images_dir;
};

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

void open_selected_folder(app_state &app) {
    if(app.mode == view_mode::folder_list && !app.folders.empty()) {
        auto &folder = app.folders[app.selected_folder_index];
        if(!folder.loaded) {
            load_folder_slides(folder, app.mon_width, app.mon_height);
        }
        app.current_slide_index = 0;
        app.mode = view_mode::image_viewer;
    }
}

void handle_close_action(HWND hwnd, app_state &app) noexcept {
    if(app.mode == view_mode::image_viewer) {
        app.mode = view_mode::folder_list;
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);
    } else {
        DestroyWindow(hwnd);
    }
}

void discover_folders(app_state &app) {
    app.folders.clear();
    std::error_code err;
    const fs::directory_iterator dir_it(app.images_dir, err);
    if(err) {
        throw std::runtime_error("Failed to open images directory '" + to_utf8(app.images_dir.wstring())
                                 + "': " + err.message());
    }

    for(const auto &entry: dir_it) {
        const bool is_dir = entry.is_directory(err);
        if(err) {
            throw std::runtime_error("Failed to inspect directory entry '" + to_utf8(entry.path().wstring())
                                     + "': " + err.message());
        }
        if(is_dir) {
            app.folders.push_back(strat_folder{
                .name = entry.path().filename().wstring(),
                .path = entry.path(),
                .slides = {},
                .loaded = false,
            });
        }
    }

    std::ranges::sort(app.folders, [](const auto &lhs, const auto &rhs) -> bool {
        const auto cmp = StrCmpIW(lhs.name.c_str(), rhs.name.c_str());
        if(cmp != 0) {
            return cmp < 0;
        }
        return lhs.name < rhs.name;
    });

    if(app.folders.empty()) {
        bool has_loose_images = false;
        const fs::directory_iterator loose_it(app.images_dir, err);
        if(err) {
            throw std::runtime_error("Failed to scan images directory '" + to_utf8(app.images_dir.wstring())
                                     + "': " + err.message());
        }
        for(const auto &entry: loose_it) {
            const bool is_reg = entry.is_regular_file(err);
            if(err) {
                throw std::runtime_error("Failed to inspect file '" + to_utf8(entry.path().wstring())
                                         + "': " + err.message());
            }
            if(is_reg && is_image_file(entry.path())) {
                has_loose_images = true;
                break;
            }
        }
        if(has_loose_images) {
            app.folders.push_back(strat_folder{
                .name = L"Default (images/)",
                .path = app.images_dir,
                .slides = {},
                .loaded = false,
            });
        }
    }

    if(std::cmp_greater_equal(app.selected_folder_index, app.folders.size())) {
        app.selected_folder_index = 0;
    }
    app.scroll_offset = 0;
}

struct monitor_enum_context {
    std::vector<RECT> rects;
    DWORD error_code = ERROR_SUCCESS;
    bool failed = false;
};

auto CALLBACK enum_monitors_proc(HMONITOR h_mon, HDC /*hdc*/, LPRECT /*rect*/, LPARAM l_param) -> BOOL {
    auto *ctx = std::bit_cast<monitor_enum_context *>(l_param);
    if(ctx == nullptr) {
        return FALSE;
    }
    MONITORINFO mon_info{.cbSize = sizeof(MONITORINFO), .rcMonitor = {}, .rcWork = {}, .dwFlags = 0};
    if(GetMonitorInfoW(h_mon, &mon_info) == FALSE) {
        ctx->error_code = GetLastError();
        ctx->failed = true;
        return FALSE;
    }
    ctx->rects.push_back(mon_info.rcMonitor);
    return TRUE;
}

auto get_all_monitor_rects() -> std::vector<RECT> {
    monitor_enum_context ctx{};
    auto *const ctx_ptr = &ctx;
    if(EnumDisplayMonitors(nullptr, nullptr, enum_monitors_proc, std::bit_cast<LPARAM>(ctx_ptr)) == FALSE) {
        if(ctx.failed) {
            throw std::runtime_error(format_win32_error("GetMonitorInfoW", ctx.error_code));
        }
        throw std::runtime_error(format_win32_error("EnumDisplayMonitors"));
    }
    if(ctx.failed) {
        throw std::runtime_error(format_win32_error("GetMonitorInfoW", ctx.error_code));
    }
    if(ctx.rects.empty()) {
        throw std::runtime_error("No display monitors were detected by EnumDisplayMonitors.");
    }
    return ctx.rects;
}

void render_folder_list(HDC hdc, const app_state &app) {
    auto *hdc_mem = CreateCompatibleDC(hdc);
    if(hdc_mem == nullptr) {
        throw std::runtime_error(format_win32_error("CreateCompatibleDC in render_folder_list"));
    }
    auto *hbm_mem = CreateCompatibleBitmap(hdc, app.mon_width, app.mon_height);
    if(hbm_mem == nullptr) {
        const DWORD err_code = GetLastError();
        DeleteDC(hdc_mem);
        throw std::runtime_error(format_win32_error("CreateCompatibleBitmap in render_folder_list", err_code));
    }
    auto *old_bmp = SelectObject(hdc_mem, hbm_mem);

    const RECT full_rect{0, 0, app.mon_width, app.mon_height};
    auto *bg_brush = CreateSolidBrush(RGB(14, 17, 23));
    FillRect(hdc_mem, &full_rect, bg_brush);
    DeleteObject(bg_brush);

    SetBkMode(hdc_mem, TRANSPARENT);

    const int dpi_y = GetDeviceCaps(hdc_mem, LOGPIXELSY);
    const int pt20_height = -MulDiv(20, dpi_y, 72);
    const int pt14_height = -MulDiv(14, dpi_y, 72);
    const int pt24_height = -MulDiv(24, dpi_y, 72);

    auto *font_title = CreateFontW(pt24_height,
                                   0,
                                   0,
                                   0,
                                   FW_BOLD,
                                   FALSE,
                                   FALSE,
                                   FALSE,
                                   DEFAULT_CHARSET,
                                   OUT_DEFAULT_PRECIS,
                                   CLIP_DEFAULT_PRECIS,
                                   CLEARTYPE_QUALITY,
                                   DEFAULT_PITCH | FF_SWISS,
                                   L"Segoe UI");

    auto *font_item = CreateFontW(pt20_height,
                                  0,
                                  0,
                                  0,
                                  FW_SEMIBOLD,
                                  FALSE,
                                  FALSE,
                                  FALSE,
                                  DEFAULT_CHARSET,
                                  OUT_DEFAULT_PRECIS,
                                  CLIP_DEFAULT_PRECIS,
                                  CLEARTYPE_QUALITY,
                                  DEFAULT_PITCH | FF_SWISS,
                                  L"Segoe UI");

    auto *font_footer = CreateFontW(pt14_height,
                                    0,
                                    0,
                                    0,
                                    FW_NORMAL,
                                    FALSE,
                                    FALSE,
                                    FALSE,
                                    DEFAULT_CHARSET,
                                    OUT_DEFAULT_PRECIS,
                                    CLIP_DEFAULT_PRECIS,
                                    CLEARTYPE_QUALITY,
                                    DEFAULT_PITCH | FF_SWISS,
                                    L"Segoe UI");

    // Header
    SelectObject(hdc_mem, font_title);
    SetTextColor(hdc_mem, RGB(240, 246, 252));
    RECT title_rect{80, 45, app.mon_width - 80, 95};
    DrawTextW(hdc_mem, L"RAID STRATEGY SELECTOR", -1, &title_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(hdc_mem, font_footer);
    SetTextColor(hdc_mem, RGB(139, 148, 158));
    RECT sub_rect{80, 95, app.mon_width - 80, 130};
    DrawTextW(hdc_mem,
              L"Choose a raid strategy folder to view diagrams",
              -1,
              &sub_rect,
              DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    // Folder Items
    if(app.folders.empty()) {
        SelectObject(hdc_mem, font_item);
        SetTextColor(hdc_mem, RGB(180, 180, 180));
        RECT empty_rect{80, 200, app.mon_width - 80, 320};
        DrawTextW(hdc_mem,
                  L"No folders found inside 'images'.\n"
                  L"Create strategy subfolders (e.g. 'M12S - Phase 1') inside the 'images' folder.\n"
                  L"(F16 or Esc to close)",
                  -1,
                  &empty_rect,
                  DT_LEFT | DT_NOPREFIX);
    } else {
        const int start_y = 150;
        const int row_h = 64;
        const int gap = 8;
        const int visible_rows = get_visible_row_count(app);
        const int total_folders = static_cast<int>(app.folders.size());

        SelectObject(hdc_mem, font_item);

        for(int idx = 0; idx < visible_rows; ++idx) {
            const int folder_idx = app.scroll_offset + idx;
            if(folder_idx >= total_folders) {
                break;
            }

            const int row_top = start_y + (idx * (row_h + gap));
            const int row_bottom = row_top + row_h;
            const RECT row_rect{80, row_top, app.mon_width - 80, row_bottom};

            const bool is_selected = (folder_idx == app.selected_folder_index);

            if(is_selected) {
                auto *hl_brush = CreateSolidBrush(RGB(30, 58, 102));
                FillRect(hdc_mem, &row_rect, hl_brush);
                DeleteObject(hl_brush);

                const RECT bar_rect{80, row_top, 86, row_bottom};
                auto *bar_brush = CreateSolidBrush(RGB(88, 166, 255));
                FillRect(hdc_mem, &bar_rect, bar_brush);
                DeleteObject(bar_brush);

                SetTextColor(hdc_mem, RGB(255, 255, 255));
                const std::wstring label = L"  \x25B8  " + app.folders[folder_idx].name;
                RECT text_rect{96, row_top, app.mon_width - 96, row_bottom};
                DrawTextW(hdc_mem, label.c_str(), -1, &text_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            } else {
                auto *row_brush = CreateSolidBrush(RGB(22, 27, 34));
                FillRect(hdc_mem, &row_rect, row_brush);
                DeleteObject(row_brush);

                SetTextColor(hdc_mem, RGB(160, 175, 195));
                const std::wstring label = L"     " + app.folders[folder_idx].name;
                RECT text_rect{96, row_top, app.mon_width - 96, row_bottom};
                DrawTextW(hdc_mem, label.c_str(), -1, &text_rect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
            }
        }
    }

    // Footer divider and controls legend
    const int footer_line_y = app.mon_height - 85;
    auto *pen = CreatePen(PS_SOLID, 1, RGB(48, 54, 61));
    auto *old_pen = SelectObject(hdc_mem, pen);
    MoveToEx(hdc_mem, 80, footer_line_y, nullptr);
    LineTo(hdc_mem, app.mon_width - 80, footer_line_y);
    SelectObject(hdc_mem, old_pen);
    DeleteObject(pen);

    SelectObject(hdc_mem, font_footer);
    SetTextColor(hdc_mem, RGB(139, 148, 158));
    RECT footer_rect{80, footer_line_y + 10, app.mon_width - 80, app.mon_height - 15};
    DrawTextW(hdc_mem,
              L"F13 / Up / Left: Previous Strat    |    F14 / Down / Right: Next Strat    |    "
              L"F15 / Enter / Space: Open Strat    |    F16 / Esc: Back / Close",
              -1,
              &footer_rect,
              DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    BitBlt(hdc, 0, 0, app.mon_width, app.mon_height, hdc_mem, 0, 0, SRCCOPY);

    DeleteObject(font_title);
    DeleteObject(font_item);
    DeleteObject(font_footer);
    SelectObject(hdc_mem, old_bmp);
    DeleteObject(hbm_mem);
    DeleteDC(hdc_mem);
}

void render_empty_folder_notice(HDC hdc, const app_state &app, const std::wstring &folder_name) {
    RECT full_rect{0, 0, app.mon_width, app.mon_height};
    FillRect(hdc, &full_rect, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(180, 180, 180));

    const int dpi_y = GetDeviceCaps(hdc, LOGPIXELSY);
    const int pt20_height = -MulDiv(20, dpi_y, 72);
    auto *font = CreateFontW(pt20_height,
                             0,
                             0,
                             0,
                             FW_SEMIBOLD,
                             FALSE,
                             FALSE,
                             FALSE,
                             DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS,
                             CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS,
                             L"Segoe UI");
    auto *old_font = SelectObject(hdc, font);

    const std::wstring msg = L"No images found in '" + folder_name + L"'.\nPress F16 / Esc to return to folder list.";
    DrawTextW(hdc, msg.c_str(), -1, &full_rect, DT_CENTER | DT_VCENTER | DT_NOPREFIX);

    SelectObject(hdc, old_font);
    DeleteObject(font);
}

void handle_hotkey(HWND hwnd, app_state &app, WPARAM w_param) {
    const auto hotkey = static_cast<hotkey_id>(w_param);
    if(hotkey == hotkey_id::close) {
        handle_close_action(hwnd, app);
        return;
    }

    switch(hotkey) {
    case hotkey_id::next:
        select_next(app);
        break;
    case hotkey_id::prev:
        select_previous(app);
        break;
    case hotkey_id::selection:
        if(app.mode != view_mode::folder_list) {
            return;
        }
        open_selected_folder(app);
        break;
    default:
        throw std::runtime_error("Received unknown hotkey ID: " + std::to_string(w_param));
    }

    InvalidateRect(hwnd, nullptr, FALSE);
    UpdateWindow(hwnd);
}

void handle_keydown(HWND hwnd, app_state &app, WPARAM w_param) {
    switch(w_param) {
    case VK_DOWN:
    case VK_RIGHT:
        select_next(app);
        break;
    case VK_UP:
    case VK_LEFT:
        select_previous(app);
        break;
    case VK_RETURN:
    case VK_SPACE:
        if(app.mode != view_mode::folder_list) {
            return;
        }
        open_selected_folder(app);
        break;
    case VK_ESCAPE:
        handle_close_action(hwnd, app);
        return;
    default:
        return;
    }

    InvalidateRect(hwnd, nullptr, FALSE);
    UpdateWindow(hwnd);
}

void handle_paint(HWND hwnd, const app_state &app) {
    PAINTSTRUCT paint{};
    auto *hdc = BeginPaint(hwnd, &paint);
    if(hdc == nullptr) {
        throw std::runtime_error(format_win32_error("BeginPaint"));
    }

    try {
        if(app.mode == view_mode::image_viewer && !app.folders.empty()) {
            const auto &cur_folder = app.folders[app.selected_folder_index];
            if(!cur_folder.slides.empty()) {
                auto *hdc_mem = CreateCompatibleDC(hdc);
                if(hdc_mem == nullptr) {
                    throw std::runtime_error(format_win32_error("CreateCompatibleDC in handle_paint"));
                }
                auto *old_bmp = SelectObject(hdc_mem, cur_folder.slides[app.current_slide_index].get());

                BitBlt(hdc, 0, 0, app.mon_width, app.mon_height, hdc_mem, 0, 0, SRCCOPY);

                SelectObject(hdc_mem, old_bmp);
                DeleteDC(hdc_mem);
            } else {
                render_empty_folder_notice(hdc, app, cur_folder.name);
            }
        } else {
            render_folder_list(hdc, app);
        }
    } catch(...) {
        EndPaint(hwnd, &paint);
        throw;
    }

    EndPaint(hwnd, &paint);
}

auto get_app_state(HWND hwnd) -> app_state & {
    auto *app = std::bit_cast<app_state *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if(app == nullptr) {
        throw std::runtime_error("Window userdata (app_state*) is null.");
    }
    return *app;
}

auto CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) noexcept -> LRESULT {
    try {
        if(msg == WM_NCCREATE) {
            auto *create_struct = std::bit_cast<CREATESTRUCTW *>(l_param);
            if(create_struct == nullptr || create_struct->lpCreateParams == nullptr) {
                throw std::runtime_error("WM_NCCREATE received null create params.");
            }
            const auto user_data = std::bit_cast<LONG_PTR>(create_struct->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, user_data);
            return DefWindowProcW(hwnd, msg, w_param, l_param);
        }

        switch(msg) {
        case WM_HOTKEY:
            handle_hotkey(hwnd, get_app_state(hwnd), w_param);
            return 0;

        case WM_KEYDOWN:
            handle_keydown(hwnd, get_app_state(hwnd), w_param);
            return 0;

        case WM_ERASEBKGND:
            return 1; // Double-buffered / BitBlt prevents flicker

        case WM_PAINT:
            handle_paint(hwnd, get_app_state(hwnd));
            return 0;

        case WM_DESTROY:
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
            PostQuitMessage(0);
            return 0;

        default:
            break;
        }
        return DefWindowProcW(hwnd, msg, w_param, l_param);
    } catch(const std::exception &ex) {
        show_error_dialog(ex.what());
        PostQuitMessage(EXIT_FAILURE);
        return 0;
    } catch(...) {
        show_error_dialog("Unknown fatal exception inside window procedure.");
        PostQuitMessage(EXIT_FAILURE);
        return 0;
    }
}

class gdiplus_session {
public:
    gdiplus_session() {
        const Gdiplus::GdiplusStartupInput gdi_input;
        const auto status = Gdiplus::GdiplusStartup(&token_, &gdi_input, nullptr);
        if(status != Gdiplus::Ok) {
            throw std::runtime_error("GdiplusStartup failed with status " + std::to_string(static_cast<int>(status))
                                     + ".");
        }
    }

    ~gdiplus_session() {
        if(token_ != 0) {
            Gdiplus::GdiplusShutdown(token_);
        }
    }

    gdiplus_session(const gdiplus_session &) = delete;
    auto operator=(const gdiplus_session &) -> gdiplus_session & = delete;
    gdiplus_session(gdiplus_session &&) = delete;
    auto operator=(gdiplus_session &&) -> gdiplus_session & = delete;

private:
    ULONG_PTR token_ = 0;
};

auto run_app() -> int {
    auto *const h_instance = GetModuleHandleW(nullptr);
    if(h_instance == nullptr) {
        throw std::runtime_error(format_win32_error("GetModuleHandleW(nullptr)"));
    }

    // 1. Enable Per-Monitor V2 DPI Awareness for pixel-perfect scaling
    if(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) == FALSE) {
        throw std::runtime_error(format_win32_error("SetProcessDpiAwarenessContext"));
    }

    // 2. Ensure the "images" directory exists right next to the .exe
    const auto images_dir = get_exe_directory() / k_images_dir_name;
    std::error_code err;
    fs::create_directories(images_dir, err);
    if(err) {
        throw std::runtime_error("Failed to create or access images directory '" + to_utf8(images_dir.wstring())
                                 + "': " + err.message());
    }

    // 3. Start GDI+ (RAII ensures shutdown happens after app_state is destroyed)
    const gdiplus_session gdi_session;

    // 4. Query display monitor (pick 2nd monitor if available, otherwise primary)
    const auto monitors = get_all_monitor_rects();
    const std::size_t target_index = (monitors.size() > 1) ? 1U : 0U;
    const auto mon_rect = monitors[target_index];
    const int mon_width = mon_rect.right - mon_rect.left;
    const int mon_height = mon_rect.bottom - mon_rect.top;
    if(mon_width <= 0 || mon_height <= 0) {
        throw std::runtime_error("Invalid monitor dimensions detected: " + std::to_string(mon_width) + "x"
                                 + std::to_string(mon_height) + ".");
    }

    app_state app{
        .mode = view_mode::folder_list,
        .folders = {},
        .selected_folder_index = 0,
        .current_slide_index = 0,
        .scroll_offset = 0,
        .mon_width = mon_width,
        .mon_height = mon_height,
        .images_dir = images_dir,
    };

    // 5. Discover strategy folders inside images/
    discover_folders(app);

    // 6. Create borderless fullscreen window
    auto *cursor = LoadCursor(nullptr, IDC_ARROW);
    if(cursor == nullptr) {
        throw std::runtime_error(format_win32_error("LoadCursor(IDC_ARROW)"));
    }

    WNDCLASSEXW wnd_class{};
    wnd_class.cbSize = sizeof(WNDCLASSEXW);
    wnd_class.lpfnWndProc = wnd_proc;
    wnd_class.hInstance = h_instance;
    wnd_class.hCursor = cursor;
    wnd_class.lpszClassName = k_window_class_name;
    if(RegisterClassExW(&wnd_class) == 0) {
        throw std::runtime_error(format_win32_error("RegisterClassExW"));
    }

    auto *hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                                 wnd_class.lpszClassName,
                                 k_window_title,
                                 WS_POPUP | WS_VISIBLE,
                                 mon_rect.left,
                                 mon_rect.top,
                                 app.mon_width,
                                 app.mon_height,
                                 nullptr,
                                 nullptr,
                                 h_instance,
                                 &app);

    if(hwnd == nullptr) {
        throw std::runtime_error(format_win32_error("CreateWindowExW"));
    }

    // 7. Register Global Hotkeys
    for(const auto &[hotkey, mod, vk]: k_hotkeys) {
        if(RegisterHotKey(hwnd, static_cast<int>(hotkey), mod, vk) == FALSE) {
            const DWORD err_code = GetLastError();
            DestroyWindow(hwnd);
            throw std::runtime_error(
                format_win32_error("RegisterHotKey (ID " + std::to_string(static_cast<int>(hotkey)) + ")", err_code));
        }
    }

    // 8. Event loop
    MSG msg{};
    while(true) {
        const BOOL get_result = GetMessageW(&msg, nullptr, 0, 0);
        if(get_result < 0) {
            const DWORD err_code = GetLastError();
            for(const auto &[hotkey, mod, vk]: k_hotkeys) {
                UnregisterHotKey(hwnd, static_cast<int>(hotkey));
            }
            DestroyWindow(hwnd);
            throw std::runtime_error(format_win32_error("GetMessageW", err_code));
        }
        if(get_result == 0) {
            break;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // 9. Cleanup
    for(const auto &[hotkey, mod, vk]: k_hotkeys) {
        UnregisterHotKey(hwnd, static_cast<int>(hotkey));
    }
    return static_cast<int>(msg.wParam);
}

} // namespace

auto main() -> int {
    try {
        return run_app();
    } catch(const std::exception &ex) {
        show_error_dialog(ex.what());
        return EXIT_FAILURE;
    } catch(...) {
        show_error_dialog("Unknown fatal error occurred in RaidViewer.");
        return EXIT_FAILURE;
    }
}
