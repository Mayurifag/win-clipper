#include "config.h"

#include <windows.h>

#include <cstdlib>
#include <string>

namespace
{

void Check(bool condition)
{
    if (!condition)
    {
        std::abort();
    }
}

void TestTargetDeduplication()
{
    clipper::Config config;
    clipper::AddTarget(config, {L"C:\\Apps\\Game.exe", L"GameWindow"});
    clipper::AddTarget(config, {L"c:\\apps\\game.EXE", L"gamewindow"});
    Check(config.targets.size() == 1);
}

void TestBlacklist()
{
    clipper::Config config;
    Check(clipper::IsBlacklisted(config, L"C:\\Windows\\Explorer.EXE"));
    Check(!clipper::IsBlacklisted(config, L"C:\\Apps\\Game.exe"));
}

void TestRoundTrip()
{
    wchar_t temp_path[MAX_PATH]{};
    const DWORD length = GetTempPathW(MAX_PATH, temp_path);
    Check(length > 0 && length < MAX_PATH);

    const std::wstring path = std::wstring(temp_path, length) + L"win-clipper-config-test.txt";
    clipper::Config expected;
    expected.start_with_windows = false;
    expected.blacklist = {
        L"explorer.exe",
        L"pwsh.exe",
        L"powertoys.quickaccess.exe",
        L"steamwebhelper.exe",
        L"textinputhost.exe",
        L"windowsterminal.exe",
    };
    clipper::AddTarget(expected, {L"C:\\Apps\\Game\tBuild.exe", L"Game\\Class"});
    Check(clipper::SaveConfig(expected, path));

    clipper::Config actual;
    Check(clipper::LoadConfig(actual, path));
    Check(!actual.start_with_windows);
    Check(actual.targets.size() == 1);
    Check(clipper::TargetMatches(expected.targets[0], actual.targets[0]));
    Check(actual.blacklist.size() == expected.blacklist.size());
    Check(clipper::IsBlacklisted(actual, L"explorer.exe"));
    Check(clipper::IsBlacklisted(actual, L"pwsh.exe"));
    Check(clipper::IsBlacklisted(actual, L"powertoys.quickaccess.exe"));
    Check(clipper::IsBlacklisted(actual, L"steamwebhelper.exe"));
    Check(clipper::IsBlacklisted(actual, L"textinputhost.exe"));
    Check(clipper::IsBlacklisted(actual, L"windowsterminal.exe"));
    DeleteFileW(path.c_str());
}

} // namespace

int main()
{
    TestTargetDeduplication();
    TestBlacklist();
    TestRoundTrip();
    return 0;
}
