# Launcher UX audit and proposed layout

Status: audit and proposed layout. Main navigation now has a saved Tabs / Sidebar switch.
The review covers the current `tools/launcher/Launcher.cs`, including the new approval dialog.
It is a source review. A live keyboard, display scaling and visual check still needs to run.

## What is hard to use

| Area | Current behavior | Effect |
| --- | --- | --- |
| Navigation and context | Start, Stop and server status are on Server. The selected server folder is on Launcher settings. | The operator cannot see the target folder or server state while working in Plugins. |
| Tab order | The main form relies on control creation order. Only the new approval dialog sets explicit tab indices. | Page navigation and the path from a plugin row to its action need a defined keyboard sequence and a live check. |
| Wording | `Undo` reloads saved settings. `Frozen?` follows a missing status update. The plugin list uses technical state and reason strings. | The labels do not state the effect or the next action. Units such as bytes per second are mainly in tooltips. |
| Errors | `ChangePlugin` sets its result, then `RefreshPlugins` replaces the message. Other errors append raw exception text to one shared label. | Enable/disable failures can disappear. Long PowerShell errors can dominate the page without a short recovery action. |
| Long operations | Plugin installation now has stage messages. Update discovery has no initial progress message. Download shows a release name. Stop waits up to 15 seconds without a countdown. Closing during an update or plugin action silently cancels the close. | The operator cannot distinguish a wait from a stalled operation or know when a safe cancel is possible. |
| Plugin list | Six columns total 1,085 pixels. The window starts at 1,000 pixels and permits 820 pixels. Name, ID and a 490-pixel reason column share the row. | Important details need horizontal scrolling. Author, description and capabilities have no persistent selected-item view. |
| Plugin actions | The action bar is flat. `SetState` does not disable its local buttons, although their handlers return while busy. Required plugins can still present a Disable action. | Actions can look available and do nothing, or fail only after selection. The command box also competes with routine plugin controls. |

The new review already combines installation and approval.
It shows capability colors, version changes and explicit native consent.
Keep this shared workflow and its tests in the redesign.
Task 3 also preserves installation and approval results after refresh.
Apply the same result handling to the remaining plugin actions in a later change.

Source anchors: `LauncherForm`, `ServerPage`, `LauncherSettingsPage`, `PluginsPage`, `SettingsPage`,
`SetState`, `ChangePlugin`, `RefreshPlugins`, `CheckForUpdates`, `StopServer` and `OnClosing`.

## Proposed layout

Use the saved navigation choice. Tabs remain the default.
Add a persistent context header and operation footer.
Use a compact list with selected-plugin details below it.
The example shows the Plugins page.

```text
SubVulpes Server
Server folder: C:\SubVulpes                         [Browse]
[Start] [Stop]  State: Stopped  Players: --  Last status: --

[Server] [Server settings] [Plugins] [Launcher settings] [Diagnostics]

Plugins
[Install / update]  [Refresh]

Plugin                         Version       State
Health Commands                0.2.0         Enabled for next start
Server Status                  0.1.0         Required

Selected plugin: Health Commands
ID: sv.health        Author: Example Author
Description and current state reason
Capabilities and approval information
[Enable] [Disable] [Settings] [Review approval]

Operation: Idle
Last result: Health Commands 0.2.0 is installed and approved.
[Details / copy error]
```

Keep the full ID in the selected details and use it for every action.
Show unknown status as unknown, rather than as a zero value.
Keep Start and Stop explicit and enable only valid actions.
The Server page keeps its console. Diagnostics holds captures, dumps and past tests.
An advanced area holds the native bypass and local command entry.

The native review keeps one approval action and the extra native checkbox.
A new capability still needs the full review.
A layout change must preserve the existing hash, capability and consent checks.

## Changes ordered by value

1. Show the selected folder, server state and Start/Stop controls on every page.
2. Keep operation results visible. Add stage, elapsed time and a short recovery action for errors.
3. Reduce plugin columns and add selected-plugin details. Disable invalid actions and explain the reason.
4. Use direct labels such as `Revert unsaved changes` and `No status update for 150 seconds`. Show units beside fields.
5. Define keyboard order, shortcuts and scaling behavior. Check 820-pixel width and 125%, 150% and 200% scaling.

Cancel can stop discovery, reading or downloading.
File replacement must finish or roll back before the launcher closes.
Keep technical details available through a copyable error view.

## Navigation choice

The owner chose a switch between tabs and a sidebar for main navigation.
In **Launcher settings**, use **Navigation** to select **Tabs** or **Sidebar**.
The launcher saves this choice in `launcher.ini` as `navigation=tabs` or `navigation=sidebar`.
Tabs are the default. The choice takes effect without a restart.

Both modes use the same pages and controls.
Switching preserves the selected page, unsaved fields and nested Debug tabs.
The sidebar shows horizontal labels and uses system selection colors.
The focused launcher test checks layout, repeated switches, saved choice, draft fields and hidden Debug behavior.
The normal build and required checks pass. No live visual check ran.

The broader header, plugin details, console and diagnostics changes remain proposals.

## Open choices for the owner

- Console: keep it on Server, or make it a drawer available from every page.
- Diagnostics: keep it behind an explicit advanced toggle, or give it a permanent navigation entry.

The proposed header, operation feedback and plugin details work with either navigation mode.
The broader visual redesign waits for the owner's choice.
