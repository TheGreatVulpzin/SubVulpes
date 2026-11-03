# Launcher redesign: port from the mockup

The mockup is `tools/launcher-mockup/Mockup.cs`. It uses fake data and shows the new look.
The real launcher has the same look and uses real data.

## Files

- `Launcher.cs`: the window, the server process, the log, updates, the plugin tools and the approval dialog.
- `LauncherUi.cs`: the theme, the Lucide icons and the flat controls.
- `LauncherUiMore.cs`: more controls (avatar, list entry, number list) and `Skin`, which paints a theme on the window.
- `LauncherServer.cs`: the Server page (buttons, numbers, graph, console, command box).
- `LauncherSettings.cs`: Settings with three sub-tabs, the Save bar and Past Logs.
- `LauncherPlugins.cs`: the plugin table, the detail panel (Summary, Info, Settings) and the Catalog tab.
- `LauncherHelp.cs`: the Help page. The topics are built into the program.

`LauncherForm` is one `partial` class. Add a file to `tools/launcher/build.bat` and to `CMakeLists.txt`.

## Where things moved

- Past Logs, their notes, Open Folder and Pack are in Settings, Launcher Settings.
- The Mark This Moment button, its hotkey and the marks file are gone.
- Capture options, the preset and the memory dump are in Settings, Advanced Settings. That tab shows only in Debug Mode (Ctrl+Shift+D).
- Theme and accent color are in Launcher Settings. They apply at once and the launcher remembers them.

## Differences from the mockup

- The Save bar (Save, Revert changes, Reset to defaults) belongs to Server Settings only. Launcher and Advanced settings apply at once, as before.
- Revert changes loads the saved values again. Reset to defaults resets the port, the upload limit, the mode, the player limit and the round time. It does not touch the name, the welcome text or the password.
- The Catalog tab has no online source yet. It says so and offers Install from File.
- The plugin table has no Source column. The launcher does not know where a plugin came from.
- A plugin risk comes from its runtime and capabilities: native is highest, any capability besides read, storage and config is high, the rest is low.
- The plugin approval dialog keeps the standard look. Its tests and its security checks did not change.
- The Help text is built into the program, not read from `docs/*.md`.

## Test aids

- `--page Plugins` or `--page Settings:1` opens a page when the window shows. The number is the sub-tab.
- `--start` starts the server when the window shows.
