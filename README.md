# WinClipper

WinClipper is a Windows x64 utility that confines the mouse cursor to selected foreground windows.
It runs as a background agent without a tray icon.

## Use

- Launch `win-clipper.exe` to open settings.
- Add open windows with `+`. Matching uses executable path and window class.
- Remove saved targets with `x`.
- Cursor confinement applies only while saved target is foreground.
- Press `Ctrl+Win+Enter` to toggle capture.
- Closing settings destroys its window; background agent keeps running.
- Launch `win-clipper.exe` again to reopen settings in existing agent.
- Removing last target releases cursor and leaves agent idle.
- `Start with Windows` controls per-user startup.
- Configuration lives at `%APPDATA%\WinClipper\config.txt`.

These processes are ignored by default: `explorer.exe`, `pwsh.exe`,
`powertoys.quickaccess.exe`, `steamwebhelper.exe`, `textinputhost.exe`, and
`windowsterminal.exe`.

## Limits

- Agent runs in interactive user session, not as an SCM Windows service.
- Protected applications and anti-cheat systems may block or reject its user-mode hooks.

## Build

~~~text
cmake -S . -B build -G Ninja
cmake --build build --config Release
~~~

CI checks formatting, clang-tidy, Cppcheck, and x64 compilation.
