// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#pragma once

#include "RaidViewer/app_state.hpp"

// clang-format off
#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>
// clang-format on

// --- GDI+ RAII Session Lifecycle ---
class gdiplus_session {
public:
    gdiplus_session();
    ~gdiplus_session();

    gdiplus_session(const gdiplus_session &) = delete;
    auto operator=(const gdiplus_session &) -> gdiplus_session & = delete;
    gdiplus_session(gdiplus_session &&) = delete;
    auto operator=(gdiplus_session &&) -> gdiplus_session & = delete;

    [[nodiscard]] auto status() const noexcept -> Gdiplus::Status {
        return status_;
    }

private:
    ULONG_PTR token_ = 0;
    Gdiplus::Status status_ = Gdiplus::GenericError;
};

// --- GDI Drawing Operations ---
[[nodiscard]] auto handle_paint(HWND hwnd, const app_state &app) -> bool;
