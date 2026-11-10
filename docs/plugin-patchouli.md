# Patchouli: Help System

Source and tests: [SubVulpes Workshop](https://github.com/TheGreatVulpzin/SubVulpes-Workshop/tree/main/official/plugins/sv.patchouli).
Build and test this plugin in the Workshop. This page keeps its behavior and settings.

Working ID: `sv.patchouli`. Version: `0.1.0`. This native plugin starts enabled.
It owns `/help` and `/page` for all plugins. It needs read, write and plugin services.
It uses only the public API and no internal record layouts.

## For players

| Command | Effect |
| --- | --- |
| /help | List the commands that you can use, six lines for each page |
| /help N | Show page N of the list |
| /help COMMAND | Show the form, the summary and the permission node of a command |
| /page | Show the next page of the last long text |
| /page N | Show page N of the last long text |

A command appears in `/help` only if its permission node is empty, or the caller holds the node.
`/bmhelp` is the same as `/help`.
The console sees every topic, with no pages.
Each player has one text at a time. A new long text replaces the old one. The text goes away when the player leaves.
The server keeps a copy of each text, and 256 players can hold a text at once. If every slot is in use, `pages` returns `SV_LIMIT`. It never takes the text of another player.

## For plugin authors

Include `plugins/patchouli/sv_patchouli.h`. Find `SV_PATCHOULI_API` with `SvPlugins.borrow` or, with a dependency, `SvPlugins.find`.
The version is `SV_PATCHOULI_VERSION` (1). Check the table size before each call.
Every function takes the owner of the caller as its first argument.

| Method | Effect |
| --- | --- |
| topic(owner, record) | Add one command to `/help`. Strings are copied |
| forget(owner) | Remove all topics of the caller. Call it in the disable callback |
| pages(owner, player, title, text) | Show a text in pages. The first page goes out at once |

Topic limits: a command of 1 to 23 lowercase letters, digits, underscores or hyphens (no slash),
a usage of 0 to 47 printable ASCII bytes, a summary of 1 to 79 bytes and a node of 0 to 39 bytes.
The same owner and command replace the old topic. At most 1024 topics exist.
A bad record returns `SV_INVALID`. A full list returns `SV_LIMIT`.

Page limits: a title of 1 to 31 printable bytes and a text of at most 16384 bytes. Lines end with one newline.
An empty title or text returns `SV_INVALID`. A longer text returns `SV_LIMIT`.
A player that is not online returns `SV_NOT_FOUND`.
The messages of `pages` use the owner of the caller, so the caller needs the write service.

The service is optional. A plugin that wants `/help` without Patchouli keeps its own `/help`.
Patchouli starts first when it comes before the other plugin in `plugins.ini`.
A plugin that finds Patchouli at its own start must not register `help`. `sv.admcmd` follows this rule.
If another plugin already owns `/help`, Patchouli keeps `/page` and the service, and it does not own `/help`.
The host does not tell a provider when a consumer stops, so each consumer calls `forget` in its disable callback.

## Checks and limits

The native unit test covers topic bounds, replace and limit rules, the page text, the order of pages,
the numbers outside a text, permission filtering, the console list and the leave event.
The SDK scenarios `help` and `help-fallback` load the real DLLs with `sv.admcmd`.
Private messages do not reach the log of the SDK runner, so the scenarios check only the console output.
No original client playtest or manual launcher session ran.
