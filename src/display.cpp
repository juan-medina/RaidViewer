// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#include "RaidViewer/display.hpp"

#include "RaidViewer/platform.hpp"

#include <bit>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace {

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

} // namespace

[[nodiscard]] auto init_dpi_awareness() noexcept -> bool {
    return SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) != FALSE;
}

[[nodiscard]] auto get_all_monitor_rects(std::vector<RECT> &out_rects) -> bool {
    monitor_enum_context ctx{};
    auto *const ctx_ptr = &ctx;
    if(EnumDisplayMonitors(nullptr, nullptr, enum_monitors_proc, std::bit_cast<LPARAM>(ctx_ptr)) == FALSE) {
        if(ctx.failed) {
            report_win32_error(L"GetMonitorInfoW", ctx.error_code);
            return false;
        }
        report_win32_error(L"EnumDisplayMonitors");
        return false;
    }
    if(ctx.failed) {
        report_win32_error(L"GetMonitorInfoW", ctx.error_code);
        return false;
    }
    if(ctx.rects.empty()) {
        show_error_dialog(L"No display monitors were detected by EnumDisplayMonitors.");
        return false;
    }
    out_rects = std::move(ctx.rects);
    return true;
}

[[nodiscard]] auto select_monitor_index(std::size_t monitor_count, int configured_monitor) noexcept -> std::size_t {
    if(configured_monitor >= 1 && std::cmp_less_equal(configured_monitor, monitor_count)) {
        return static_cast<std::size_t>(configured_monitor - 1);
    }
    return (monitor_count > 1) ? 1U : 0U;
}

[[nodiscard]] auto compute_window_placement(const RECT &mon_rect, bool is_windowed) noexcept -> window_placement {
    const int mon_width = mon_rect.right - mon_rect.left;
    const int mon_height = mon_rect.bottom - mon_rect.top;

    window_placement placement{};
    if(is_windowed) {
        placement.width = (mon_width * 3) / 4;
        placement.height = (mon_height * 3) / 4;
        placement.x = mon_rect.left + ((mon_width - placement.width) / 2);
        placement.y = mon_rect.top + ((mon_height - placement.height) / 2);
        placement.style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        placement.ex_style = WS_EX_TOPMOST;
    } else {
        placement.width = mon_width;
        placement.height = mon_height;
        placement.x = mon_rect.left;
        placement.y = mon_rect.top;
        placement.style = WS_POPUP | WS_VISIBLE;
        placement.ex_style = WS_EX_TOPMOST | WS_EX_TOOLWINDOW;
    }
    return placement;
}
