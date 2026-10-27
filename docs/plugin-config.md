# Plugin settings (N23)

Status: built on 2026-10-27. The host, the command, the API of the plugin and the launcher form exist. The original client is not involved.
A setting is a value that the administrator sets for one plugin, for example the price of an item. It is not the storage of the plugin (N18).
The settings of the core are a different thing (N14).

| | Settings (N23) | Storage (N18) |
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
A player who is not an administrator gets `Plugin command permission denied: plugins`. The other `plugins` commands stay for the local operator.

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

## Tests

The lifecycle test (`tests/test_plugins.py`, with the fixture of `tests/plugin_fixture.c`) runs:

- every type, with the defaults read through the API, and every result code of the three getters;
- a live change with its event, a change that waits for a restart, and the default put back;
- ten wrong values (four for an integer, a flag, a choice, two decimals and two texts), an unknown key and an unknown plugin;
- a secret: it is saved, its length is read, and its text is in no output;
- an administrator who changes a key with `admin=1` and is refused on a key with `admin=0`, the listing for that player, and a player who is not an administrator;
- values that stay after a restart, and an update of the manifest (a kept key, a new key, a retyped key and removed keys);
- the file command: the listing, a value, the default, a value out of range, an unknown key and an unknown plugin;
- the listing beside a running server, and the refusal of a change then;
- the installer: a package with settings and `unsafe_events` installs and keeps its manifest, and eight wrong packages are refused (a value that is not 0 or 1, an unknown field, a repeated field, a repeated section, an uppercase key, another section, a section before `[plugin]`, and 33 sections);
- 19 wrong declarations, each one refused: unknown type, wrong range, default out of range (integer and decimal), wrong flag default, choice default outside the list, one choice, nine choices, a choice name that is too long, a secret with a default, a text default longer than the limit, `maxlen` of 64, an uppercase key, a key of 33 characters, no label, a `live` or `admin` value that is not 0 or 1, a semicolon in the label, and 33 settings.

Removing one check at a time made the lifecycle test fail each time (`build/logs/mutation20.log`): the range of an integer and of a decimal, the text length, the text characters, the limit of 32 settings, the limit of 8 choices, the size of a choice name, the length of a key, the administrator gate, the hiding of a secret in the event and in the listing, the reset of a retyped key, and the buffer capacity of `get_text`. The installer checks (the fields, the count, a repeated section and the `unsafe_events` value) have their own proofs (`build/logs/mutation21.log`).

Not proven: the launcher form (no automatic test runs a window), the reparse-point checks of the private file (they copy the pattern of the approval file), and a crash of the host in the middle of the write (the design leaves the old file, and the test does not kill the process).
