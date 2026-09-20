# WinClipper

Keeps the mouse cursor inside selected foreground windows. Runs without a tray icon.
Useful for windowed games which don't do same thing for you.

## Usage

Closing settings leaves agent running; run executable again to reopen them.
Config: `%APPDATA%\WinClipper\config.txt`.

## Limits

- Agent runs in interactive user session, not as an SCM Windows service.
- Protected applications and anti-cheat systems may block or reject its user-mode hooks (not tested at all)
- Ignored by default: `explorer.exe`, `pwsh.exe`,
`powertoys.quickaccess.exe`, `steamwebhelper.exe`, `textinputhost.exe`, and
`windowsterminal.exe`.
