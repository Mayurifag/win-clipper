#pragma once

#include "config.h"
#include "window_catalog.h"

#include <windows.h>

#include <memory>
#include <string>

namespace clipper
{

class SettingsWindow;

inline constexpr wchar_t kAgentWindowClass[] = L"WinClipper.Agent";
inline constexpr UINT kShowSettingsMessage = WM_APP + 1;

class CaptureAgent
{
  public:
    explicit CaptureAgent(HINSTANCE instance);
    ~CaptureAgent();

    CaptureAgent(const CaptureAgent&) = delete;
    CaptureAgent& operator=(const CaptureAgent&) = delete;

    bool Initialize();
    int Run(bool show_settings);
    bool ShowSettings();
    void ToggleCapture();

    HINSTANCE instance() const
    {
        return instance_;
    }
    HWND window() const
    {
        return window_;
    }
    const Config& config() const
    {
        return config_;
    }
    std::vector<OpenWindow> OpenWindows() const;
    bool AddTarget(Target target);
    bool RemoveTargetAt(size_t index);
    bool SetStartWithWindows(bool enabled);

  private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param);
    static LRESULT CALLBACK MouseHookProc(int code, WPARAM w_param, LPARAM l_param);
    static void CALLBACK WinEventProc(HWINEVENTHOOK hook, DWORD event, HWND window, LONG object_id,
                                      LONG child_id, DWORD event_thread, DWORD event_time);

    bool RegisterAgentWindow();
    void Stop();
    void UpdateForegroundTarget();
    void UpdateClipRect();
    void ApplyClip();
    void ReleaseClip();

    HINSTANCE instance_ = nullptr;
    HWND window_ = nullptr;
    HHOOK mouse_hook_ = nullptr;
    HWINEVENTHOOK event_hook_ = nullptr;
    HWND active_window_ = nullptr;
    RECT clip_rect_{};
    bool capture_enabled_ = true;
    bool moving_active_window_ = false;
    bool stopping_ = false;
    Config config_;
    std::wstring config_path_;
    std::wstring executable_path_;
    std::unique_ptr<SettingsWindow> settings_;

    static CaptureAgent* current_;
};

} // namespace clipper
