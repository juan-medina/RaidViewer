// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#include "RaidViewer/renderer.hpp"

#include "RaidViewer/platform.hpp"

#include <string>

gdiplus_session::gdiplus_session() {
    const Gdiplus::GdiplusStartupInput gdi_input;
    status_ = Gdiplus::GdiplusStartup(&token_, &gdi_input, nullptr);
}

gdiplus_session::~gdiplus_session() {
    if(status_ == Gdiplus::Ok && token_ != 0) {
        Gdiplus::GdiplusShutdown(token_);
    }
}

namespace {

[[nodiscard]] auto render_folder_list(HDC hdc, const app_state &app) -> bool {
    auto *hdc_mem = CreateCompatibleDC(hdc);
    if(hdc_mem == nullptr) {
        report_win32_error(L"CreateCompatibleDC in render_folder_list");
        return false;
    }
    auto *hbm_mem = CreateCompatibleBitmap(hdc, app.mon_width, app.mon_height);
    if(hbm_mem == nullptr) {
        const DWORD err_code = GetLastError();
        DeleteDC(hdc_mem);
        report_win32_error(L"CreateCompatibleBitmap in render_folder_list", err_code);
        return false;
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
    if(!app.images_dir_exists) {
        SelectObject(hdc_mem, font_item);
        SetTextColor(hdc_mem, RGB(180, 180, 180));
        RECT empty_rect{80, 200, app.mon_width - 80, app.mon_height - 100};
        const std::wstring msg = L"Image folder not found: '" + app.images_dir
                                 + L"'.\n"
                                   L"Create the directory or update RaidViewer.ini.\n("
                                 + app.close_keys_label + L" to close)";
        DrawTextW(hdc_mem, msg.c_str(), -1, &empty_rect, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
    } else if(app.folders.empty()) {
        SelectObject(hdc_mem, font_item);
        SetTextColor(hdc_mem, RGB(180, 180, 180));
        RECT empty_rect{80, 200, app.mon_width - 80, app.mon_height - 100};
        const std::wstring empty_msg =
            L"No folders found inside '" + app.images_dir
            + L"'.\n"
              L"Create strategy subfolders (e.g. 'M12S - Phase 1') inside the image folder.\n("
            + app.close_keys_label + L" to close)";
        DrawTextW(hdc_mem, empty_msg.c_str(), -1, &empty_rect, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
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
    DrawTextW(
        hdc_mem, app.footer_legend.c_str(), -1, &footer_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    BitBlt(hdc, 0, 0, app.mon_width, app.mon_height, hdc_mem, 0, 0, SRCCOPY);

    DeleteObject(font_title);
    DeleteObject(font_item);
    DeleteObject(font_footer);
    SelectObject(hdc_mem, old_bmp);
    DeleteObject(hbm_mem);
    DeleteDC(hdc_mem);
    return true;
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

    const std::wstring msg =
        L"No images found in '" + folder_name + L"'.\nPress " + app.close_keys_label + L" to return to folder list.";
    DrawTextW(hdc, msg.c_str(), -1, &full_rect, DT_CENTER | DT_VCENTER | DT_NOPREFIX);

    SelectObject(hdc, old_font);
    DeleteObject(font);
}

[[nodiscard]] auto render_resizing_notice(HDC hdc, const app_state &app) -> bool {
    auto *hdc_mem = CreateCompatibleDC(hdc);
    if(hdc_mem == nullptr) {
        report_win32_error(L"CreateCompatibleDC in render_resizing_notice");
        return false;
    }
    auto *hbm_mem = CreateCompatibleBitmap(hdc, app.mon_width, app.mon_height);
    if(hbm_mem == nullptr) {
        const DWORD err_code = GetLastError();
        DeleteDC(hdc_mem);
        report_win32_error(L"CreateCompatibleBitmap in render_resizing_notice", err_code);
        return false;
    }
    auto *old_bmp = SelectObject(hdc_mem, hbm_mem);

    RECT full_rect{0, 0, app.mon_width, app.mon_height};
    auto *bg_brush = CreateSolidBrush(RGB(14, 17, 23));
    FillRect(hdc_mem, &full_rect, bg_brush);
    DeleteObject(bg_brush);

    SetBkMode(hdc_mem, TRANSPARENT);
    SetTextColor(hdc_mem, RGB(180, 180, 180));

    const int dpi_y = GetDeviceCaps(hdc_mem, LOGPIXELSY);
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
    auto *old_font = SelectObject(hdc_mem, font);

    DrawTextW(hdc_mem, L"Resizing window...", -1, &full_rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    BitBlt(hdc, 0, 0, app.mon_width, app.mon_height, hdc_mem, 0, 0, SRCCOPY);

    SelectObject(hdc_mem, old_font);
    DeleteObject(font);
    SelectObject(hdc_mem, old_bmp);
    DeleteObject(hbm_mem);
    DeleteDC(hdc_mem);
    return true;
}

} // namespace

[[nodiscard]] auto handle_paint(HWND hwnd, const app_state &app) -> bool {
    PAINTSTRUCT paint{};
    auto *hdc = BeginPaint(hwnd, &paint);
    if(hdc == nullptr) {
        report_win32_error(L"BeginPaint");
        return false;
    }

    bool paint_ok = true;
    if(app.is_resizing) {
        paint_ok = render_resizing_notice(hdc, app);
    } else if(app.mode == view_mode::image_viewer && !app.folders.empty()) {
        const auto &cur_folder = app.folders[app.selected_folder_index];
        if(!cur_folder.slides.empty()) {
            auto *hdc_mem = CreateCompatibleDC(hdc);
            if(hdc_mem == nullptr) {
                const DWORD err_code = GetLastError();
                EndPaint(hwnd, &paint);
                report_win32_error(L"CreateCompatibleDC in handle_paint", err_code);
                return false;
            }
            auto *old_bmp = SelectObject(hdc_mem, cur_folder.slides[app.current_slide_index].get());

            BitBlt(hdc, 0, 0, app.mon_width, app.mon_height, hdc_mem, 0, 0, SRCCOPY);

            SelectObject(hdc_mem, old_bmp);
            DeleteDC(hdc_mem);
        } else {
            render_empty_folder_notice(hdc, app, cur_folder.name);
        }
    } else {
        paint_ok = render_folder_list(hdc, app);
    }

    EndPaint(hwnd, &paint);
    return paint_ok;
}
