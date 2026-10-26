# SubVulpes milestones

SubVulpes is the modified server. OverRosa is the future modified client.
The initial base comes from `svs-base`, commit `eb68519b0982583e188ceb619ff49f14f332e6d9`.
The direct-call and native-storage changes from OpenSubRosa24 commits `ed4aef5` and `6e56d22` are now imported.
See [synchronization evidence](upstream-sync.md) for local checks and remaining validation limits.

## S0: Runnable base

Build `subvulpes_server.exe` and `subvulpes_launcher.exe` on Windows x86.
Run the server in a separate test folder with public release data.
Check launcher Start and Stop, package names, update destination and repository privacy.
Record observed results and limits in `docs/s0.md`. S0 does not prove full gameplay equivalence.

## S1: Direct calls

**Complete for the structural migration.** The server calls C functions directly. The address dispatch table is removed.
Source review, build and startup/stop checks pass. The user accepts deferring the live client playtest.
That follow-up does not block S1 closure or establish full gameplay equivalence.

## S2: Global access

**Complete for the structural migration.** The server owns 138 separate typed objects, with no reconstructed image region.
Initial values, capacities, aliases and deterministic entity checks match the upstream migration tests.
The independent source review finds no actionable introduced defect. The native checks pass again at closure.
The type and storage generators run without a game executable. The deferred client playtest remains a follow-up.
S3 work has not started.

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
