#include "startup.h"

#include <windows.h>

#include <array>
#include <optional>
#include <string>

namespace {
constexpr wchar_t run_key_path[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t run_value_name[] = L"MedAuras";

std::optional<std::wstring> startup_command() {
    std::array<wchar_t, 32'768> executable{};
    const DWORD length = GetModuleFileName(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length == 0 || length == executable.size()) return std::nullopt;

    return L"\"" + std::wstring{executable.data(), length} + L"\"";
}
}

bool enable_startup() {
    const auto command = startup_command();
    if (!command) return false;

    HKEY run_key{};
    if (RegCreateKeyEx(
            HKEY_CURRENT_USER, run_key_path, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &run_key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    const LONG result = RegSetValueEx(
        run_key, run_value_name, 0, REG_SZ, reinterpret_cast<const BYTE*>(command->c_str()),
        static_cast<DWORD>((command->size() + 1) * sizeof(wchar_t)));
    RegCloseKey(run_key);
    return result == ERROR_SUCCESS;
}

bool disable_startup() {
    const LONG result = RegDeleteKeyValue(HKEY_CURRENT_USER, run_key_path, run_value_name);
    return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
}

bool is_startup_enabled() {
    const auto expected = startup_command();
    if (!expected) return false;

    std::array<wchar_t, 32'768> command{};
    DWORD size = static_cast<DWORD>(command.size() * sizeof(wchar_t));
    return RegGetValue(
               HKEY_CURRENT_USER, run_key_path, run_value_name, RRF_RT_REG_SZ, nullptr, command.data(), &size) ==
               ERROR_SUCCESS &&
           *expected == command.data();
}
