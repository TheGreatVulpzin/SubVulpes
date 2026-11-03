# Launcher redesign: port from the mockup

The mockup is `tools/launcher-mockup/Mockup.cs`. It uses fake data and shows the new look.
The real launcher has the same look and uses real data.

## Files

- `Launcher.cs`: the window, the server process, the log, updates, the plugin tools and the approval dialog.
- `LauncherUi.cs`: the theme, the Lucide icons and the flat controls.
- `LauncherUiMore.cs`: more controls (avatar, list entry, number list) and `Skin`, which paints a theme on the window.
- `LauncherServer.cs`: the Server page (buttons, numbers, graph, console, command box).
- `LauncherSettings.cs`: Settings with four sub-tabs (Server, Launcher, Past Logs, Advanced) and the Save bar.
- `LauncherPlugins.cs`: the plugin table, the detail panel (Summary, Info, Settings) and the Catalog tab.
- `LauncherHelp.cs`: the Help page. `help.md` holds its text.

`LauncherForm` is one `partial` class. Add a file to `tools/launcher/build.bat` and to `CMakeLists.txt`.

## Where things moved

- Past Logs, their notes, Open Folder and Pack are in Settings, on their own tab (Past Logs).
- The Mark This Moment button, its hotkey and the marks file are gone.
- Capture options, the preset and the memory dump are in Settings, Advanced Settings. That tab shows only in Debug Mode (Ctrl+Shift+D).
- Theme and accent color are in Launcher Settings. They apply at once and the launcher remembers them.

## Differences from the mockup

- The Save bar (Save, Revert changes, Reset to defaults) belongs to Server Settings only. Launcher and Advanced settings apply at once, as before.
- Revert changes loads the saved values again. Reset to defaults resets the port, the upload limit, the mode, the player limit and the round time. It does not touch the name, the welcome text or the password.
- The Catalog tab reads `catalog.json` in the server folder. There is no online source yet. The file is a list of objects:
  `[{"id": "sv.bans", "name": "Bans", "description": "...", "author": "SubVulpes", "version": "0.1.0", "source": "Official", "file": "plugins-pending/sv.bans.svplugin"}]`.
  `source` is Official or Community. `file` is a package, as a path in the server folder or an absolute path. Install starts the normal review. A card without a package file cannot install.
- A plugin listed in the catalog takes its Source from the catalog.
- The plugin table has a Source column. A plugin whose id starts with `sv.` is Official. Any other is Installed. Risk and Source are hidden at first. Right-click a column title to choose the columns. The launcher remembers the choice.
- A plugin risk comes from its runtime and capabilities: native is highest, any capability besides read, storage and config is high, the rest is low.
- The plugin approval dialog keeps the standard look. Its tests and its security checks did not change.
- The Help text is `tools/launcher/help.md`. The build puts it in the program. Each `# ` line starts a topic.
- The dashboard has 11 rows. Round and Network out are not in the launcher: the server does not report them. The launcher remembers the rows, the graph, the height and the width of the panel.

## Extras

- Right-click a plugin: turn on or off, review approval, copy the id.
- A plugin that needs a restart has a Restart server button in its Summary tab.
- The tab bar shows the player count while the server runs.
- Keys: F5 (read plugins), Ctrl+L (clear console), Ctrl+F (console search), Ctrl+S (save server settings).
- The Help search marks the words that it finds.

## Test aids

- `--page Plugins` or `--page Settings:1` opens a page when the window shows. The number is the sub-tab.
- `--start` starts the server when the window shows.
