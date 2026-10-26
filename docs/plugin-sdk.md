# Native plugin SDK v1

The current SDK targets Windows x86 native DLLs.
It supports status, console logs, commands and tick/stop events. It has no game mutation API.
Game APIs, entity references, broader module services, Lua and WASM remain future work.
See [platform scope](plugins.md), [system coverage](plugin-api-map.md) and [S3 evidence](s3.md).

## Build and ABI

Use `src/plugins/sv_plugin.h`. Build C code, or use C linkage from C++.
`SV_PLUGIN_ABI` is `1`. `SV_CALL` is `__cdecl`, and public records use 8-byte packing.
Export the exact symbol `sv_plugin_entry`:

```c
SV_EXPORT SvResult SV_CALL sv_plugin_entry(uint32_t abi, SvPlugin *plugin);
```

Use `/EXPORT:sv_plugin_entry` with MSVC, as the existing targets do.
The host owns the descriptor buffer and initializes its `size`.
The entry must return `SV_OK` and fill `size`, `abi`, `id`, `version`, `flags` and `enable`.
The descriptor must match the manifest. The `disable` callback is optional.
The ID buffer holds 48 bytes. The version buffer holds 24 bytes, including terminators.
Public records contain no internal game pointers.

`plugins/status/status.c` is the buildable example. The normal `tools\build_hook.bat` build compiles it.
It registers `status`. Its output goes to the server console.

## Files and activation

Run commands from the server folder. Install each module under `plugins/ID/` with its DLL and `plugin.ini`:

```ini
[plugin]
id=sv.status
version=0.1.0
library=status.dll
api=1
live=1
dependencies=
```

Dependencies are comma-separated module IDs, without spaces. The limit is eight per module.
IDs use lowercase letters, digits, dots, underscores and hyphens. They start with a letter or digit and cannot contain `..`.
The host accepts plain x86 DLLs of at most 64 MiB.
Link private helpers statically. The loader searches System32 for imported DLLs, with no support for arbitrary adjacent helper DLLs.

`plugins/plugins.ini` selects configured modules:

```ini
[plugins]
sv.status=enabled
```

`required` prevents startup if activation fails and prevents disabling.
`enabled` selects a default-enabled module. `disabled` leaves an optional module inactive.
The optional status module is enabled in the first proof profile. No required production catalog exists yet.
Private overrides use `[plugins]` in `user/plugins/state.ini`.
The host lock is `user/plugins/host.lock`.

## Host calls and lifecycle

Call host functions only during your callbacks, on the server thread, with the supplied owner.
Worker-thread calls return `SV_STATE`.

- `log` accepts one line of at most 512 bytes, without CR or LF.
- `status` copies tick, player count, bot count and uptime into `SvStatus`. Set its `size` first.
- `command` registers a unique name and callback with public, admin or local permission. The host reserves `plugins`.
- `subscribe` replaces the module subscription for `SV_EVENT_TICK` and `SV_EVENT_STOP`. A zero mask removes it.

The limits are 32 modules and 128 commands across the host.
Command callbacks receive the owner, actor and arguments. Actor zero is the local operator.
Commands accept at most 300 bytes overall and 255 argument bytes. Nonlocal admin authorization currently denies access.
Event callbacks receive `size`, `kind` and `tick`.
Return `SvResult` codes from the header. Failed enable/event callbacks disable the module and remove registrations.
Required-service failure requests a controlled stop.

Live disable/re-enable requires `live=1` and `SV_PLUGIN_LIVE`.
Required modules and active dependents block disable. Nonlive transitions require restart.
Disabled native code stays loaded until shutdown. Replacement requires restart.

## Local operation and approval

Enter these lines through server standard input:

```text
plugins list
plugins disable sv.status
plugins enable sv.status
status
```

Offline commands use `subvulpes_server.exe`:

| Arguments | Effect |
| --- | --- |
| `--plugins-list` | Inspect configured metadata and hashes without executing DLLs |
| `--plugins-check` | Load, enable and stop selected modules |
| `--plugin-hash ID` | Inspect metadata and hash without executing the DLL |
| `--plugin-approve ID SHA256` | Approve exact current bytes |
| `--plugin-enable ID` / `--plugin-disable ID` | Save next-start state while the server is stopped |

Approvals use `[native]` entries with `ID=SHA256` in `plugins/native-approvals.ini` or `user/plugins/native-approvals.ini`.
The approval command writes the private file. Changed bytes require new approval.
A hash proves identity, not security. Native DLLs have full process access and can block, corrupt memory or crash.
The user-requested `--allow-unapproved-native` flag defaults off. It prints a warning and skips approval only, including during checks.
It preserves ABI and dependency checks. Metadata inspection does not prove safe execution or client compatibility.
