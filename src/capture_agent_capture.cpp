#include "capture_agent.h"

#include <algorithm>

namespace clipper
{
bool CaptureAgent::IsCaptureTarget(const OpenWindow& window) const
{
    return !IsBlacklisted(config_, window.target.executable_path) &&
           std::any_of(config_.targets.begin(), config_.targets.end(),
                       [&](const Target& target) { return TargetMatches(target, window.target); });
}

void CaptureAgent::UpdateForegroundTarget()
{
    if (moving_window_ != nullptr)
    {
        if (IsWindow(moving_window_))
        {
            ReleaseClip();
            return;
        }
        moving_window_ = nullptr;
    }

    active_window_ = nullptr;
    if (!capture_enabled_)
    {
        ReleaseClip();
        return;
    }

    const HWND foreground = GetForegroundWindow();
    OpenWindow description;
    if (foreground == nullptr || !DescribeWindow(foreground, description) ||
        !IsCaptureTarget(description))
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

    RECT client{};
    if (!GetClientRect(active_window_, &client))
    {
        return;
    }

    // Map the rectangle together so mirrored windows keep left <= right.
    SetLastError(ERROR_SUCCESS);
    if (MapWindowPoints(active_window_, nullptr, reinterpret_cast<POINT*>(&client), 2) == 0 &&
        GetLastError() != ERROR_SUCCESS)
    {
        return;
    }

    clip_rect_ = client;
}

void CaptureAgent::ApplyClip()
{
    const bool can_clip = capture_enabled_ && active_window_ != nullptr &&
                          moving_window_ == nullptr && IsWindow(active_window_) &&
                          !IsIconic(active_window_) && GetForegroundWindow() == active_window_ &&
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
        if (event == EVENT_SYSTEM_MOVESIZESTART)
        {
            if (window != current_->active_window_)
            {
                return;
            }

            current_->moving_window_ = window;
            current_->ReleaseClip();
            return;
        }

        if (window != current_->moving_window_)
        {
            return;
        }

        current_->moving_window_ = nullptr;
        current_->UpdateForegroundTarget();
        return;
    }

    if (event == EVENT_OBJECT_LOCATIONCHANGE &&
        (object_id != OBJID_WINDOW || child_id != CHILDID_SELF ||
         window != current_->active_window_ || current_->moving_window_ != nullptr))
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
