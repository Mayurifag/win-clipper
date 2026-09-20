#include "settings_window.h"

#include "capture_agent.h"
#include "settings_theme.h"

#include <commctrl.h>
#include <uxtheme.h>

#include <string>

namespace clipper
{
namespace
{

using settings_theme::kBackground;
using settings_theme::kCurrentLine;
using settings_theme::kCyan;
using settings_theme::kForeground;
using settings_theme::kPink;

void InsertColumn(HWND list, int index, const wchar_t* text, int width)
{
    LVCOLUMNW column{};
    column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
    column.fmt = LVCFMT_LEFT;
    column.pszText = const_cast<LPWSTR>(text);
    column.cx = width;
    SendMessageW(list, LVM_INSERTCOLUMNW, static_cast<WPARAM>(index),
                 reinterpret_cast<LPARAM>(&column));
}

int InsertItem(HWND list, int index, const std::wstring& text)
{
    LVITEMW item{};
    item.mask = LVIF_TEXT;
    item.iItem = index;
    item.pszText = const_cast<LPWSTR>(text.c_str());
    return static_cast<int>(
        SendMessageW(list, LVM_INSERTITEMW, 0, reinterpret_cast<LPARAM>(&item)));
}

void SetItemText(HWND list, int item, int subitem, const wchar_t* text)
{
    LVITEMW value{};
    value.iSubItem = subitem;
    value.pszText = const_cast<LPWSTR>(text);
    SendMessageW(list, LVM_SETITEMTEXTW, static_cast<WPARAM>(item),
                 reinterpret_cast<LPARAM>(&value));
}

void RemoveRows(HWND list, size_t current_count, size_t wanted_count)
{
    while (current_count > wanted_count)
    {
        --current_count;
        SendMessageW(list, LVM_DELETEITEM, static_cast<WPARAM>(current_count), 0);
    }
}

void SetAvailableRow(HWND list, int row, const OpenWindow& window)
{
    const std::wstring process = FileName(window.target.executable_path);
    SetItemText(list, row, 0, window.title.c_str());
    SetItemText(list, row, 1, process.c_str());
    SetItemText(list, row, 2, window.target.window_class.c_str());
    SetItemText(list, row, 3, L"+");
}

void SetSavedRow(HWND list, int row, const Target& target)
{
    const std::wstring process = FileName(target.executable_path);
    SetItemText(list, row, 0, process.c_str());
    SetItemText(list, row, 1, target.window_class.c_str());
    SetItemText(list, row, 2, L"x");
}

HWND ListHeader(HWND list)
{
    return reinterpret_cast<HWND>(SendMessageW(list, LVM_GETHEADER, 0, 0));
}

} // namespace

void SettingsWindow::ConfigureList(HWND list, bool available)
{
    constexpr DWORD extended_style = LVS_EX_DOUBLEBUFFER | LVS_EX_FULLROWSELECT | LVS_EX_LABELTIP;
    SendMessageW(list, LVM_SETEXTENDEDLISTVIEWSTYLE, extended_style, extended_style);
    SendMessageW(list, LVM_SETBKCOLOR, 0, static_cast<LPARAM>(kBackground));
    SendMessageW(list, LVM_SETTEXTBKCOLOR, 0, static_cast<LPARAM>(kBackground));
    SendMessageW(list, LVM_SETTEXTCOLOR, 0, static_cast<LPARAM>(kForeground));
    const HWND header = ListHeader(list);
    if (header != nullptr)
    {
        SetWindowTheme(header, L"", L"");
    }
    if (available)
    {
        InsertColumn(list, 0, L"Window", 300);
        InsertColumn(list, 1, L"Process", 180);
        InsertColumn(list, 2, L"Class", 150);
        InsertColumn(list, 3, L"+", 36);
    }
    else
    {
        InsertColumn(list, 0, L"Process", 300);
        InsertColumn(list, 1, L"Class", 294);
        InsertColumn(list, 2, L"x", 36);
    }
}

void SettingsWindow::Refresh()
{
    const std::vector<OpenWindow> next_windows = agent_->OpenWindows();
    const size_t shared_windows = available_windows_.size() < next_windows.size()
                                      ? available_windows_.size()
                                      : next_windows.size();
    for (size_t index = 0; index < shared_windows; ++index)
    {
        if (available_windows_[index].title != next_windows[index].title ||
            available_windows_[index].target.executable_path !=
                next_windows[index].target.executable_path ||
            available_windows_[index].target.window_class !=
                next_windows[index].target.window_class)
        {
            SetAvailableRow(available_list_, static_cast<int>(index), next_windows[index]);
        }
    }
    for (size_t index = shared_windows; index < next_windows.size(); ++index)
    {
        const int row = InsertItem(available_list_, static_cast<int>(index), L"");
        if (row >= 0)
        {
            SetAvailableRow(available_list_, row, next_windows[index]);
        }
    }
    RemoveRows(available_list_, available_windows_.size(), next_windows.size());
    available_windows_ = next_windows;

    const auto& targets = agent_->config().targets;
    const size_t shared_targets =
        saved_targets_.size() < targets.size() ? saved_targets_.size() : targets.size();
    for (size_t index = 0; index < shared_targets; ++index)
    {
        if (saved_targets_[index].executable_path != targets[index].executable_path ||
            saved_targets_[index].window_class != targets[index].window_class)
        {
            SetSavedRow(saved_list_, static_cast<int>(index), targets[index]);
        }
    }
    for (size_t index = shared_targets; index < targets.size(); ++index)
    {
        const int row = InsertItem(saved_list_, static_cast<int>(index), L"");
        if (row >= 0)
        {
            SetSavedRow(saved_list_, row, targets[index]);
        }
    }
    RemoveRows(saved_list_, saved_targets_.size(), targets.size());
    saved_targets_ = targets;

    std::wstring blacklist = L"Ignored: ";
    for (size_t index = 0; index < agent_->config().blacklist.size(); ++index)
    {
        if (index != 0)
        {
            blacklist += L", ";
        }
        blacklist += agent_->config().blacklist[index];
    }
    SetWindowTextW(blacklist_label_, blacklist.c_str());
}

void SettingsWindow::SetStatus(const wchar_t* text)
{
    SetWindowTextW(status_label_, text);
}

LRESULT SettingsWindow::HandleNotify(NMHDR* notification)
{
    if (notification == nullptr)
    {
        return 0;
    }

    if (notification->code == NM_CUSTOMDRAW)
    {
        const HWND available_header = ListHeader(available_list_);
        const HWND saved_header = ListHeader(saved_list_);
        if (notification->hwndFrom == available_header || notification->hwndFrom == saved_header)
        {
            auto* header = reinterpret_cast<NMCUSTOMDRAW*>(notification);
            if (header->dwDrawStage == CDDS_PREPAINT)
            {
                return CDRF_NOTIFYITEMDRAW;
            }
            if (header->dwDrawStage == CDDS_ITEMPREPAINT)
            {
                SetTextColor(header->hdc, kForeground);
                SetBkColor(header->hdc, kCurrentLine);
                return CDRF_DODEFAULT;
            }
            return CDRF_DODEFAULT;
        }

        auto* draw = reinterpret_cast<NMLVCUSTOMDRAW*>(notification);
        if (draw->nmcd.dwDrawStage == CDDS_PREPAINT)
        {
            return CDRF_NOTIFYITEMDRAW;
        }
        if (draw->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM))
        {
            draw->clrTextBk = (draw->nmcd.dwItemSpec % 2 == 0) ? kBackground : kCurrentLine;
            draw->clrText = kForeground;
            if (notification->hwndFrom == available_list_ && draw->iSubItem == 3)
            {
                draw->clrText = kCyan;
            }
            else if (notification->hwndFrom == saved_list_ && draw->iSubItem == 2)
            {
                draw->clrText = kPink;
            }
            return CDRF_DODEFAULT;
        }
        if (draw->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
        {
            return CDRF_NOTIFYSUBITEMDRAW;
        }
        return CDRF_DODEFAULT;
    }

    if (notification->code != NM_CLICK)
    {
        return 0;
    }

    const auto* click = reinterpret_cast<const NMITEMACTIVATE*>(notification);
    if (click->iItem < 0)
    {
        return 0;
    }
    if (notification->hwndFrom == available_list_ && click->iSubItem == 3)
    {
        AddAvailableTarget(click->iItem);
    }
    else if (notification->hwndFrom == saved_list_ && click->iSubItem == 2)
    {
        RemoveSavedTarget(click->iItem);
    }
    return 0;
}

void SettingsWindow::AddAvailableTarget(int item)
{
    if (item < 0 || static_cast<size_t>(item) >= available_windows_.size())
    {
        return;
    }

    if (agent_->AddTarget(available_windows_[static_cast<size_t>(item)].target))
    {
        SetStatus(L"Target saved.");
    }
    else
    {
        SetStatus(L"Target could not be saved.");
    }
    Refresh();
}

void SettingsWindow::RemoveSavedTarget(int item)
{
    const bool deletes_service = agent_->config().targets.size() == 1;
    if (item < 0 || !agent_->RemoveTargetAt(static_cast<size_t>(item)))
    {
        SetStatus(L"Target could not be removed.");
        return;
    }

    SetStatus(deletes_service ? L"Service deleted." : L"Target removed.");
    Refresh();
}

} // namespace clipper
