#include "window_catalog.h"

#include <algorithm>
#include <array>

namespace clipper
{
namespace
{

bool ProcessPath(DWORD process_id, std::wstring& path)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, process_id);
    if (process == nullptr)
    {
        return false;
    }

    std::vector<wchar_t> buffer(512);
    bool success = false;
    for (;;)
    {
        DWORD size = static_cast<DWORD>(buffer.size());
        if (QueryFullProcessImageNameW(process, 0, buffer.data(), &size))
        {
            path.assign(buffer.data(), size);
            success = true;
            break;
        }
        if (GetLastError() != ERROR_INSUFFICIENT_BUFFER || buffer.size() >= 32768)
        {
            break;
        }
        buffer.resize(buffer.size() * 2);
    }

    CloseHandle(process);
    return success;
}

std::wstring WindowTitle(HWND window)
{
    const int length = GetWindowTextLengthW(window);
    if (length <= 0)
    {
        return {};
    }

    std::wstring title(static_cast<size_t>(length) + 1, L'\0');
    const int copied = GetWindowTextW(window, &title[0], static_cast<int>(title.size()));
    title.resize(static_cast<size_t>(copied > 0 ? copied : 0));
    return title;
}

struct EnumerationContext
{
    HWND excluded_window;
    DWORD current_process;
    std::vector<OpenWindow>* windows;
};

BOOL CALLBACK EnumerateCallback(HWND window, LPARAM parameter)
{
    auto& context = *reinterpret_cast<EnumerationContext*>(parameter);
    if (window == context.excluded_window || !IsWindowVisible(window))
    {
        return TRUE;
    }

    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id == 0 || process_id == context.current_process)
    {
        return TRUE;
    }

    OpenWindow item;
    if (DescribeWindow(window, item))
    {
        context.windows->push_back(std::move(item));
    }
    return TRUE;
}

} // namespace

bool DescribeWindow(HWND window, OpenWindow& result)
{
    if (window == nullptr || !IsWindow(window))
    {
        return false;
    }

    const HWND root = GetAncestor(window, GA_ROOT);
    if (root != nullptr)
    {
        window = root;
    }

    DWORD process_id = 0;
    GetWindowThreadProcessId(window, &process_id);
    if (process_id == 0)
    {
        return false;
    }

    std::wstring executable_path;
    if (!ProcessPath(process_id, executable_path))
    {
        return false;
    }

    std::array<wchar_t, 256> class_name{};
    const int class_length =
        GetClassNameW(window, class_name.data(), static_cast<int>(class_name.size()));
    if (class_length <= 0)
    {
        return false;
    }

    result.handle = window;
    result.target = NormalizeTarget(Target{
        std::move(executable_path),
        std::wstring(class_name.data(), static_cast<size_t>(class_length)),
        {},
    });
    result.title = WindowTitle(window);
    return true;
}

std::vector<OpenWindow> EnumerateOpenWindows(HWND excluded_window)
{
    std::vector<OpenWindow> windows;
    EnumerationContext context{excluded_window, GetCurrentProcessId(), &windows};
    EnumWindows(EnumerateCallback, reinterpret_cast<LPARAM>(&context));
    std::sort(windows.begin(), windows.end(), [](const OpenWindow& left, const OpenWindow& right)
              { return WindowLabel(left) < WindowLabel(right); });
    return windows;
}

std::wstring WindowLabel(const OpenWindow& window)
{
    const std::wstring title = window.title.empty() ? L"(untitled)" : window.title;
    return title + L" | " + FileName(window.target.executable_path) + L" | " +
           window.target.window_class;
}

} // namespace clipper
