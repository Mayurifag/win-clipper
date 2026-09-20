#include "config.h"

#include <algorithm>
#include <cwctype>
#include <utility>

namespace clipper
{

std::wstring NormalizeValue(std::wstring value)
{
    while (value.size() >= 2 && value.front() == L'"' && value.back() == L'"')
    {
        value = value.substr(1, value.size() - 2);
    }

    std::transform(value.begin(), value.end(), value.begin(), [](wchar_t character)
                   { return static_cast<wchar_t>(std::towlower(character)); });
    return value;
}

std::wstring FileName(const std::wstring& path)
{
    const size_t separator = path.find_last_of(L"\\/");
    return separator == std::wstring::npos ? path : path.substr(separator + 1);
}

Target NormalizeTarget(Target target)
{
    target.executable_path = NormalizeValue(std::move(target.executable_path));
    target.window_class = NormalizeValue(std::move(target.window_class));
    return target;
}

bool TargetMatches(const Target& left, const Target& right)
{
    const Target normalized_left = NormalizeTarget(left);
    const Target normalized_right = NormalizeTarget(right);
    return normalized_left.executable_path == normalized_right.executable_path &&
           normalized_left.window_class == normalized_right.window_class;
}

bool IsBlacklisted(const Config& config, const std::wstring& executable_path)
{
    const std::wstring executable = NormalizeValue(FileName(executable_path));
    return std::any_of(config.blacklist.begin(), config.blacklist.end(),
                       [&](const std::wstring& entry)
                       { return executable == NormalizeValue(FileName(entry)); });
}

void AddTarget(Config& config, Target target)
{
    target = NormalizeTarget(std::move(target));
    if (target.executable_path.empty() || target.window_class.empty())
    {
        return;
    }

    if (std::none_of(config.targets.begin(), config.targets.end(),
                     [&](const Target& existing) { return TargetMatches(existing, target); }))
    {
        config.targets.push_back(std::move(target));
    }
}

} // namespace clipper
