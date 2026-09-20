#include "capture_agent.h"

#include <windows.h>

#include <string>

namespace
{

void ShowStartupError()
{
    MessageBoxW(nullptr, L"WinClipper could not start its background agent.", L"WinClipper",
                MB_OK | MB_ICONERROR);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Local\\WinClipper.Singleton");
    if (mutex == nullptr)
    {
        ShowStartupError();
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        HWND agent_window = nullptr;
        for (int attempt = 0; attempt < 10 && agent_window == nullptr; ++attempt)
        {
            agent_window = FindWindowW(clipper::kAgentWindowClass, nullptr);
            if (agent_window == nullptr)
            {
                Sleep(50);
            }
        }
        if (agent_window != nullptr)
        {
            PostMessageW(agent_window, clipper::kShowSettingsMessage, 0, 0);
        }
        CloseHandle(mutex);
        return 0;
    }

    clipper::CaptureAgent agent(instance);
    if (!agent.Initialize())
    {
        CloseHandle(mutex);
        ShowStartupError();
        return 1;
    }

    const bool show_settings =
        std::wstring(GetCommandLineW()).find(L"--agent") == std::wstring::npos;
    const int result = agent.Run(show_settings);
    CloseHandle(mutex);
    return result;
}
