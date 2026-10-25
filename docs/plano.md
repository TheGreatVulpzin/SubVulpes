# SubVulpes milestones

SubVulpes is the modified server. OverRosa is the future modified client.
The runtime base comes from `svs-base`, commit `eb68519b0982583e188ceb619ff49f14f332e6d9`.

## S0: Runnable base

Build `subvulpes_server.exe` and `subvulpes_launcher.exe` on Windows x86.
Run the server in a separate test folder with public release data.
Check launcher Start and Stop, package names, update destination and repository privacy.
Record observed results and limits in `docs/s0.md`. S0 does not prove full gameplay equivalence.

## S1: Direct calls

Remove `src/standalone/fn_table.c` and `CODE(rva)` dispatch. Use direct C calls.
Keep startup and normal gameplay working after each conversion.

## S2: Global access

Add an access layer for globals. Move globals into real C variables in later changes.
Preserve initialization, sizes and dependencies during the transition.

## S3: Rules and administration

Add behavior changes A7 through A10, B5, C3, C4, D11, D12 and persistent `ban_time`.
Define A10, D11 and D12 before implementation. The inherited notes do not define their scope.
Add admin commands, RCON and structured logs. Define authentication and rate limits for public administration.

## S4: Plugin API

Add a native C API first, then Lua, then WASM with an SDK.
Define events, handles, API versions and compatibility rules before hosts depend on them.
See [plugin design](plugins.md). Core rule changes can exist independently of plugins.

## Later

Add dynamic pools, 64-bit builds, other platforms, an independent server list and server anti-cheat.
Check protocol limits before increasing pools. Client-dependent features belong to OverRosa.
