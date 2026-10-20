// SPDX-FileCopyrightText: 2026 Juan Medina
// SPDX-License-Identifier: MIT

#include "RaidViewer/config.hpp"

#include "RaidViewer/platform.hpp"

#include <array>
#include <cstddef>
#include <cwctype>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

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

} // namespace

[[nodiscard]] auto load_config(const std::wstring &config_path, app_config &out_config) -> bool {
    if(!ensure_default_config_file(config_path)) {
        return false;
    }

    const auto monitor_str = read_ini_value(L"Display", L"Monitor", L"2", config_path);
    int parsed_monitor = 0;
    out_config.monitor_number = parse_positive_int(monitor_str, parsed_monitor) ? parsed_monitor : 0;

    const auto mode_str = read_ini_value(L"Display", L"Mode", L"Fullscreen", config_path);
    if(!parse_display_mode(mode_str, out_config.win_mode)) {
        return false;
    }

    out_config.global_hotkeys.clear();
    out_config.window_keys.clear();
    out_config.footer_legend.clear();
    out_config.close_keys_label.clear();

    int next_hotkey_id = 1;
    for(const auto &spec: k_action_specs) {
        std::vector<std::wstring> action_labels;

        const auto global_str = read_ini_value(L"GlobalHotkeys", spec.ini_key, spec.default_global, config_path);
        std::vector<key_combo> global_combos;
        if(!parse_combo_list(global_str, L"GlobalHotkeys", spec.ini_key, global_combos, action_labels)) {
            return false;
        }
        for(const auto &combo: global_combos) {
            out_config.global_hotkeys.push_back(global_hotkey_binding{
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
            out_config.window_keys.push_back(window_key_binding{
                .action = spec.action,
                .combo = combo,
            });
        }

        const auto keys_summary = action_labels.empty() ? std::wstring(L"Unbound") : join_labels(action_labels);
        if(!out_config.footer_legend.empty()) {
            out_config.footer_legend += L"    |    ";
        }
        out_config.footer_legend += keys_summary;
        out_config.footer_legend += spec.legend_suffix;

        if(spec.action == action_id::close) {
            out_config.close_keys_label = keys_summary;
        }
    }

    return true;
}
