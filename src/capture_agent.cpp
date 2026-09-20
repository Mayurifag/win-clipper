#include "capture_agent.h"

#include "resource.h"
#include "settings_window.h"
#include "startup.h"

#include <algorithm>
#include <utility>

namespace clipper
{

CaptureAgent* CaptureAgent::current_ = nullptr;

namespace
{

constexpr int kHotkeyId = 1;

std::wstring ModulePath()
{
    std::vector<wchar_t> buffer(512);
    for (;;)
    {
        const DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0)
        {
            return {};
        }
        if (length < buffer.size() - 1)
        {
            return std::wstring(buffer.data(), length);
        }
        buffer.resize(buffer.size() * 2);
    }
}

void PopulateTargetLabels(Config& config)
{
    const std::vector<OpenWindow> windows = EnumerateOpenWindows(nullptr);
    for (Target& target : config.targets)
    {
        if (!target.label.empty())
        {
            continue;
        }

        const auto match =
            std::find_if(windows.begin(), windows.end(), [&](const OpenWindow& window)
                         { return TargetMatches(target, window.target); });
        if (match != windows.end() && !match->title.empty())
        {
            target.label = match->title;
        }
    }
}

} // namespace

CaptureAgent::CaptureAgent(HINSTANCE instance) : instance_(instance) {}

CaptureAgent::~CaptureAgent()
{
    Stop();
}

bool CaptureAgent::Initialize()
{
    config_path_ = ConfigPath();
    if (!LoadConfig(config_, config_path_))
    {
        return false;
    }

    executable_path_ = ModulePath();
    if (executable_path_.empty() || !RegisterAgentWindow())
    {
        return false;
    }

    current_ = this;
    PopulateTargetLabels(config_);
    SaveConfig(config_, config_path_);
    clipper::SetStartWithWindows(config_.start_with_windows, executable_path_);

    settings_ = std::make_unique<SettingsWindow>();
    if (!settings_->Create(*this))
    {
        Stop();
        return false;
    }

    mouse_hook_ = SetWindowsHookExW(WH_MOUSE_LL, MouseHookProc, nullptr, 0);
    event_hook_ =
        SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_OBJECT_LOCATIONCHANGE, nullptr, WinEventProc,
                        0, 0, WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS);
    RegisterHotKey(window_, kHotkeyId, MOD_CONTROL | MOD_WIN, VK_RETURN);
    UpdateForegroundTarget();
    return mouse_hook_ != nullptr && event_hook_ != nullptr;
}

int CaptureAgent::Run(bool show_settings)
{
    if (show_settings)
    {
        ShowSettings();
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}

void CaptureAgent::ShowSettings()
{
    if (settings_ == nullptr)
    {
        settings_ = std::make_unique<SettingsWindow>();
    }
    if (settings_->window() == nullptr && !settings_->Create(*this))
    {
        settings_.reset();
        return;
    }
    settings_->Show();
}

void CaptureAgent::ToggleCapture()
{
    capture_enabled_ = !capture_enabled_;
    UpdateForegroundTarget();
}

std::vector<OpenWindow> CaptureAgent::OpenWindows() const
{
    std::vector<OpenWindow> windows =
        EnumerateOpenWindows(settings_ == nullptr ? nullptr : settings_->window());
    windows.erase(std::remove_if(windows.begin(), windows.end(),
                                 [&](const OpenWindow& window)
                                 {
                                     if (IsBlacklisted(config_, window.target.executable_path))
                                     {
                                         return true;
                                     }
                                     return std::any_of(
                                         config_.targets.begin(), config_.targets.end(),
                                         [&](const Target& target)
                                         { return TargetMatches(target, window.target); });
                                 }),
                  windows.end());
    return windows;
}

bool CaptureAgent::AddTarget(Target target)
{
    target = NormalizeTarget(std::move(target));
    if (IsBlacklisted(config_, target.executable_path) || target.executable_path.empty() ||
        target.window_class.empty())
    {
        return false;
    }

    const size_t old_size = config_.targets.size();
    clipper::AddTarget(config_, std::move(target));
    if (config_.targets.size() == old_size)
    {
        return true;
    }

    const bool saved = SaveConfig(config_, config_path_);
    UpdateForegroundTarget();
    return saved;
}

bool CaptureAgent::RemoveTargetAt(size_t index)
{
    if (index >= config_.targets.size())
    {
        return false;
    }

    config_.targets.erase(config_.targets.begin() + static_cast<std::ptrdiff_t>(index));
    const bool saved = SaveConfig(config_, config_path_);
    if (config_.targets.empty())
    {
        active_window_ = nullptr;
        ReleaseClip();
    }
    else
    {
        UpdateForegroundTarget();
    }
    return saved;
}

bool CaptureAgent::SetStartWithWindows(bool enabled)
{
    config_.start_with_windows = enabled;
    const bool saved = SaveConfig(config_, config_path_);
    const bool startup = clipper::SetStartWithWindows(enabled, executable_path_);
    return saved && startup;
}

bool CaptureAgent::RegisterAgentWindow()
{
    WNDCLASSEXW window_class{};
    window_class.cbSize = sizeof(window_class);
    window_class.hInstance = instance_;
    window_class.lpfnWndProc = WindowProc;
    window_class.lpszClassName = kAgentWindowClass;
    window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
    window_class.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_MAIN));
    if (window_class.hIcon == nullptr)
    {
        window_class.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32512));
    }

    if (RegisterClassExW(&window_class) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
    {
        return false;
    }

    window_ = CreateWindowExW(0, kAgentWindowClass, L"WinClipper", WS_OVERLAPPED, 0, 0, 0, 0,
                              nullptr, nullptr, instance_, this);
    return window_ != nullptr;
}

void CaptureAgent::Stop()
{
    if (stopping_)
    {
        return;
    }
    stopping_ = true;

    ReleaseClip();
    if (mouse_hook_ != nullptr)
    {
        UnhookWindowsHookEx(mouse_hook_);
        mouse_hook_ = nullptr;
    }
    if (event_hook_ != nullptr)
    {
        UnhookWinEvent(event_hook_);
        event_hook_ = nullptr;
    }
    if (window_ != nullptr)
    {
        UnregisterHotKey(window_, kHotkeyId);
    }
    settings_.reset();
    if (window_ != nullptr)
    {
        DestroyWindow(window_);
        window_ = nullptr;
    }
    current_ = nullptr;
}

LRESULT CALLBACK CaptureAgent::WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param)
{
    auto* agent = reinterpret_cast<CaptureAgent*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        const auto* create = reinterpret_cast<const CREATESTRUCTW*>(l_param);
        agent = static_cast<CaptureAgent*>(create->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(agent));
    }

    if (agent == nullptr)
    {
        return DefWindowProcW(window, message, w_param, l_param);
    }

    switch (message)
    {
    case kShowSettingsMessage:
        agent->ShowSettings();
        return 0;
    case WM_HOTKEY:
        if (w_param == kHotkeyId)
        {
            agent->ToggleCapture();
        }
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window, message, w_param, l_param);
    }
}

} // namespace clipper
