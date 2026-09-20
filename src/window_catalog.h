#pragma once

#include "config.h"

#include <windows.h>

#include <string>
#include <vector>

namespace clipper
{

struct OpenWindow
{
    HWND handle = nullptr;
    Target target;
    std::wstring title;
};

std::vector<OpenWindow> EnumerateOpenWindows(HWND excluded_window);
bool DescribeWindow(HWND window, OpenWindow& result);
std::wstring WindowLabel(const OpenWindow& window);

} // namespace clipper
