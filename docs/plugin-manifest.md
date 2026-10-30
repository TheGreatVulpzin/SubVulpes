# Plugin manifest

Status: built on 2026-10-28. The file `plugin.ini` describes a plugin. The server, the installer and the launcher read the same rules.
The server holds the rules. The installer and the script `validate_plugin.ps1` ask the server, so no tool copies them.

## Keys

All keys sit in the section `[plugin]`. A key is lower case. A key that appears twice makes the manifest invalid.

| Key | Required | Meaning |
| --- | --- | --- |
| `id` | yes | The identifier. It is the only name for paths, dependencies and conflicts. |
| `version` | yes | The version of the package, such as `0.1.0`. |
| `library` | yes | The file of the plugin. It must be in the same folder. |
| `api` | yes | The ABI number. It is `1`. |
| `live` | yes | `1` when the plugin can start and stop with the server running. `0` when it needs a restart. |
| `runtime` | no | `native` (the default), `lua` or `wasm`. Lower case only. Only `native` has an adapter today. |
| `capabilities` | no | The tables that the plugin may open. See the list below. No key means no table. |
| `dependencies`, `conflicts`, `role`, `unsafe_events` | no | See [the SDK page](plugin-sdk.md) and [plugin libraries](plugin-libraries.md). |
| `name` | no | A display name of 1 to 48 printable ASCII characters, such as `Health Commands`. |
| `description` | no | A line of up to 160 characters. |
| `author` | no | Up to 48 characters. |
| `homepage` | no | A web address that starts with `https://` or `http://`, up to 96 characters. |

The display keys are for people only. The launcher shows the name first and the identifier in small text.
Every action, such as enable, disable or approve, uses the identifier.

## Capabilities

The list has names separated by commas, with no spaces. A name appears once. The names and the tables they open:

| Name | Number | Table |
| --- | --- | --- |
| `read` | 1 | `SvGameRead` |
| `write` | 2 | `SvGameWrite` |
| `ai` | 3 | `SvGameAi` |
| `economy` | 4 | `SvGameEconomy` |
| `mission` | 5 | `SvGameMission` |
| `settings` | 6 | `SvGameSettings` |
| `storage` | 7 | `SvGameStorage` |
| `engine` | 8 | `SvGameEngine` |
| `shop` | 9 | `SvGameShop` |
| `props` | 10 | `SvGameProps` |
| `config` | 11 | `SvConfig` |
| `plugins` | 12 | `SvPlugins` |
| `network` | None | Connection address permission at bit 12. Danger: high. Reads the IP addresses of players. |

The host checks the list when the plugin calls `get_api`. A table that the manifest did not declare gives `SV_DENIED`, and the output pointer stays empty.
A declared table opens as before.

**What the list does.** It tells the approval screen and the administrator what the plugin uses. For an honest plugin it also catches a mistake.
**What the list does not do for native code.** A native plugin runs inside the server process. It can reach memory without the API. The list is a warning for it, and not a wall.
For a script runtime (Lua or WASM) the list is a hard wall, because a script can only call what the host gives it.

## Check a plugin

```text
subvulpes_server.exe --plugin-validate-folder FOLDER
tools\plugins\validate_plugin.ps1 -Path PATH -ServerExe subvulpes_server.exe
```

The first command checks a folder with `plugin.ini` and the library. The script checks a folder, a `.svplugin` file or a `.zip` file. It unpacks a package into a temporary folder first, and it refuses an entry with an unsafe name.
A good plugin gives one line that starts with `MANIFEST` and exit code 0.
A bad plugin gives one line `INVALID`, a tab, the key and a fixed message, for every problem, and exit code 1. The message never repeats the value.

The installer `tools\plugins\install_plugin.ps1` runs the same check on its staging folder before it copies any file.
It carries native code only, so `runtime=lua` and `runtime=wasm` are refused until the script adapters exist.

## Tests

`tests/test_manifest.py` (group `manifest-rules`) covers the valid and invalid values of each new key, the report for several problems at once, the script with a good package, a bad package and an unsafe entry, and the installer.
`tests/test_plugins.py` covers the capability wall in the mode `wall`.
Removal proofs on 2026-10-28: without the capability check in `get_api`, without the unsafe-name check of the script, and without the server check in the installer, a test fails each time. The restored source passes.

## Inspect a plugin for approval

Run one of these commands in the server folder:

```text
subvulpes_server.exe --plugin-inspect sv.health
subvulpes_server.exe --plugin-inspect path\to\sv.health-0.1.0-native.svplugin
```

The command reads an installed ID, a `.svplugin` file or a ZIP.
It does not load plugin code or save approval.
A successful command returns 0 and prints tab-separated records in this order:

```text
MANIFEST ID VERSION LIBRARY RUNTIME CAPABILITIES NAME DESCRIPTION AUTHOR HOMEPAGE
INSPECT 1 ID PAYLOAD_SHA256 MANIFEST_SHA256
DANGER CAPABILITY LEVEL TEXT
PACKAGE ZIP_SHA256
```

The spaces above stand for tabs. Empty display fields stay empty.
The first record uses the existing manifest format.
The number `1` identifies the inspection format.
The hashes use 64 lower-case hexadecimal characters.
The final record has an empty hash for an installed ID.
Its ZIP hash covers the original package bytes for a package.

One `DANGER` record describes each declared capability.
The levels are `low`, `high` and `highest`.
Read, storage and configuration use `low`.
Write and the other mutation capabilities use `high`.
Native code always has a separate `highest` record.
The native warning states that code has full process access and can read private data or stop the server.
The `unsafe_events` flag always adds its own `high` record.

Package inspection needs Windows PowerShell and the shipped `install_plugin.ps1` beside the server executable.
The installer reuses its ZIP entry checks, size bounds and payload checks.
It stages the checked bytes and asks the server to read the manifest with the WP1 reader.
The internal `--plugin-inspect-folder FOLDER` command serves this step.
The server also calls the existing folder validator before it prints review records.
Files stay locked against writes and replacement while the server hashes and reads them.
The installer removes its temporary folder after the review.

A failed command returns a nonzero exit code.
A caller must require exit code 0 and the full record set before it uses a review.
A partial report does not grant approval.
Only native packages have an adapter today.

The focused CTest group is `plugin-inspect`.
It checks metadata, all three hashes, changed metadata, changed payloads, risk lines, invalid packages and read-only behavior.

## Network capability and connection identifiers

`network` is capability bit 12, after `plugins`. It grants the real IPv4 address through the existing read table.
Danger level: high. Reads the IP addresses of players. No new table exists.
Without `network`, `ip` contains four zero bytes and `flags` bit 0 (`SV_CONNECTION_MASKED`) is set.
With `network`, the address is real and bit 0 is clear. Port, idle ticks and spectate remain available to read callers.
Both callers receive the same `address_id` and `network_id`. These are 32-bit HMAC-SHA256 hashes scoped to the profile.
The address hash uses all four IPv4 bytes. The network hash uses the first three bytes, with a separate domain byte.
The host makes a 16-byte salt with `BCryptGenRandom` once and saves it in `user/connection-salt.bin`.
The host reuses that file at each start. A missing salt creates new identifiers. A malformed file prevents startup.
Keep the salt private and preserve it to preserve the identifiers. The host never logs its contents or a real address.
The test room declares `network` and sends the address directly to private chat, without its usual chat log.
The capability does not isolate native code. A native plugin can read server memory, including the salt.
Tests cover both permissions, stable identifiers, distinct addresses, a shared /24, salt reuse and the capability removal proof.
