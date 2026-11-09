# Aya: Logging System

Working ID: `sv.ayalog`. Version: `0.1.0`. This native plugin starts enabled.
It writes one fixed log line for each action that other plugins record, and it keeps the last 128 lines in memory.
It needs the read, write, configuration and plugin services. It uses only the public API.

## Why it is a plugin

Plugins that change the game, such as `sv.admcmd`, should not each invent a log format.
They call this service. The administrator reads one format, and a later audit plugin can read the same lines.
The service is optional. A plugin that cannot find it works the same and records nothing.

## The line

```
ACTION by NAME (acc N) target NAME (acc N) result OK
ACTION by console result FAIL SV_CONFLICT(4) DETAIL
```

- The host adds the ID of the calling plugin in front, for example `[sv.admcmd]`.
- `acc N` is the account number of the game, not the account ID of the client. A bot has no account number.
- The console is `console`. A player that left is `a player that left`.
- A failure shows the name of the result and its number. The detail is optional text from the caller.
- The log never holds a password, a token or an address.

## For players

| Command | Node | Effect |
| --- | --- | --- |
| /logs [COUNT] | logging.view | Show the last COUNT lines, oldest first. COUNT is 1 to 100 and the default is 10 |

With [Patchouli](plugin-patchouli.md) active, a player reads long answers in pages with `/page`.
The console sees all lines at once. The memory holds 128 lines and the oldest line goes first.
The lines go away when the plugin stops. The server log keeps them.

## Settings

| Key | Default | Effect |
| --- | --- | --- |
| record_actions | 1 | Record actions that worked and that the calling plugin sends |
| record_failures | 1 | Record actions that failed |

Both settings apply live. A kind that is off is not an error for the caller.

## For plugin authors

Include `plugins/ayalog/sv_ayalog.h`. Find `SV_LOGGING_API` with `SvPlugins.borrow`, in each callback.
The version is `SV_LOGGING_VERSION` (1). Check the table size before the call.

| Method | Effect |
| --- | --- |
| record(owner, entry) | Write one line to the server log and keep it for `/logs` |

The record `SvLogEntry` has these fields:

| Field | Rule |
| --- | --- |
| size | `sizeof` the record |
| kind | `SV_LOG_ACTION` for a result of `SV_OK`, and `SV_LOG_FAILURE` for any other result |
| action | 1 to 23 bytes: lowercase letters, digits, underscore or hyphen |
| actor | The player that acted, or 0 for the console |
| target | The player that was the target, or 0 for none |
| result | The result of the action |
| detail | 0 to 79 printable ASCII bytes, or NULL |

A bad record returns `SV_INVALID`. This includes a kind that does not fit the result.
The service reads the names with the owner of the caller, so the caller needs the read service.

## What `sv.admcmd` records

Every failure is recorded. These commands are recorded when they work too: `/heavy`, `/give`, `/round`, `/kill`, `/tpall` and `/delh`.
The detail is empty today, because the plugin does not know which call of the game failed. The result name and number are in the line.

## Checks and limits

The native unit test covers the line form, bad records, the settings, the ring of 128 lines and the permission of `/logs`.
The SDK scenario `logging` loads the real DLLs with `sv.admcmd`.
No original client playtest or manual launcher session ran.
