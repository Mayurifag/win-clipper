#include "settings_window.h"

#include "capture_agent.h"
#include "resource.h"
#include "settings_theme.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <uxtheme.h>

namespace clipper
{
namespace
{

constexpr wchar_t kSettingsWindowClass[] = L"WinClipper.Settings";
constexpr DWORD kDarkModeAttribute = 20;
constexpr int kActionColumnWidth = 36;
constexpr int kFooterHeight = 126;

using settings_theme::BackgroundBrush;
using settings_theme::kBackground;
using settings_theme::kCyan;
using settings_theme::kCurrentLine;
using settings_theme::kForeground;
using settings_theme::kComment;

HWND CreateControl(const wchar_t* class_name, const wchar_t* text, DWORD style, HWND parent, int id,
                   HINSTANCE instance)
{
    return CreateWindowExW(0, class_name, text, WS_CHILD | WS_VISIBLE | style, 0, 0, 0, 0, parent,
                           reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), instance, nullptr);
}

HWND CreateList(HWND parent, int id, HINSTANCE instance)
{
    constexpr DWORD style =
        LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOSORTHEADER | WS_VSCROLL | WS_TABSTOP;
    return CreateWindowExW(WS_EX_CLIENTEDGE, WC_LISTVIEWW, L"", WS_CHILD | WS_VISIBLE | style, 0, 0,
                           0, 0, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                           instance, nullptr);
}

void SetFont(HWND control, HFONT font)
{
    SendMessageW(control, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SetWindowTheme(control, L"", L"");
}

void SetColumn(HWND list, int index, int width, int format)
{
    LVCOLUMNW column{};
    column.mask = LVCF_WIDTH | LVCF_FMT;
    column.cx = width;
    column.fmt = format;
    ListView_SetColumn(list, index, &column);
}

} // namespace

SettingsWindow::~SettingsWindow()
{
    if (window_ != nullptr)
    {
        KillTimer(window_, kRefreshTimer);
        DestroyWindow(window_);
    }
}

bool SettingsWindow::Create(CaptureAgent& agent)
{
    agent_ = &agent;

    INITCOMMONCONTROLSEX common_controls{};
    common_controls.dwSize = sizeof(common_controls);
    common_controls.dwICC = ICC_LISTVIEW_CLASSES;
    if (!InitCommonControlsEx(&common_controls))
    {
        return false;
    }

    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = agent.instance();
    window_class.lpfnWndProc = WindowProc;
    window_class.lpszClassName = kSettingsWindowClass;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.hIcon = LoadIconW(agent.instance(), MAKEINTRESOURCEW(IDI_MAIN));
    window_class.hIconSm = window_class.hIcon;
    window_class.hbrBackground = BackgroundBrush();
    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return false;
    }

    window_ = CreateWindowExW(
        WS_EX_APPWINDOW, kSettingsWindowClass, L"WinClipper",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_THICKFRAME, CW_USEDEFAULT,
        CW_USEDEFAULT, 820, 520, agent.window(), nullptr, agent.instance(), this);
    return window_ != nullptr;
}

void SettingsWindow::Show()
{
    if (window_ == nullptr)
    {
        return;
    }
    const BOOL dark_mode = TRUE;
    DwmSetWindowAttribute(window_, kDarkModeAttribute, &dark_mode, sizeof(dark_mode));
    Refresh();
    ShowWindow(window_, SW_SHOWNORMAL);
    SetForegroundWindow(window_);
    SetFocus(available_list_);
}

void SettingsWindow::CreateControls()
{
    font_ = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    const HINSTANCE instance =
        reinterpret_cast<HINSTANCE>(GetWindowLongPtrW(window_, GWLP_HINSTANCE));

    available_label_ =
        CreateControl(L"STATIC", L"Open windows", SS_LEFTNOWORDWRAP, window_, 0, instance);
    available_list_ = CreateList(window_, kAvailableList, instance);
    saved_label_ =
        CreateControl(L"STATIC", L"Saved targets", SS_LEFTNOWORDWRAP, window_, 0, instance);
    saved_list_ = CreateList(window_, kSavedList, instance);
    hotkey_label_ = CreateControl(L"STATIC", L"Ctrl+Win+Enter toggles capture",
                                  SS_RIGHT | SS_NOPREFIX, window_, 0, instance);
    startup_checkbox_ = CreateControl(L"BUTTON", L"Start with Windows",
                                      BS_AUTOCHECKBOX | WS_TABSTOP, window_, kStartup, instance);
    blacklist_label_ = CreateControl(L"STATIC", nullptr, SS_LEFT | SS_NOPREFIX, window_, 0,
                                     instance);
    status_label_ = CreateControl(L"STATIC", L"Service ready.", SS_LEFT | SS_NOPREFIX, window_, 0,
                                  instance);

    const HWND controls[] = {
        available_label_, available_list_,   saved_label_,     saved_list_,
        hotkey_label_,    startup_checkbox_, blacklist_label_, status_label_,
    };
    for (const HWND control : controls)
    {
        SetFont(control, font_);
    }
    ConfigureList(available_list_, true);
    ConfigureList(saved_list_, false);
    SendMessageW(startup_checkbox_, BM_SETCHECK,
                 agent_->config().start_with_windows ? BST_CHECKED : BST_UNCHECKED, 0);
    SetTimer(window_, kRefreshTimer, 2000, nullptr);
}

void SettingsWindow::Layout()
{
    RECT client{};
    GetClientRect(window_, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    constexpr int margin = 14;
    constexpr int gap = 14;
    constexpr int label_height = 22;
    const int content_width = width > margin * 2 ? width - margin * 2 : 0;
    const int column_width = content_width > gap ? (content_width - gap) / 2 : 0;
    const int list_top = margin + label_height;
    const int list_bottom = height - kFooterHeight;
    const int list_height = list_bottom - list_top > 120 ? list_bottom - list_top : 120;
    const int left = margin;
    const int right = margin + column_width + gap;

    MoveWindow(available_label_, left, margin, column_width, label_height, TRUE);
    MoveWindow(saved_label_, right, margin, column_width, label_height, TRUE);
    MoveWindow(available_list_, left, list_top, column_width, list_height, TRUE);
    MoveWindow(saved_list_, right, list_top, column_width, list_height, TRUE);

    const int bottom_top = list_bottom + 12;
    MoveWindow(startup_checkbox_, left, bottom_top, 180, 22, TRUE);
    const int hotkey_width = content_width > 190 ? content_width - 190 : 0;
    MoveWindow(hotkey_label_, left + 190, bottom_top, hotkey_width, 22, TRUE);
    MoveWindow(blacklist_label_, left, bottom_top + 30, content_width, 38, TRUE);
    MoveWindow(status_label_, left, bottom_top + 76, content_width, 22, TRUE);

    const int available_width = column_width - 4 > 180 ? column_width - 4 : 180;
    const int available_process = available_width * 28 / 100;
    const int available_class = available_width * 22 / 100;
    SetColumn(available_list_, 0,
              available_width - kActionColumnWidth - available_process - available_class,
              LVCFMT_LEFT);
    SetColumn(available_list_, 1, available_process, LVCFMT_LEFT);
    SetColumn(available_list_, 2, available_class, LVCFMT_LEFT);
    SetColumn(available_list_, 3, kActionColumnWidth, LVCFMT_CENTER);

    const int saved_class = available_width * 35 / 100;
    SetColumn(saved_list_, 0, available_width - kActionColumnWidth - saved_class, LVCFMT_LEFT);
    SetColumn(saved_list_, 1, saved_class, LVCFMT_LEFT);
    SetColumn(saved_list_, 2, kActionColumnWidth, LVCFMT_CENTER);
}

LRESULT CALLBACK SettingsWindow::WindowProc(HWND window, UINT message, WPARAM w_param,
                                             LPARAM l_param)
{
    auto* settings = reinterpret_cast<SettingsWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
        settings = static_cast<SettingsWindow*>(create->lpCreateParams);
        settings->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(settings));
    }

    if (settings == nullptr)
    {
        return DefWindowProcW(window, message, w_param, l_param);
    }

    switch (message)
    {
    case WM_CREATE:
        settings->CreateControls();
        settings->Layout();
        return 0;
    case WM_SIZE:
        settings->Layout();
        return 0;
    case WM_GETMINMAXINFO:
    {
        auto* limits = reinterpret_cast<MINMAXINFO*>(l_param);
        limits->ptMinTrackSize.x = 720;
        limits->ptMinTrackSize.y = 440;
        return 0;
    }
    case WM_TIMER:
        if (w_param == kRefreshTimer)
        {
            settings->Refresh();
            return 0;
        }
        break;
    case WM_NOTIFY:
        return settings->HandleNotify(reinterpret_cast<NMHDR*>(l_param));
    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        const HDC device_context = BeginPaint(window, &paint);
        RECT client{};
        GetClientRect(window, &client);
        HPEN separator = CreatePen(PS_SOLID, 1, kCurrentLine);
        const HGDIOBJ previous = SelectObject(device_context, separator);
        MoveToEx(device_context, 14, client.bottom - kFooterHeight, nullptr);
        LineTo(device_context, client.right - 14, client.bottom - kFooterHeight);
        SelectObject(device_context, previous);
        DeleteObject(separator);
        EndPaint(window, &paint);
        return 0;
    }
    case WM_ERASEBKGND:
    {
        RECT client{};
        GetClientRect(window, &client);
        FillRect(reinterpret_cast<HDC>(w_param), &client, BackgroundBrush());
        return 1;
    }
    case WM_CTLCOLORSTATIC:
    {
        const HDC device_context = reinterpret_cast<HDC>(w_param);
        const HWND control = reinterpret_cast<HWND>(l_param);
        const COLORREF text_color = control == settings->status_label_ ||
                                            control == settings->blacklist_label_
                                        ? kComment
                                    : control == settings->hotkey_label_ ? kCyan
                                                                         : kForeground;
        SetTextColor(device_context, text_color);
        SetBkColor(device_context, kBackground);
        return reinterpret_cast<LRESULT>(BackgroundBrush());
    }
    case WM_CTLCOLORBTN:
    {
        const HDC device_context = reinterpret_cast<HDC>(w_param);
        SetTextColor(device_context, kForeground);
        SetBkColor(device_context, kBackground);
        return reinterpret_cast<LRESULT>(BackgroundBrush());
    }
    case WM_COMMAND:
        if (LOWORD(w_param) == kStartup && HIWORD(w_param) == BN_CLICKED)
        {
            const bool enabled =
                SendMessageW(settings->startup_checkbox_, BM_GETCHECK, 0, 0) == BST_CHECKED;
            if (settings->agent_->SetStartWithWindows(enabled))
            {
                settings->SetStatus(L"Startup setting updated.");
            }
            else
            {
                settings->SetStatus(L"Startup setting could not be updated.");
            }
            return 0;
        }
        break;
    case WM_CLOSE:
        KillTimer(window, kRefreshTimer);
        DestroyWindow(window);
        return 0;
    case WM_NCDESTROY:
        settings->window_ = nullptr;
        settings->available_label_ = nullptr;
        settings->available_list_ = nullptr;
        settings->saved_label_ = nullptr;
        settings->saved_list_ = nullptr;
        settings->hotkey_label_ = nullptr;
        settings->startup_checkbox_ = nullptr;
        settings->blacklist_label_ = nullptr;
        settings->status_label_ = nullptr;
        settings->available_windows_.clear();
        settings->saved_targets_.clear();
        return DefWindowProcW(window, message, w_param, l_param);
    default:
        break;
    }

    return DefWindowProcW(window, message, w_param, l_param);
}

} // namespace clipper
