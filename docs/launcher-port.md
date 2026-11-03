# Launcher redesign: port from the mockup

The mockup is `tools/launcher-mockup/Mockup.cs`. It uses fake data and shows the new look.
The real launcher is `tools/launcher/Launcher.cs`. The controls of the new look are in `tools/launcher/LauncherUi.cs`.
Move the pages one at a time. After each step, build the launcher, run the launcher tests and open the window.

## Done

- `LauncherUi.cs` holds the theme, the Lucide icons and the flat controls (button, switch, dropdown, text field, search box, segmented option, badge, tab label, state pill).
- The tab bar at the top of the window draws the tabs. The headers of the `TabControl` stay hidden, so `ShowPage` and `SetDebugMode` work as before.
- The Server page has the state pill and the Start and Stop buttons with icons.

## Next, in this order

1. Server page: the console filter (level and text), the icon buttons in the console corner, the stats list and the tick rate graph (the data is in `speeds`, `started` and the player count).
2. Settings: one page with sub-tabs (Server, Launcher, Advanced), the text fields and dropdowns of the new look, and one bar with Save, Revert changes and Reset to defaults. The `dirty` logic stays.
3. Plugins: a table with the state dot, the risk pill and the switch in the list, a detail panel with a fixed header and the tabs Summary, Info and Settings, and the Catalog tab.
4. Help: topics from `docs/*.md`, drawn in the launcher.
5. Theme and accent color: a dark theme and a choice of accent color in the launcher settings.

## Open questions

- Where do Past Logs, Mark This Moment, Pack This Log and the notes go? The mockup has no place for them. They are for tests (the Debug tab).
- Windows style of the standard controls (`Button`, `TextBox`, `ComboBox`) stays on the pages that are not ported yet.
