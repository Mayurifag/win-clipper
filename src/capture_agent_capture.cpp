#include "capture_agent.h"

#include <algorithm>

namespace clipper
{
void CaptureAgent::UpdateForegroundTarget()
{
    moving_active_window_ = false;
    active_window_ = nullptr;
    if (!capture_enabled_)
    {
        ReleaseClip();
        return;
    }

    const HWND foreground = GetForegroundWindow();
    OpenWindow description;
    if (foreground == nullptr || !DescribeWindow(foreground, description) ||
        IsBlacklisted(config_, description.target.executable_path) ||
        std::none_of(config_.targets.begin(), config_.targets.end(), [&](const Target& target)
                     { return TargetMatches(target, description.target); }))
    {
        ReleaseClip();
        return;
    }

    active_window_ = description.handle;
    UpdateClipRect();
    ApplyClip();
}

void CaptureAgent::UpdateClipRect()
{
    clip_rect_ = {};
    if (active_window_ == nullptr || !IsWindow(active_window_) || IsIconic(active_window_))
    {
        return;
    }

    RECT window_rect{};
    if (!GetWindowRect(active_window_, &window_rect))
    {
        return;
    }

    clip_rect_ = window_rect;
}

void CaptureAgent::ApplyClip()
{
    const bool can_clip =
        capture_enabled_ && active_window_ != nullptr && !moving_active_window_ &&
        IsWindow(active_window_) && !IsIconic(active_window_) &&
        GetForegroundWindow() == active_window_ &&
        clip_rect_.right > clip_rect_.left && clip_rect_.bottom > clip_rect_.top;
    ClipCursor(can_clip ? &clip_rect_ : nullptr);
}

void CaptureAgent::ReleaseClip()
{
    clip_rect_ = {};
    ClipCursor(nullptr);
}

LRESULT CALLBACK CaptureAgent::MouseHookProc(int code, WPARAM w_param, LPARAM l_param)
{
    if (code >= 0 && current_ != nullptr && w_param != WM_MOUSEMOVE && w_param != WM_MOUSEWHEEL &&
        w_param != WM_MOUSEHWHEEL)
    {
        current_->ApplyClip();
    }
    return CallNextHookEx(current_ == nullptr ? nullptr : current_->mouse_hook_, code, w_param,
                          l_param);
}

void CALLBACK CaptureAgent::WinEventProc(HWINEVENTHOOK, DWORD event, HWND window, LONG object_id,
                                         LONG child_id, DWORD, DWORD)
{
    if (current_ == nullptr)
    {
        return;
    }

    if (event == EVENT_SYSTEM_MOVESIZESTART || event == EVENT_SYSTEM_MOVESIZEEND)
    {
        if (window != current_->active_window_)
        {
            return;
        }

        if (event == EVENT_SYSTEM_MOVESIZESTART)
        {
            current_->moving_active_window_ = true;
            current_->ReleaseClip();
        }
        else
        {
            current_->moving_active_window_ = false;
            current_->UpdateForegroundTarget();
        }
        return;
    }

    if (event == EVENT_OBJECT_LOCATIONCHANGE &&
        (object_id != OBJID_WINDOW || child_id != CHILDID_SELF ||
         window != current_->active_window_ || current_->moving_active_window_))
    {
        return;
    }

    if (event != EVENT_SYSTEM_FOREGROUND && event != EVENT_OBJECT_LOCATIONCHANGE)
    {
        return;
    }

    current_->UpdateForegroundTarget();
}

} // namespace clipper
