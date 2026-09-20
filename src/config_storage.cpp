#include "config.h"

#include <windows.h>

#include <algorithm>
#include <sstream>

namespace clipper
{
namespace
{

constexpr wchar_t kConfigDirectory[] = L"WinClipper";
constexpr wchar_t kConfigFile[] = L"config.txt";

std::string ToUtf8(const std::wstring& value)
{
    if (value.empty())
    {
        return {};
    }

    const int size =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                            static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0)
    {
        return {};
    }

    std::string result(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        &result[0], size, nullptr, nullptr);
    return result;
}

std::wstring FromUtf8(const std::string& value)
{
    if (value.empty())
    {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0)
    {
        return {};
    }

    std::wstring result(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        &result[0], size);
    return result;
}

std::wstring Escape(const std::wstring& value)
{
    std::wstring result;
    result.reserve(value.size());
    for (const wchar_t character : value)
    {
        switch (character)
        {
        case L'\\':
            result += L"\\\\";
            break;
        case L'\t':
            result += L"\\t";
            break;
        case L'\r':
            result += L"\\r";
            break;
        case L'\n':
            result += L"\\n";
            break;
        default:
            result += character;
            break;
        }
    }
    return result;
}

std::wstring Unescape(const std::wstring& value)
{
    std::wstring result;
    result.reserve(value.size());
    bool escaped = false;
    for (const wchar_t character : value)
    {
        if (!escaped)
        {
            if (character == L'\\')
            {
                escaped = true;
            }
            else
            {
                result += character;
            }
            continue;
        }

        switch (character)
        {
        case L'\\':
            result += L'\\';
            break;
        case L't':
            result += L'\t';
            break;
        case L'r':
            result += L'\r';
            break;
        case L'n':
            result += L'\n';
            break;
        default:
            result += character;
            break;
        }
        escaped = false;
    }
    if (escaped)
    {
        result += L'\\';
    }
    return result;
}

bool ReadFile(const std::wstring& path, std::string& contents)
{
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        if (GetLastError() == ERROR_FILE_NOT_FOUND)
        {
            contents.clear();
            return true;
        }
        return false;
    }

    LARGE_INTEGER size{};
    if (!GetFileSizeEx(file, &size) || size.QuadPart < 0 || size.QuadPart > 4 * 1024 * 1024)
    {
        CloseHandle(file);
        return false;
    }

    contents.resize(static_cast<size_t>(size.QuadPart));
    DWORD bytes_read = 0;
    const bool success =
        contents.empty() ||
        ::ReadFile(file, &contents[0], static_cast<DWORD>(contents.size()), &bytes_read, nullptr);
    CloseHandle(file);
    return success && static_cast<size_t>(bytes_read) == contents.size();
}

bool WriteFile(const std::wstring& path, const std::string& contents)
{
    if (contents.size() > MAXDWORD)
    {
        return false;
    }

    const size_t separator = path.find_last_of(L"\\/");
    if (separator != std::wstring::npos)
    {
        CreateDirectoryW(path.substr(0, separator).c_str(), nullptr);
    }

    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    DWORD bytes_written = 0;
    const bool success =
        contents.empty() || ::WriteFile(file, contents.data(), static_cast<DWORD>(contents.size()),
                                        &bytes_written, nullptr);
    CloseHandle(file);
    return success && static_cast<size_t>(bytes_written) == contents.size();
}

std::wstring EnvironmentPath(const wchar_t* name)
{
    wchar_t buffer[MAX_PATH]{};
    const DWORD capacity = static_cast<DWORD>(sizeof(buffer) / sizeof(buffer[0]));
    const DWORD length = GetEnvironmentVariableW(name, buffer, capacity);
    if (length == 0 || length >= capacity)
    {
        return {};
    }
    return buffer;
}

} // namespace

std::wstring ConfigPath()
{
    std::wstring base = EnvironmentPath(L"APPDATA");
    if (base.empty())
    {
        base = EnvironmentPath(L"TEMP");
    }
    if (base.empty())
    {
        base = L".";
    }

    while (!base.empty() && (base.back() == L'\\' || base.back() == L'/'))
    {
        base.pop_back();
    }

    const std::wstring directory = base + L"\\" + kConfigDirectory;
    CreateDirectoryW(directory.c_str(), nullptr);
    return directory + L"\\" + kConfigFile;
}

bool LoadConfig(Config& config, const std::wstring& path)
{
    config = Config{};

    std::string bytes;
    if (!ReadFile(path, bytes))
    {
        return false;
    }

    const std::wstring contents = FromUtf8(bytes);
    if (!bytes.empty() && contents.empty())
    {
        return false;
    }

    bool blacklist_seen = false;
    std::wistringstream lines(contents);
    std::wstring line;
    while (std::getline(lines, line))
    {
        if (!line.empty() && line.back() == L'\r')
        {
            line.pop_back();
        }

        if (line.rfind(L"start_with_windows=", 0) == 0)
        {
            config.start_with_windows = line.substr(19) == L"1";
            continue;
        }

        if (line.rfind(L"target=", 0) == 0)
        {
            const std::wstring value = line.substr(7);
            const size_t separator = value.find(L'\t');
            if (separator != std::wstring::npos)
            {
                const size_t label_separator = value.find(L'\t', separator + 1);
                AddTarget(config, Target{
                                      Unescape(value.substr(0, separator)),
                                      Unescape(value.substr(separator + 1,
                                                            label_separator == std::wstring::npos
                                                                ? std::wstring::npos
                                                                : label_separator - separator - 1)),
                                      label_separator == std::wstring::npos
                                          ? L""
                                          : Unescape(value.substr(label_separator + 1)),
                                  });
            }
            continue;
        }

        if (line.rfind(L"blacklist=", 0) == 0)
        {
            if (!blacklist_seen)
            {
                config.blacklist.clear();
                blacklist_seen = true;
            }
            const std::wstring value = NormalizeValue(Unescape(line.substr(10)));
            if (!value.empty())
            {
                config.blacklist.push_back(value);
            }
        }
    }

    const Config defaults;
    for (const std::wstring& entry : defaults.blacklist)
    {
        const std::wstring normalized_entry = NormalizeValue(FileName(entry));
        if (std::none_of(config.blacklist.begin(), config.blacklist.end(),
                         [&](const std::wstring& existing)
                         { return NormalizeValue(FileName(existing)) == normalized_entry; }))
        {
            config.blacklist.push_back(entry);
        }
    }

    return true;
}

bool SaveConfig(const Config& config, const std::wstring& path)
{
    std::wstring contents;
    contents += L"start_with_windows=";
    contents += config.start_with_windows ? L"1\n" : L"0\n";

    for (const Target& target : config.targets)
    {
        const Target normalized = NormalizeTarget(target);
        contents += L"target=" + Escape(normalized.executable_path) + L"\t" +
                    Escape(normalized.window_class) + L"\t" + Escape(normalized.label) + L"\n";
    }

    for (const std::wstring& entry : config.blacklist)
    {
        contents += L"blacklist=" + Escape(NormalizeValue(entry)) + L"\n";
    }
    return WriteFile(path, ToUtf8(contents));
}

} // namespace clipper
