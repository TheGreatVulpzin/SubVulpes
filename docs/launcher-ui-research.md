# Launcher UI research

This page lists the patterns that other server programs use. It holds no copied screens.
The notes come from image searches and project pages in November 2026. Treat them as observations, not as a test of each program.

## Programs that were looked at

| Program | Kind | What the main screen shows |
| --- | --- | --- |
| Pterodactyl, Crafty Controller | Web panel for game servers | A dark page with a menu on the left, tabs at the top (console, files, settings), a large console, and small blocks with CPU, memory and network numbers and mini graphs. Start, Restart and Stop (and Kill) are large buttons above the console |
| WGS (Windows Game Server) | Windows desktop program | A list of servers, a console, CPU and RAM charts of the last 6 minutes, and an advanced section (firewall rules, process priority, memory limit) |
| Ronin Server Manager, V Rising launcher | Windows desktop program | One-click Start, Stop and Restart, a status line and a log viewer. Stop sends the stop command first. A hung server can be ended from the window |
| XAMPP Control Panel | Windows desktop program | One table of modules (name, process number, port) with Start, Stop, Config and Logs on each row, and a log pane under it. Everything is on one screen |
| Uptime Kuma | Web monitor | A row of counters (Up, Down, Unknown), a list of services with a colored status and a small history bar, and a graph per service |
| Docker Desktop, Portainer | Desktop and web | A table of containers with status and numbers, and a page for logs and statistics of the chosen one |

## Patterns that repeat

1. **The state and the speed are on the first screen.** The owner sees if the server is up, its load and its player count without a click.
2. **The console is the main part of the page.** Statistics sit beside it or above it, as small blocks.
3. **One group of controls.** Start, Stop and Restart sit together. A separate button ends a hung process (Kill), and it has a warning color.
4. **A graceful stop is the default.** The program gives the server time to save before it ends it.
5. **Uptime shows early.** The time since start is visible, also while the server starts.
6. **Advanced options live deeper.** Firewall, priority, memory limits and capture tools are in an Advanced section or a second level.
7. **A dark theme is common.** Most of the web panels default to it.
8. **A status uses a color and a word.** A color alone is not enough for some users.
9. **Most pages save at once.** A change in a web panel applies when the field changes, or after one `Save` for the whole page. A warning on close is rare.

## What it means for the SubVulpes launcher

- Move the speed (ticks per second), the player count and the time since start to the first tab. Today they are in the Debug tab.
- Put `Save Log` on the first tab, beside the console.
- Add a `Kill` button, away from `Stop`, for a server that does not stop in 15 seconds.
- Keep captures, past logs and memory dumps in a second-level tab (`Advanced`).
- Group the settings by topic and save a changed field by itself (see the note in [ideas](ideas.md)).
- Keep Windows Forms, and add one `Theme` class so a dark theme costs little.
