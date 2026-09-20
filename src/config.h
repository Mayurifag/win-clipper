#pragma once

#include <string>
#include <vector>

namespace clipper
{

struct Target
{
    std::wstring executable_path;
    std::wstring window_class;
};

struct Config
{
    bool start_with_windows = true;
    std::vector<Target> targets;
    std::vector<std::wstring> blacklist = {
        L"explorer.exe",
        L"pwsh.exe",
        L"powertoys.quickaccess.exe",
        L"steamwebhelper.exe",
        L"textinputhost.exe",
        L"windowsterminal.exe",
    };
};

std::wstring NormalizeValue(std::wstring value);
std::wstring FileName(const std::wstring& path);
Target NormalizeTarget(Target target);
bool TargetMatches(const Target& left, const Target& right);
bool IsBlacklisted(const Config& config, const std::wstring& executable_path);
void AddTarget(Config& config, Target target);

std::wstring ConfigPath();
bool LoadConfig(Config& config, const std::wstring& path);
bool SaveConfig(const Config& config, const std::wstring& path);

} // namespace clipper
