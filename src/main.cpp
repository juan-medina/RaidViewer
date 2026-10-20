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
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace {

// --- Hotkey & Input Configuration ---
enum class action_id : std::uint8_t {
    prev = 0,
    next = 1,
    selection = 2,
    close = 3,
};

struct key_combo {
    UINT mod = 0;
    UINT vk = 0;
};

struct global_hotkey_binding {
    int id = 0;
    action_id action = action_id::prev;
    key_combo combo{};
};

struct window_key_binding {
    action_id action = action_id::prev;
    key_combo combo{};
};

struct named_vk {
    std::wstring_view name;
    UINT vk;
};

constexpr std::array<named_vk, 33> k_named_vks = {{
    {.name = L"UP", .vk = VK_UP},
    {.name = L"DOWN", .vk = VK_DOWN},
    {.name = L"LEFT", .vk = VK_LEFT},
    {.name = L"RIGHT", .vk = VK_RIGHT},
    {.name = L"ENTER", .vk = VK_RETURN},
    {.name = L"RETURN", .vk = VK_RETURN},
    {.name = L"SPACE", .vk = VK_SPACE},
    {.name = L"SPACEBAR", .vk = VK_SPACE},
    {.name = L"ESC", .vk = VK_ESCAPE},
    {.name = L"ESCAPE", .vk = VK_ESCAPE},
    {.name = L"TAB", .vk = VK_TAB},
    {.name = L"BACKSPACE", .vk = VK_BACK},
    {.name = L"BACK", .vk = VK_BACK},
    {.name = L"INSERT", .vk = VK_INSERT},
    {.name = L"INS", .vk = VK_INSERT},
    {.name = L"DELETE", .vk = VK_DELETE},
    {.name = L"DEL", .vk = VK_DELETE},
    {.name = L"HOME", .vk = VK_HOME},
    {.name = L"END", .vk = VK_END},
    {.name = L"PAGEUP", .vk = VK_PRIOR},
    {.name = L"PGUP", .vk = VK_PRIOR},
    {.name = L"PAGEDOWN", .vk = VK_NEXT},
    {.name = L"PGDN", .vk = VK_NEXT},
    {.name = L"PLUS", .vk = VK_OEM_PLUS},
    {.name = L"MINUS", .vk = VK_OEM_MINUS},
    {.name = L"COMMA", .vk = VK_OEM_COMMA},
    {.name = L"PERIOD", .vk = VK_OEM_PERIOD},
    {.name = L"MULTIPLY", .vk = VK_MULTIPLY},
    {.name = L"ADD", .vk = VK_ADD},
    {.name = L"SUBTRACT", .vk = VK_SUBTRACT},
    {.name = L"DECIMAL", .vk = VK_DECIMAL},
    {.name = L"DIVIDE", .vk = VK_DIVIDE},
    {.name = L"PAUSE", .vk = VK_PAUSE},
}};

// --- Constants ---
constexpr auto k_window_class_name = L"RaidViewerWindow";
constexpr auto k_window_title = L"RaidViewer";
constexpr auto k_images_dir_name = L"images";
constexpr auto k_config_file_name = L"RaidViewer.ini";

constexpr std::string_view k_default_ini_content =
    "; RaidViewer Configuration\r\n"
    "\r\n"
    "; Monitor number (1 = primary, 2 = first secondary, etc.).\r\n"
    "; If invalid or disconnected, falls back to the first secondary monitor (or primary if only one exists).\r\n"
    "; Mode: Fullscreen (borderless fullscreen) or Window (resizable window, defaults to 3/4 of monitor size).\r\n"
    "[Display]\r\n"
    "Monitor = 2\r\n"
    "Mode    = Fullscreen\r\n"
    "\r\n"
    "; Each action accepts one or more key combinations separated by commas.\r\n"
    "; Modifiers: Control (or Ctrl), Alt, Shift, Win (combine with '+', e.g. Control+Alt+Q)\r\n"
    "; Keys: F1-F24, A-Z, 0-9, NumPad0-NumPad9, Up, Down, Left, Right,\r\n"
    ";       PageUp, PageDown, Home, End, Insert, Delete, Space, Enter, Esc, Tab, Backspace\r\n"
    ";\r\n"
    "; Global hotkeys work at all times, even while playing your game.\r\n"
    "[GlobalHotkeys]\r\n"
    "Previous  = F13\r\n"
    "Next      = F14\r\n"
    "Selection = F15\r\n"
    "Close     = F16, Control+Alt+Q\r\n"
    "\r\n"
    "; Window keys only work when the RaidViewer window itself is focused.\r\n"
    "[WindowKeys]\r\n"
    "Previous  = Up, Left\r\n"
    "Next      = Down, Right\r\n"
    "Selection = Enter, Space\r\n"
    "Close     = Esc\r\n";

// Custom deleter for HBITMAP
using unique_bitmap = std::unique_ptr<std::remove_pointer_t<HBITMAP>, decltype([](HBITMAP bmp) -> void {
                                          if(bmp != nullptr) {
                                              DeleteObject(bmp);
                                          }
                                      })>;

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

enum class view_mode : std::uint8_t {
    folder_list,
    image_viewer,
};

enum class display_mode : std::uint8_t {
    fullscreen = 0,
    window = 1,
};

struct strat_folder {
    std::wstring name;
    std::wstring path;
    std::vector<unique_bitmap> slides;
    bool loaded = false;
};

void show_error_dialog(const wchar_t *message) noexcept {
    MessageBoxW(nullptr, message, L"RaidViewer Fatal Error", MB_OK | MB_ICONERROR | MB_TOPMOST);
}

void report_win32_error(std::wstring_view action, DWORD error_code = GetLastError()) {
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

// Get the directory where RaidViewer.exe lives
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

// Loads PNG/BMP/JPG, scales to screen size conserving aspect ratio, fills
// letterbox with black
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

// --- Application State ---
struct app_state {
    view_mode mode = view_mode::folder_list;
    display_mode win_mode = display_mode::fullscreen;
    bool in_size_move = false;
    bool is_resizing = false;
    std::vector<strat_folder> folders;
    int selected_folder_index = 0;
    int current_slide_index = 0;
    int scroll_offset = 0;
    int mon_width = 0;
    int mon_height = 0;
    std::wstring images_dir;
    std::vector<global_hotkey_binding> global_hotkeys;
    std::vector<window_key_binding> window_keys;
    std::wstring footer_legend;
    std::wstring close_keys_label;
};

struct action_config_spec {
    action_id action;
    const wchar_t *ini_key;
    const wchar_t *default_global;
    const wchar_t *default_window;
    std::wstring_view legend_suffix;
};

constexpr std::array<action_config_spec, 4> k_action_specs = {{
    {.action = action_id::prev,
     .ini_key = L"Previous",
     .default_global = L"F13",
     .default_window = L"Up, Left",
     .legend_suffix = L": Previous Strat"},
    {.action = action_id::next,
     .ini_key = L"Next",
     .default_global = L"F14",
     .default_window = L"Down, Right",
     .legend_suffix = L": Next Strat"},
    {.action = action_id::selection,
     .ini_key = L"Selection",
     .default_global = L"F15",
     .default_window = L"Enter, Space",
     .legend_suffix = L": Open Strat"},
    {.action = action_id::close,
     .ini_key = L"Close",
     .default_global = L"F16, Control+Alt+Q",
     .default_window = L"Esc",
     .legend_suffix = L": Back / Close"},
}};

[[nodiscard]] auto trim_whitespace(std::wstring_view text) noexcept -> std::wstring_view {
    const auto first = text.find_first_not_of(L" \t\r\n");
    if(first == std::wstring_view::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(L" \t\r\n");
    return text.substr(first, last - first + 1);
}

[[nodiscard]] auto to_upper_ascii(std::wstring_view text) -> std::wstring {
    std::wstring result(text);
    for(auto &chr: result) {
        chr = static_cast<wchar_t>(std::towupper(chr));
    }
    return result;
}

[[nodiscard]] auto parse_modifier_token(std::wstring_view upper_token, UINT &out_mod) noexcept -> bool {
    if(upper_token == L"CTRL" || upper_token == L"CONTROL") {
        out_mod |= MOD_CONTROL;
        return true;
    }
    if(upper_token == L"ALT") {
        out_mod |= MOD_ALT;
        return true;
    }
    if(upper_token == L"SHIFT") {
        out_mod |= MOD_SHIFT;
        return true;
    }
    if(upper_token == L"WIN" || upper_token == L"WINDOWS" || upper_token == L"SUPER") {
        out_mod |= MOD_WIN;
        return true;
    }
    return false;
}

[[nodiscard]] auto parse_function_key_vk(std::wstring_view upper_token, UINT &out_vk) noexcept -> bool {
    if(upper_token.size() < 2 || upper_token.size() > 3 || upper_token.front() != L'F') {
        return false;
    }
    int fn_num = 0;
    for(const wchar_t chr: upper_token.substr(1)) {
        if(chr < L'0' || chr > L'9') {
            return false;
        }
        fn_num = (fn_num * 10) + static_cast<int>(chr - L'0');
    }
    if(fn_num < 1 || fn_num > 24) {
        return false;
    }
    out_vk = static_cast<UINT>(VK_F1 + (fn_num - 1));
    return true;
}

[[nodiscard]] auto parse_numpad_vk(std::wstring_view upper_token, UINT &out_vk) noexcept -> bool {
    std::wstring_view suffix;
    if(upper_token.starts_with(L"NUMPAD")) {
        suffix = upper_token.substr(6);
    } else if(upper_token.starts_with(L"NUM")) {
        suffix = upper_token.substr(3);
    } else {
        return false;
    }
    if(suffix.size() == 1 && suffix.front() >= L'0' && suffix.front() <= L'9') {
        out_vk = static_cast<UINT>(VK_NUMPAD0 + (suffix.front() - L'0'));
        return true;
    }
    return false;
}

[[nodiscard]] auto parse_vk_token(std::wstring_view upper_token, UINT &out_vk) noexcept -> bool {
    if(upper_token.empty()) {
        return false;
    }
    if(upper_token.size() == 1) {
        const wchar_t chr = upper_token.front();
        if((chr >= L'A' && chr <= L'Z') || (chr >= L'0' && chr <= L'9')) {
            out_vk = static_cast<UINT>(chr);
            return true;
        }
    }
    if(parse_function_key_vk(upper_token, out_vk) || parse_numpad_vk(upper_token, out_vk)) {
        return true;
    }
    for(const auto &entry: k_named_vks) {
        if(entry.name == upper_token) {
            out_vk = entry.vk;
            return true;
        }
    }
    return false;
}

[[nodiscard]] auto parse_key_combo(std::wstring_view combo_text,
                                   std::wstring_view section,
                                   std::wstring_view key_name,
                                   key_combo &out_combo) -> bool {
    key_combo parsed{};
    std::size_t start = 0;
    while(start <= combo_text.size()) {
        const auto plus_pos = combo_text.find(L'+', start);
        const bool is_last = (plus_pos == std::wstring_view::npos);
        const auto raw_part = is_last ? combo_text.substr(start) : combo_text.substr(start, plus_pos - start);
        const auto token = to_upper_ascii(trim_whitespace(raw_part));

        if(is_last) {
            if(!parse_vk_token(token, parsed.vk)) {
                const std::wstring msg = L"Invalid key '" + std::wstring(trim_whitespace(raw_part)) + L"' in ["
                                         + std::wstring(section) + L"] " + std::wstring(key_name) + L" ("
                                         + std::wstring(k_config_file_name) + L").";
                show_error_dialog(msg.c_str());
                return false;
            }
            break;
        }

        if(!parse_modifier_token(token, parsed.mod)) {
            const std::wstring msg = L"Invalid modifier '" + std::wstring(trim_whitespace(raw_part)) + L"' in ["
                                     + std::wstring(section) + L"] " + std::wstring(key_name) + L" ("
                                     + std::wstring(k_config_file_name) + L").";
            show_error_dialog(msg.c_str());
            return false;
        }
        start = plus_pos + 1;
    }

    out_combo = parsed;
    return true;
}

[[nodiscard]] auto parse_combo_list(std::wstring_view raw_list,
                                    std::wstring_view section,
                                    std::wstring_view key_name,
                                    std::vector<key_combo> &out_combos,
                                    std::vector<std::wstring> &out_labels) -> bool {
    std::size_t start = 0;
    while(start <= raw_list.size()) {
        const auto comma_pos = raw_list.find(L',', start);
        const auto part =
            (comma_pos == std::wstring_view::npos) ? raw_list.substr(start) : raw_list.substr(start, comma_pos - start);
        const auto trimmed = trim_whitespace(part);
        if(!trimmed.empty()) {
            key_combo combo{};
            if(!parse_key_combo(trimmed, section, key_name, combo)) {
                return false;
            }
            out_combos.push_back(combo);
            out_labels.emplace_back(trimmed);
        }
        if(comma_pos == std::wstring_view::npos) {
            break;
        }
        start = comma_pos + 1;
    }
    return true;
}

[[nodiscard]] auto ensure_default_config_file(const std::wstring &config_path) -> bool {
    if(GetFileAttributesW(config_path.c_str()) != INVALID_FILE_ATTRIBUTES) {
        return true;
    }
    const DWORD attr_err = GetLastError();
    if(attr_err != ERROR_FILE_NOT_FOUND && attr_err != ERROR_PATH_NOT_FOUND) {
        report_win32_error(L"GetFileAttributesW for '" + config_path + L"'", attr_err);
        return false;
    }

    const file_handle file(
        CreateFileW(config_path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr));
    if(!file.valid()) {
        report_win32_error(L"CreateFileW for '" + config_path + L"'");
        return false;
    }

    DWORD bytes_written = 0;
    const auto bytes_to_write = static_cast<DWORD>(k_default_ini_content.size());
    if(WriteFile(file.get(), k_default_ini_content.data(), bytes_to_write, &bytes_written, nullptr) == FALSE
       || bytes_written != bytes_to_write) {
        report_win32_error(L"WriteFile for '" + config_path + L"'");
        return false;
    }
    return true;
}

[[nodiscard]] auto
read_ini_value(const wchar_t *section, const wchar_t *key, const wchar_t *default_val, const std::wstring &config_path)
    -> std::wstring {
    std::array<wchar_t, 256> buffer{};
    const DWORD len = GetPrivateProfileStringW(
        section, key, default_val, buffer.data(), static_cast<DWORD>(buffer.size()), config_path.c_str());
    return {buffer.data(), len};
}

[[nodiscard]] auto join_labels(const std::vector<std::wstring> &labels) -> std::wstring {
    std::wstring joined;
    for(std::size_t idx = 0; idx < labels.size(); ++idx) {
        if(idx > 0) {
            joined += L" / ";
        }
        joined += labels[idx];
    }
    return joined;
}

[[nodiscard]] auto parse_positive_int(std::wstring_view text, int &out_value) noexcept -> bool {
    const auto trimmed = trim_whitespace(text);
    if(trimmed.empty()) {
        return false;
    }
    int value = 0;
    for(const wchar_t chr: trimmed) {
        if(chr < L'0' || chr > L'9') {
            return false;
        }
        const int digit = static_cast<int>(chr - L'0');
        if(value > (std::numeric_limits<int>::max() - digit) / 10) {
            return false;
        }
        value = (value * 10) + digit;
    }
    if(value < 1) {
        return false;
    }
    out_value = value;
    return true;
}

[[nodiscard]] auto select_monitor_index(std::size_t monitor_count, int configured_monitor) noexcept -> std::size_t {
    if(configured_monitor >= 1 && std::cmp_less_equal(configured_monitor, monitor_count)) {
        return static_cast<std::size_t>(configured_monitor - 1);
    }
    return (monitor_count > 1) ? 1U : 0U;
}

[[nodiscard]] auto parse_display_mode(std::wstring_view raw_mode, display_mode &out_mode) -> bool {
    const auto token = to_upper_ascii(trim_whitespace(raw_mode));
    if(token.empty() || token == L"FULLSCREEN") {
        out_mode = display_mode::fullscreen;
        return true;
    }
    if(token == L"WINDOW" || token == L"WINDOWED") {
        out_mode = display_mode::window;
        return true;
    }
    const std::wstring msg = L"Invalid display mode '" + std::wstring(trim_whitespace(raw_mode))
                             + L"' in [Display] Mode (" + std::wstring(k_config_file_name)
                             + L"). Expected Fullscreen or Window.";
    show_error_dialog(msg.c_str());
    return false;
}

[[nodiscard]] auto load_config(const std::wstring &config_path, app_state &app, int &out_monitor_number) -> bool {
    if(!ensure_default_config_file(config_path)) {
        return false;
    }

    const auto monitor_str = read_ini_value(L"Display", L"Monitor", L"2", config_path);
    int parsed_monitor = 0;
    out_monitor_number = parse_positive_int(monitor_str, parsed_monitor) ? parsed_monitor : 0;

    const auto mode_str = read_ini_value(L"Display", L"Mode", L"Fullscreen", config_path);
    if(!parse_display_mode(mode_str, app.win_mode)) {
        return false;
    }

    app.global_hotkeys.clear();
    app.window_keys.clear();
    app.footer_legend.clear();
    app.close_keys_label.clear();

    int next_hotkey_id = 1;
    for(const auto &spec: k_action_specs) {
        std::vector<std::wstring> action_labels;

        const auto global_str = read_ini_value(L"GlobalHotkeys", spec.ini_key, spec.default_global, config_path);
        std::vector<key_combo> global_combos;
        if(!parse_combo_list(global_str, L"GlobalHotkeys", spec.ini_key, global_combos, action_labels)) {
            return false;
        }
        for(const auto &combo: global_combos) {
            app.global_hotkeys.push_back(global_hotkey_binding{
                .id = next_hotkey_id++,
                .action = spec.action,
                .combo = combo,
            });
        }

        const auto window_str = read_ini_value(L"WindowKeys", spec.ini_key, spec.default_window, config_path);
        std::vector<key_combo> window_combos;
        if(!parse_combo_list(window_str, L"WindowKeys", spec.ini_key, window_combos, action_labels)) {
            return false;
        }
        for(const auto &combo: window_combos) {
            app.window_keys.push_back(window_key_binding{
                .action = spec.action,
                .combo = combo,
            });
        }

        const auto keys_summary = action_labels.empty() ? std::wstring(L"Unbound") : join_labels(action_labels);
        if(!app.footer_legend.empty()) {
            app.footer_legend += L"    |    ";
        }
        app.footer_legend += keys_summary;
        app.footer_legend += spec.legend_suffix;

        if(spec.action == action_id::close) {
            app.close_keys_label = keys_summary;
        }
    }

    return true;
}

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

void handle_close_action(HWND hwnd, app_state &app) noexcept {
    if(app.mode == view_mode::image_viewer) {
        app.mode = view_mode::folder_list;
        InvalidateRect(hwnd, nullptr, FALSE);
        UpdateWindow(hwnd);
    } else {
        DestroyWindow(hwnd);
    }
}

[[nodiscard]] auto discover_folders(app_state &app) -> bool {
    app.folders.clear();
    bool has_loose_images = false;

    WIN32_FIND_DATAW find_data{};
    const auto search_pattern = join_path(app.images_dir, L"*");
    const find_file_handle find_handle(FindFirstFileW(search_pattern.c_str(), &find_data));
    if(!find_handle.valid()) {
        const DWORD err_code = GetLastError();
        if(err_code != ERROR_FILE_NOT_FOUND) {
            report_win32_error(L"FindFirstFileW in '" + app.images_dir + L"'", err_code);
            return false;
        }
    } else {
        while(true) {
            const std::wstring_view entry_name(static_cast<const wchar_t *>(find_data.cFileName));
            if(!is_dot_entry(entry_name)) {
                const bool is_dir = (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                if(is_dir) {
                    app.folders.push_back(strat_folder{
                        .name = std::wstring(entry_name),
                        .path = join_path(app.images_dir, entry_name),
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
            report_win32_error(L"FindNextFileW in '" + app.images_dir + L"'", iter_err);
            return false;
        }
    }

    std::ranges::sort(app.folders, [](const auto &lhs, const auto &rhs) -> bool {
        const auto cmp = StrCmpIW(lhs.name.c_str(), rhs.name.c_str());
        if(cmp != 0) {
            return cmp < 0;
        }
        return lhs.name < rhs.name;
    });

    if(app.folders.empty() && has_loose_images) {
        app.folders.push_back(strat_folder{
            .name = L"Default (images/)",
            .path = app.images_dir,
            .slides = {},
            .loaded = false,
        });
    }

    if(std::cmp_greater_equal(app.selected_folder_index, app.folders.size())) {
        app.selected_folder_index = 0;
    }
    app.scroll_offset = 0;
    return true;
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
    if(app.folders.empty()) {
        SelectObject(hdc_mem, font_item);
        SetTextColor(hdc_mem, RGB(180, 180, 180));
        RECT empty_rect{80, 200, app.mon_width - 80, 320};
        const std::wstring empty_msg =
            L"No folders found inside 'images'.\n"
            L"Create strategy subfolders (e.g. 'M12S - Phase 1') inside the 'images' folder.\n("
            + app.close_keys_label + L" to close)";
        DrawTextW(hdc_mem, empty_msg.c_str(), -1, &empty_rect, DT_LEFT | DT_NOPREFIX);
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

void invalidate_loaded_slides(app_state &app) noexcept {
    for(auto &folder: app.folders) {
        folder.slides.clear();
        folder.loaded = false;
    }
}

[[nodiscard]] auto apply_client_resize(HWND hwnd, app_state &app, int new_w, int new_h) -> bool {
    if(new_w <= 0 || new_h <= 0) {
        return true;
    }
    app.mon_width = new_w;
    app.mon_height = new_h;
    invalidate_loaded_slides(app);
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

[[nodiscard]] auto execute_action(HWND hwnd, app_state &app, action_id action) -> bool {
    switch(action) {
    case action_id::close:
        handle_close_action(hwnd, app);
        return true;
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

[[nodiscard]] auto handle_hotkey(HWND hwnd, app_state &app, WPARAM w_param) -> bool {
    const auto hotkey_id = static_cast<int>(w_param);
    for(const auto &binding: app.global_hotkeys) {
        if(binding.id == hotkey_id) {
            return execute_action(hwnd, app, binding.action);
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

[[nodiscard]] auto get_app_state(HWND hwnd) noexcept -> app_state * {
    auto *app = std::bit_cast<app_state *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if(app == nullptr) {
        show_error_dialog(L"Window userdata (app_state*) is null.");
    }
    return app;
}

auto CALLBACK wnd_proc(HWND hwnd, UINT msg, WPARAM w_param, LPARAM l_param) noexcept -> LRESULT {
    if(msg == WM_NCCREATE) {
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

    switch(msg) {
    case WM_HOTKEY: {
        auto *app = get_app_state(hwnd);
        if(app == nullptr || !handle_hotkey(hwnd, *app, w_param)) {
            PostQuitMessage(EXIT_FAILURE);
        }
        return 0;
    }

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN: {
        auto *app = get_app_state(hwnd);
        if(app == nullptr) {
            PostQuitMessage(EXIT_FAILURE);
            return 0;
        }
        action_id action = action_id::prev;
        if(match_window_key(*app, w_param, action)) {
            if(!execute_action(hwnd, *app, action)) {
                PostQuitMessage(EXIT_FAILURE);
            }
            return 0;
        }
        break;
    }

    case WM_GETMINMAXINFO: {
        auto *mmi = std::bit_cast<MINMAXINFO *>(l_param);
        if(mmi != nullptr) {
            mmi->ptMinTrackSize.x = 480;
            mmi->ptMinTrackSize.y = 320;
        }
        return 0;
    }

    case WM_ENTERSIZEMOVE: {
        auto *app = get_app_state(hwnd);
        if(app == nullptr) {
            PostQuitMessage(EXIT_FAILURE);
            return 0;
        }
        app->in_size_move = true;
        return 0;
    }

    case WM_SIZE: {
        if(w_param == SIZE_MINIMIZED) {
            return 0;
        }
        auto *app = get_app_state(hwnd);
        if(app == nullptr) {
            PostQuitMessage(EXIT_FAILURE);
            return 0;
        }
        const int new_w = static_cast<int>(LOWORD(l_param));
        const int new_h = static_cast<int>(HIWORD(l_param));
        if(new_w <= 0 || new_h <= 0) {
            return 0;
        }
        if(app->in_size_move) {
            if(new_w != app->mon_width || new_h != app->mon_height || app->is_resizing) {
                app->is_resizing = true;
                app->mon_width = new_w;
                app->mon_height = new_h;
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        if(new_w != app->mon_width || new_h != app->mon_height) {
            if(!apply_client_resize(hwnd, *app, new_w, new_h)) {
                PostQuitMessage(EXIT_FAILURE);
            }
        }
        return 0;
    }

    case WM_EXITSIZEMOVE: {
        auto *app = get_app_state(hwnd);
        if(app == nullptr) {
            PostQuitMessage(EXIT_FAILURE);
            return 0;
        }
        app->in_size_move = false;
        if(app->is_resizing) {
            app->is_resizing = false;
            if(!apply_client_resize(hwnd, *app, app->mon_width, app->mon_height)) {
                PostQuitMessage(EXIT_FAILURE);
            }
        }
        return 0;
    }

    case WM_ERASEBKGND:
        return 1; // Double-buffered / BitBlt prevents flicker

    case WM_PAINT: {
        auto *app = get_app_state(hwnd);
        if(app == nullptr || !handle_paint(hwnd, *app)) {
            PostQuitMessage(EXIT_FAILURE);
        }
        return 0;
    }

    case WM_DESTROY:
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
        PostQuitMessage(0);
        return 0;

    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, w_param, l_param);
}

class gdiplus_session {
public:
    gdiplus_session() {
        const Gdiplus::GdiplusStartupInput gdi_input;
        status_ = Gdiplus::GdiplusStartup(&token_, &gdi_input, nullptr);
    }

    ~gdiplus_session() {
        if(status_ == Gdiplus::Ok && token_ != 0) {
            Gdiplus::GdiplusShutdown(token_);
        }
    }

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

auto run_app() -> int {
    auto *const h_instance = GetModuleHandleW(nullptr);
    if(h_instance == nullptr) {
        report_win32_error(L"GetModuleHandleW(nullptr)");
        return EXIT_FAILURE;
    }

    // 1. Enable Per-Monitor V2 DPI Awareness for pixel-perfect scaling
    if(SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2) == FALSE) {
        report_win32_error(L"SetProcessDpiAwarenessContext");
        return EXIT_FAILURE;
    }

    // 2. Ensure the "images" directory exists right next to the .exe
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

    // 3. Start GDI+ (RAII ensures shutdown happens after app_state is destroyed)
    const gdiplus_session gdi_session;
    if(gdi_session.status() != Gdiplus::Ok) {
        const std::wstring msg =
            L"GdiplusStartup failed with status " + std::to_wstring(static_cast<int>(gdi_session.status())) + L".";
        show_error_dialog(msg.c_str());
        return EXIT_FAILURE;
    }

    app_state app{
        .mode = view_mode::folder_list,
        .win_mode = display_mode::fullscreen,
        .in_size_move = false,
        .is_resizing = false,
        .folders = {},
        .selected_folder_index = 0,
        .current_slide_index = 0,
        .scroll_offset = 0,
        .mon_width = 0,
        .mon_height = 0,
        .images_dir = images_dir,
        .global_hotkeys = {},
        .window_keys = {},
        .footer_legend = {},
        .close_keys_label = {},
    };

    // 4. Load or generate RaidViewer.ini next to the executable
    const auto config_path = join_path(exe_dir, k_config_file_name);
    int configured_monitor = 2;
    if(!load_config(config_path, app, configured_monitor)) {
        return EXIT_FAILURE;
    }

    // 5. Query display monitors (use configured monitor if valid, else 2nd monitor or primary)
    std::vector<RECT> monitors;
    if(!get_all_monitor_rects(monitors)) {
        return EXIT_FAILURE;
    }
    const std::size_t target_index = select_monitor_index(monitors.size(), configured_monitor);
    const auto mon_rect = monitors[target_index];
    const int mon_width = mon_rect.right - mon_rect.left;
    const int mon_height = mon_rect.bottom - mon_rect.top;
    if(mon_width <= 0 || mon_height <= 0) {
        const std::wstring msg = L"Invalid monitor dimensions detected: " + std::to_wstring(mon_width) + L"x"
                                 + std::to_wstring(mon_height) + L".";
        show_error_dialog(msg.c_str());
        return EXIT_FAILURE;
    }

    const bool is_windowed = (app.win_mode == display_mode::window);
    const DWORD ex_style = is_windowed ? WS_EX_TOPMOST : (WS_EX_TOPMOST | WS_EX_TOOLWINDOW);
    const DWORD win_style = is_windowed ? (WS_OVERLAPPEDWINDOW | WS_VISIBLE) : (WS_POPUP | WS_VISIBLE);

    int win_x = mon_rect.left;
    int win_y = mon_rect.top;
    int win_w = mon_width;
    int win_h = mon_height;

    if(is_windowed) {
        win_w = (mon_width * 3) / 4;
        win_h = (mon_height * 3) / 4;
        win_x = mon_rect.left + ((mon_width - win_w) / 2);
        win_y = mon_rect.top + ((mon_height - win_h) / 2);
    }

    app.mon_width = win_w;
    app.mon_height = win_h;

    // 6. Discover strategy folders inside images/
    if(!discover_folders(app)) {
        return EXIT_FAILURE;
    }

    // 7. Create window (borderless fullscreen or resizable 3/4 window)
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

    auto *hwnd = CreateWindowExW(ex_style,
                                 wnd_class.lpszClassName,
                                 k_window_title,
                                 win_style,
                                 win_x,
                                 win_y,
                                 win_w,
                                 win_h,
                                 nullptr,
                                 nullptr,
                                 h_instance,
                                 &app);

    if(hwnd == nullptr) {
        report_win32_error(L"CreateWindowExW");
        return EXIT_FAILURE;
    }

    // 8. Register Global Hotkeys & run event loop
    if(!register_hotkeys(hwnd, app.global_hotkeys)) {
        return EXIT_FAILURE;
    }
    return run_message_loop(hwnd, app.global_hotkeys);
}

} // namespace

auto main() -> int {
    return run_app();
}
