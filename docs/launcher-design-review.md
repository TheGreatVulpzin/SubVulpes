# Launcher design review

Date: 2026-11-05. Status: the review is done, and the first pass of changes is in the launcher (see "Applied on 2026-11-05").

## Goal

The owner set the target on 2026-11-05.
The launcher does not need a top modern look. A classic look is welcome, even one near Windows 7 or older.
The launcher must not look like raw Windows Forms: gray system dialogs, default buttons and blue links.

The target is **classic, calm and consistent**. The question for each element is "does it look chosen?"
It is not "is it modern?".

## Method

- Screens of both themes: Server, Plugins (three detail tabs), Workshop (list and plugin page), Settings (three sub-tabs) and Help.
- The two install dialogs, drawn outside the main window.
- A source review of `tools/launcher/*.cs`: fonts, radii, paddings, icons, keyboard code and DPI code.
- Contrast numbers computed from the theme colors with the WCAG formula.
- References: Prism Launcher and Modrinth App (seen in their own screenshots), the Microsoft Windows 11 guidance, and public lists of "AI slop" signs.

Limits: the screens come from a stopped server and from test data. No screen reader and no keyboard-only run was done.
The "AI slop" lists are blog posts, not measured studies. Use them as a checklist and not as proof.

## What reads as "made by AI", and what does not

| Sign in the lists | In the launcher |
| --- | --- |
| Purple or indigo gradients | No. The accent is orange, which fits the fox name. |
| Inter font, unchosen | No. The font is Segoe UI, the system font. That is a correct choice for Windows. |
| Three rounded cards in a row | No. |
| Cards inside cards | Partly. The Server page has a rounded frame inside the window. The other pages do not. |
| Pills and badges everywhere | Yes. The state pill, the Official and Community chips, and the restart badge. |
| Gradient under a chart, and a dark value bubble | Yes. The tick chart has both. |
| An icon on every button | Yes. All four server buttons and most Plugins buttons have an icon and a word. |
| Small info icons next to labels | Yes. One for each setting that has help. |
| Muted gray text for most secondary text | Yes. It is correct for the contrast on the dark theme. It is weak on the light theme. |
| Cool blue-gray neutrals with one warm accent | Yes. It is the common "slate plus orange" pair. |

Summary: the launcher avoids the loud signs. Its sign of origin is **sameness**.
Every box has the same border and radius, every state is a pill, and every action has an icon.
A person who designs by hand makes some things loud and some things quiet. Here almost everything has the same weight.

## What is not a problem

- The layout of the Server page (state, numbers and chart on top, console below). Server panels for games use the same order.
- Segoe UI as the only family.
- The orange accent. It is the one choice that is clearly the project's own.
- The Help page. It has the most "hand made" feel, with a list at the left and plain text.
- Dense rows in the Plugins table.

## Element by element

"Intent" is what the element is for. "Reason" is why it looks like it does. Verdict: keep, adjust or change.

### Window and navigation

| Element | Intent and reason | Verdict |
| --- | --- | --- |
| Window title "SubVulpes Server Launcher - v0.2.0 Kit" | Tell which program and which version. The name Kit is the version name. | Keep. The title is fixed by `CLAUDE.md`. |
| Top tab bar (Server, Plugins, Settings, Help) | Four pages in one row. The bar color is a stronger tone than the window, so it reads as a bar. | Keep. |
| Server name at the right of the bar | Shows which server this launcher controls, and the player count. | Adjust. It is bold gray text with no label. It reads like a user name. Add a small "Server:" label, or a dot for the state. |
| Sub-tab strip (My Plugins, Workshop, and the four Settings tabs) | A second level of pages. | Adjust. It uses the same orange underline and bold text as the top bar. Two levels look the same. Make the second level lighter. |
| Third tab level in the plugin detail (Summary, Info, Settings) | Three views of one plugin. | Adjust. A third orange-underline level on one screen is too much. A plain text row or a segmented control would do. |
| Status bar at the bottom | A message line. It now has its own tone and the message fades after 9 seconds. | Keep. Consider two segments like Prism Launcher: message at the left, a fixed fact (version or folder) at the right. |

### Server page

| Element | Intent and reason | Verdict |
| --- | --- | --- |
| State pill "Stopped" | Show the state with a color and a word. A color alone is not enough. | Adjust. Keep the word and the dot. Remove the pill shape. |
| Start, Stop, Restart, Kill | One group for the life of the server. Kill is last because it is the last resort. | Adjust. Remove the icons, because the words say the same. When the server is stopped, three of four buttons are gray. Keep them in place, but make the disabled look quieter. |
| Stats list (Uptime, Tick rate, Players, ...) | The numbers that an owner checks first. | Keep. The row "Tick rate" repeats the chart title. Hide one of them. |
| Tick chart | Show if the server keeps its speed. One point each minute, 20 minutes. | Adjust. The scale is now even. Remove the gradient fill and the dark bubble. Write the current value as text at the corner. |
| Gear icon in the chart panel | Choose which rows and which graph show. | Adjust. The icon floats in a corner and does not say what it does. Give it a tooltip and a label, or move it to the stats header. |
| Rounded frame around the whole Server page | None that is written down. It comes from the mockup. | Change. No other page has it. Remove it, or give every page the same frame. |
| Console | The main part of the page. Black, with four icon buttons (expand, copy, clear, save). | Adjust. The four icons are low contrast on black. |
| Level filter, search, command row | Find lines and send commands. | Keep. |

### Settings pages

| Element | Intent and reason | Verdict |
| --- | --- | --- |
| Section titles (General, Network, Game) | Group the fields. | Change. They are light and the field labels are bold, so the labels look stronger than the titles. Reverse it: semibold titles, regular labels. |
| Field labels in bold | Make a label easy to find. | Adjust. Use regular weight. Semibold is the Windows way to stress. |
| Info icons between label and field | Hold the help text without making the page long. | Adjust. The icons make a column of empty space. Put a short gray line of help under the label instead. A hover target is hard to find. |
| Wide gap between label and field | Align all fields in one column. | Adjust. The label column is 270 pixels wide. 200 is enough. |
| Save, Revert changes, Reset to defaults | Edit, then save in one step. Reset is a last resort and sits apart. | Keep. |
| "Restart required" badge | Tell that a setting needs a restart. It shows only after a real change. | Keep. |
| Launcher Settings rows (folder, updates, theme, accent, version, debug) | The settings of the program, and not of the server. | Keep. |
| Past Logs, Advanced Settings | Second-level tools. | Keep. Past Logs has a large empty area when no test exists. Show a sentence that says why it is empty. |

### Plugins and Workshop

| Element | Intent and reason | Verdict |
| --- | --- | --- |
| Plugins table (Name, Version, State, On) | See and switch plugins. Risk, Source and Author are hidden by default. | Keep. |
| State dot and word | The same reason as the server state. | Keep. |
| On switch (green) | A fast way to turn a plugin on or off. | Adjust. Green is also the state color. Use the accent color, or make the switch neutral when on. |
| Letter avatar | A fallback until a plugin has an icon. | Keep. Every avatar has the same orange tint, so the list has no variety. Use a few tones that come from the id. |
| Detail panel (name, version, tabs) | The details of the chosen plugin. | Adjust. See the tab note above. |
| Install, Update, Uninstall buttons | One width, in the card. | Keep. |
| "Official" and "Community" chips | The seal of the source. A trust signal. | Adjust. Keep them, but as small square tags and not as pills. |
| Workshop cards | Browse with the icon, name, source, short text and one action. | Keep. This is the right pattern for a catalog. |
| Plugin page with pictures | The long view, like a mod site. | Keep. Not seen with real pictures yet. |

### Dialogs

| Element | Intent and reason | Verdict |
| --- | --- | --- |
| Full approval dialog (review of a file) | The owner reads what a plugin can do and allows native code. A security step. | **Change.** It is raw Windows Forms: system gray, default button, a blue underlined "Cancel", a plain box. It ignores the theme. On the dark theme it is a bright gray window. This is the one place that looks unfinished. |
| Short community card | The short version for a plugin from the community. | **Change.** Same problem. It has the same raw look. |
| Message boxes (Uninstall, Reset) | Ask for a yes or no. | Keep. A system message box is acceptable. |

### Behavior that affects the look

| Finding | Detail |
| --- | --- |
| The custom controls have no keyboard code and no focus ring | The buttons, switches, drop-downs, tabs and icon buttons are painted by hand. The code has no key handler and no focus drawing, except in text fields. Tab reaches them, but Space and Enter do nothing and the focus is invisible. |
| No accessible names | No control sets an accessible name. A screen reader reads nothing useful. |
| Contrast on the light theme | Gray secondary text is 4.65 (passes). Green text is 4.35, the white text on the Start button is 4.35, the orange text is 3.65 and the amber text is 3.64. The target for small text is 4.5. The dark theme passes everywhere (5.9 to 6.8). |
| Type scale | 13 sizes and weights, from 8 to 19 points. Most text is 9 or 9.5 points. Only Regular and Bold exist. |
| Shape | Radius 5 and 6 pixels on buttons, fields and cards, 8 on a few, and full pills. Windows 11 uses 4 pixels for controls and 8 for windows and menus. |

## References

**Prism Launcher** (a Qt program, dark theme, [screens](https://prismlauncher.org/)).
It looks classic: a toolbar with a small icon and a word, a list of items, a column of actions at the right, a two-part status bar.
Its dialogs follow the theme of the main window. It is dense and calm. It is also well liked, so a classic look does not hurt.
Lesson: consistency (the dialogs match) matters more than a modern style.

**Modrinth App** ([page](https://modrinth.com/app)).
It is the modern end: a dark page, an icon rail, rounded cards, green pills ("New instance", online counts), soft glows.
It shows how pills and rounded boxes pile up. The SubVulpes launcher is much nearer to this than to Prism.
Lesson: pick the quiet side if the goal is a classic look.

**Microsoft, Windows 11 guidance.**
[Typography](https://learn.microsoft.com/windows/apps/design/signature-experiences/typography): Segoe UI Variable, a short ramp (12, 14, 18, 20, 28, 40 pixels), and Semibold instead of Bold.
[Geometry](https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/geometry): 4 pixels for controls, 8 pixels for windows and menus.
A classic look can use the same ideas with 2 or 3 pixels of radius. The value matters less than the use of one value.

**Lists of "AI slop" signs.**
[Noqta](https://noqta.tn/blog/ai-design-slop-overused-ui-patterns-fix-2026) and [925 Studios](https://www.925studios.co/blog/ai-slop-design-tells).
They agree on gradients, unchosen fonts, cards in cards, hairline borders with soft shadows, and icons for everything.
One of them says that no single sign is bad. The absence of a choice is the sign.

## Direction: "classic, calm, consistent"

1. **One radius.** 3 pixels for controls and cards, no pills. This gives the older, flatter look that the owner accepts, and it removes the pill sign.
2. **Three levels of text.** Section title (semibold, 11 points), body and label (regular, 9.5 points), help text (regular, 8.5 points, gray). Replace Bold with Semibold.
3. **Weight by rank.** Few loud things: the Start button, the state, the selected tab. Everything else is quiet.
4. **Icons only where they carry meaning.** The console tools, the Workshop and the file actions. No icon on Start, Stop, Restart and Kill.
5. **Themed dialogs.** The two install dialogs use the same colors, buttons and fonts as the main window.
6. **One page frame.** Every page has the same frame, or none has one.
7. **Real keyboard and focus.** Space and Enter work on the custom controls, the focus shows, and each control has an accessible name.
8. **Contrast.** Darker green, orange and amber for text on the light theme, so that every small text passes 4.5.
9. **Warm neutrals.** Gray with a little beige in place of blue-gray, to go with the orange. This is the most subjective item.

## Order of work

| # | Change | Effort | Risk |
| --- | --- | --- | --- |
| 1 | Themed install dialogs (item 5) | 2 hours | Low. The review steps and the consent box stay. |
| 2 | Keyboard, focus and accessible names (item 7) | 3 hours | Medium. Test by hand. |
| 3 | Contrast of the light theme (item 8) | 30 minutes | Low. |
| 4 | Type scale and weights (items 2 and 3) | 1 hour | Low. |
| 5 | One radius, no pills, state dot (item 1) | 1 hour | Low. |
| 6 | Chart without gradient and bubble | 40 minutes | Low. |
| 7 | One page frame (item 6) | 30 minutes | Low. |
| 8 | Icons only where they help (item 4) | 30 minutes | Low. |
| 9 | Warm neutrals (item 9) | 30 minutes | Low. |

## Questions for the owner

- Is a flatter and squarer look (3 pixel radius, no pills) acceptable? It matches the "classic" goal. It also changes the look the most.
- The Server page frame: remove it, or add it to all pages?
- Should the second and third tab levels use a quieter style?
- Warm neutrals: yes or no?

## Applied on 2026-11-05

The owner chose: a flatter look, no frame on the Server page, quieter tabs, a small change of color, and pills adjusted.

| Change | Where |
| --- | --- |
| Warm neutrals in both themes. Every text color passes a contrast of 4.5. The Start button has its own darker green with white text. | `Theme` in `LauncherUi.cs`, and a test |
| An accent for text (`AccentFg`) that follows the accent color that the user picks | `Theme.SetAccent`, and a test |
| One radius: 3 pixels for controls, 4 for panels. The Official and Community tags, the restart badge and the risk tag are square. | `UI.Radius`, `UI.PanelRadius` |
| The server state is a dot and a word, with no pill | `StateLabel` |
| Segoe UI Semibold in place of Bold. Setting labels are regular, and section titles are semibold. | `UI.F`, `LauncherSettings.cs` |
| Help text sits under the label, in small gray text. The "i" icons are gone from the settings. | `AddRow` |
| No frame on the Server page. Start, Stop, Restart and Kill have no icons. | `LauncherServer.cs` |
| Three tab styles: accent underline (main), thin gray underline (second level), text only (plugin details) | `TabLabel` |
| The tick chart is a line with no fill. The current value is text in the corner. | `AreaChart` |
| The two install dialogs use the theme, flat buttons and the same fonts. The risk colors come from the theme. | `PluginApprovalDialog`, `CommunityCard` |
| The custom controls answer Space, Enter and the arrow keys, show a dotted focus line after a key press, and have accessible names and roles | `FlatButton`, `Switch`, `Dropdown`, `Segmented`, `TabLabel`, `IconButton`, `NavItem`, and a test |
| Plugin avatars take one of six tones from the name | `Avatar` |
| Space turns the chosen plugin on or off in the list | `LauncherPlugins.cs` |

Not done, or not checked:

- The "On" switch stays a rounded pill. Windows switches have that shape.
- The Official and Community tags are text labels with a flat fill. They are square, but they are not drawn by the launcher.
- The focus line and the screen reader output were not checked by eye or with a reader. The test checks the key answers and the names.
- The warm neutral values come from a calculation of contrast and from the owner's taste on the screens. They are easy to change in `Theme`.
