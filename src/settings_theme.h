#pragma once

#include <windows.h>

namespace clipper::settings_theme
{

inline constexpr COLORREF kBackground = RGB(40, 42, 54);
inline constexpr COLORREF kCurrentLine = RGB(68, 71, 90);
inline constexpr COLORREF kForeground = RGB(248, 248, 242);
inline constexpr COLORREF kComment = RGB(98, 114, 164);
inline constexpr COLORREF kCyan = RGB(139, 233, 253);
inline constexpr COLORREF kPink = RGB(255, 121, 198);

inline HBRUSH BackgroundBrush()
{
    static HBRUSH brush = CreateSolidBrush(kBackground);
    return brush;
}

} // namespace clipper::settings_theme
