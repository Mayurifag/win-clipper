#include "startup.h"

#include <windows.h>

namespace clipper
{
namespace
{

constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"WinClipper";

} // namespace

bool SetStartWithWindows(bool enabled, const std::wstring& executable_path)
{
    HKEY key = nullptr;
    const LONG open_result =
        RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, REG_OPTION_NON_VOLATILE,
                        KEY_SET_VALUE, nullptr, &key, nullptr);
    if (open_result != ERROR_SUCCESS)
    {
        return false;
    }

    LONG result = ERROR_SUCCESS;
    if (enabled)
    {
        const std::wstring command = L"\"" + executable_path + L"\" --agent";
        result = RegSetValueExW(key, kValueName, 0, REG_SZ,
                                reinterpret_cast<const BYTE*>(command.c_str()),
                                static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t)));
    }
    else
    {
        result = RegDeleteValueW(key, kValueName);
        if (result == ERROR_FILE_NOT_FOUND)
        {
            result = ERROR_SUCCESS;
        }
    }

    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}

} // namespace clipper
