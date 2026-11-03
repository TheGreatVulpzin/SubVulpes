# Plugin settings

A setting is a value that the administrator sets for one plugin, for example the price of an item. It is not the storage of the plugin (see [plugin storage](plugin-storage.md)).
The settings of the core are a different thing (see [core settings](plugin-settings.md)).

| | Settings | Storage |
| --- | --- | --- |
| Who writes | The administrator | The plugin |
| Shape | Keys that the plugin declares, with a type and a range | Any bytes, within a size limit |
| Check | The host checks each value before the plugin sees it | The plugin checks its own data |
| Shown to the user | Yes, in the launcher and by command | No |
| On update | Kept for the keys that remain | Kept whole |

## Declaration

The plugin declares each setting in its `plugin.ini`, in a section named `[setting.KEY]`. The host reads the declaration without loading the DLL.
The key has 1 to 32 characters: lowercase letters, digits and underscore. It starts with a letter. A plugin has at most 32 settings.
A wrong declaration makes the manifest invalid, as a wrong `api` value does, and the plugin does not start.

| Field | Meaning |
| --- | --- |
| `type` | `flag`, `int`, `float`, `text`, `choice` or `secret` |
| `default` | The first value. Required. A secret has no default (the field is empty) |
| `min`, `max` | The range of an `int` or a `float`. The default must be inside it |
| `maxlen` | The longest text of a `text` or a `secret`, from 1 to 63. The default is 63 |
| `choices` | A list for a `choice`, with commas and no spaces: 2 to 8 names of 1 to 23 characters (lowercase letters, digits, underscore, hyphen) |
| `label` | The name that the form shows. Required, at most 63 characters |
| `help` | A line of help. Optional, at most 127 characters |
| `live` | `1` when a change applies at once. `0` (the default) when it waits for a restart |
| `admin` | `1` when a player who is an administrator may change it. `0` (the default) for the local operator only |

```ini
[setting.knife_price]
type=int
default=125
min=0
max=1000000
label=Price of the knife
help=Dollars.
live=1
admin=1
```

Text values use printable ASCII with no quote and no semicolon, and with no space at the ends, because the ini file would change them.
A number is a plain number: an `int` has an optional minus and digits. A `float` has an optional minus, digits and at most one point. There is no exponent.
A flag is `0` or `1`. A choice is one of its names.

## Where the values live

The values are in `user/plugins/ID.settings.ini`. This private folder is the same one that keeps the approvals and the plugin state, and an update does not touch it.
The file has two sections. `[types]` has the type of each key when it was saved, and `[settings]` has the values. The host writes the whole file to a new file and replaces the old one, so a crash leaves the old file whole.
The file is plain text. A secret is in it as typed, so protect the folder as you protect `config.txt`. The folder is in `.gitignore`. Never commit it.

An update of the plugin changes the declaration. The rules are:

- A key that stays, with the same type, keeps its value, if the new range still allows it. If not, it takes the new default.
- A new key takes its default.
- A key that goes away is ignored. Its old value stays in the file and does no harm.
- A key that changes its type goes back to its default.

## The command

The local operator types these in the server console, or the launcher sends them. Every answer goes to the log.

| Command | Effect |
| --- | --- |
| `plugins config` | Lists the plugins that have settings |
| `plugins config ID` | Lists the settings of a plugin, one line each, with tabs: `SETTING`, id, key, type, value, default, min, max (or the choices, or the longest text), live, admin, pending, label, help |
| `plugins config ID KEY VALUE` | Checks and saves a value. The rest of the line is the value, so a text may have spaces |
| `plugins config ID KEY` | Puts the default back |

A secret shows `*` in the listing when it has a value, and nothing otherwise. Its default column is empty. No log line, no event and no answer holds the text of a secret.
The pending column is `1` when the saved value waits for a restart.

A player who is an administrator can use the same command in the chat. He sees only the settings with `admin=1`, he can change only those, and the answers are short messages to him (at most 63 bytes).
A player who is not an administrator gets `Plugin command denied: plugins`. The other `plugins` commands stay for the local operator.

Results of a change: `Setting saved` with `applies now` or `applies at the next server start`, or `Setting refused` with `invalid value`, `unknown key` or `denied by the plugin` (an administrator and a key with `admin=0`).
A refusal changes nothing.

The server tool has the same operations for the launcher and for scripts. The listing only reads the manifest and the saved file, so it works while the server runs. A change needs the stopped server, and the launcher uses the local command when the server runs:

```text
subvulpes_server.exe --plugin-config ID                 (the listing)
subvulpes_server.exe --plugin-config ID KEY VALUE       (a value)
subvulpes_server.exe --plugin-config ID KEY             (the default)
```

## The launcher

The Plugins page has a **Settings...** button. It reads the listing, builds one row for each setting (a check box, a number field with the range, a list, a text field or a password field) and shows the help as a tip.
A secret field stays empty and changes only when the administrator types in it. The **Default** button of a row puts the default back.
When the server runs, the launcher sends each changed value as the local command. When the server is stopped, it uses `--plugin-config`. The form helps, and the server checks every value again.

## What the plugin sees

A plugin reads only its own settings. Include `sv_config.h` and ask `get_api` for `SV_API_CONFIG` (11), version `SV_CONFIG_VERSION` (1). The three getters work in any host callback:

| Method | For the types | Result codes |
| --- | --- | --- |
| `get_int(key, value)` | `flag`, `int` and `choice` (the index of the name, counted from 0) | `SV_STATE` outside a callback, `SV_NOT_FOUND` for an unknown key, `SV_INVALID` for another type or a null pointer |
| `get_float(key, value)` | `float` | The same |
| `get_text(key, buffer, capacity)` | `text`, `secret` and `choice` (the name). The capacity must be at least 64 | The same, and `SV_INVALID` for a small buffer |

The plugin sees the effective value. A live setting becomes effective when it is saved. A setting with `live=0` becomes effective at the next start of the server, and the saved value waits.

A live setting gives its owner a change event (`SV_EVENT_CONFIG`, record `SvConfigEvent`) with the key and the old and new text. For a secret both are empty and the plugin reads the new value with `get_text`.
The event goes only to the plugin that owns the setting, at once, at the point where the command runs. A plugin must subscribe to `SV_EVENT_CONFIG` to get it.

The package installer (`tools/plugins/install_plugin.ps1`) accepts the sections `[setting.KEY]` with the fields of the table above, at most 32 of them. It checks the shape only: the section names, the field names and the count. It copies the manifest as it is, and the host checks the values when it starts.
