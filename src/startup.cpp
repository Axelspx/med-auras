#include "startup.h"

#include <windows.h>

#include <array>
#include <string>

bool enable_startup() {
    std::array<wchar_t, 32'768> executable{};
    const DWORD length = GetModuleFileName(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length == 0 || length == executable.size()) return false;

    const std::wstring command = L"\"" + std::wstring{executable.data(), length} + L"\"";
    HKEY run_key{};
    if (RegCreateKeyEx(
            HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, nullptr, 0, KEY_SET_VALUE,
            nullptr, &run_key, nullptr) != ERROR_SUCCESS) {
        return false;
    }

    const LONG result = RegSetValueEx(
        run_key, L"MedAuras", 0, REG_SZ, reinterpret_cast<const BYTE*>(command.c_str()),
        static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(run_key);
    return result == ERROR_SUCCESS;
}
