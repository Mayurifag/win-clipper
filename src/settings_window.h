#pragma once

#include "window_catalog.h"

#include <windows.h>

#include <vector>

namespace clipper
{

class CaptureAgent;

class SettingsWindow
{
  public:
    enum ControlId : int
    {
        kAvailableList = 1001,
        kSavedList = 1002,
        kStartup = 1003,
    };

    static constexpr UINT kRefreshTimer = 1;

    SettingsWindow() = default;
    ~SettingsWindow();

    SettingsWindow(const SettingsWindow&) = delete;
    SettingsWindow& operator=(const SettingsWindow&) = delete;

    bool Create(CaptureAgent& agent);
    void Show();
    HWND window() const
    {
        return window_;
    }

  private:
    static LRESULT CALLBACK WindowProc(HWND window, UINT message, WPARAM w_param, LPARAM l_param);
    void CreateControls();
    void Layout();
    void Refresh();
    void SetStatus(const wchar_t* text);
    LRESULT HandleNotify(NMHDR* notification);
    static void ConfigureList(HWND list, bool available);
    void AddAvailableTarget(int item);
    void RemoveSavedTarget(int item);

    CaptureAgent* agent_ = nullptr;
    HWND window_ = nullptr;
    HWND available_label_ = nullptr;
    HWND available_list_ = nullptr;
    HWND saved_label_ = nullptr;
    HWND saved_list_ = nullptr;
    HWND hotkey_label_ = nullptr;
    HWND startup_checkbox_ = nullptr;
    HWND blacklist_label_ = nullptr;
    HWND status_label_ = nullptr;
    HFONT font_ = nullptr;
    std::vector<OpenWindow> available_windows_;
    std::vector<Target> saved_targets_;
};

} // namespace clipper
