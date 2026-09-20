# WinClipper

Native Windows x64 cursor capture agent.

## Behavior

- First launch opens settings and registers per-user startup.
- Closing settings destroys window; background agent keeps running without a tray icon.
- Launching `win-clipper.exe` again recreates settings window in existing agent.
- Add open windows by executable path plus window class with `+`.
- Remove saved targets with `x`.
- Saved targets capture only while foreground.
- Removing last target releases `ClipCursor` immediately; agent remains idle.
- `Ctrl+Win+Enter` toggles capture.
- `explorer.exe`, `pwsh.exe`, `powertoys.quickaccess.exe`, `steamwebhelper.exe`, `textinputhost.exe`, and `windowsterminal.exe` are blacklisted by default.
- Configuration lives at `%APPDATA%\WinClipper\config.txt`.

## Build

~~~text
cmake -S . -B build -G Ninja
cmake --build build --config Release
ctest --test-dir build --output-on-failure
~~~

Build uses only Win32 APIs. Static runtime linking produces one x64 executable without an installer.

## Architecture

- `CaptureAgent`: low-level mouse hook, WinEvent hook, hotkey, and runtime target matching.
- `window_catalog`: visible-window enumeration and executable/class identity.
- `config`: UTF-8 persistence and target/blacklist matching.
- `SettingsWindow`: Win32 selector and target list editor.
- `startup`: HKCU logon registration without administrator privileges.

Agent runs in user session instead of as an SCM service. Windows services run in Session 0 and cannot reliably own interactive desktop hooks.

The capture path follows original `clipper.asm`: client-rectangle conversion, foreground gating, and `ClipCursor`. The original MASM32 x86 binary cannot be linked into an x64 executable, so behavior is ported to native Win32 C++.
