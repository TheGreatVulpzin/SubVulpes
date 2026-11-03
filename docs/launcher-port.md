# Launcher redesign: port from the mockup

The mockup is `tools/launcher-mockup/Mockup.cs`. It uses fake data and shows the new look.
The real launcher is `tools/launcher/Launcher.cs`. The controls of the new look are in `tools/launcher/LauncherUi.cs`.
Move the pages one at a time. After each step, build the launcher, run the launcher tests and open the window.

## Done

- `LauncherUi.cs` holds the theme, the Lucide icons and the flat controls (button, switch, dropdown, text field, search box, segmented option, badge, tab label, state pill).
- The tab bar at the top of the window draws the tabs. The headers of the `TabControl` stay hidden, so `ShowPage` and `SetDebugMode` work as before.
- The Server page has the state pill, the Start and Stop buttons with icons, the four numbers, the speed graph and the console.
- Past Logs moved to Launcher Settings. Each log has its notes, the Save Notes button, Open Folder and Pack. A finished log appears in the list.
- The Mark This Moment button, its hotkey and the marks file are removed.
- The Debug tab keeps the capture options, the preset and the memory dump button.

## Next, in this order

1. Server page: the console filter (level and text), the icon buttons in the console corner, and the stats list with the gear (the four numbers stay as a row until then).
2. Settings: one page with sub-tabs (Server, Launcher, Advanced), the text fields and dropdowns of the new look, and one bar with Save, Revert changes and Reset to defaults. The `dirty` logic stays.
3. Plugins: a table with the state dot, the risk pill and the switch in the list, a detail panel with a fixed header and the tabs Summary, Info and Settings, and the Catalog tab.
4. Help: topics from `docs/*.md`, drawn in the launcher.
5. Theme and accent color: a dark theme and a choice of accent color in the launcher settings.

## Open questions

- Windows style of the standard controls (`Button`, `TextBox`, `ComboBox`) stays on the pages that are not ported yet.
