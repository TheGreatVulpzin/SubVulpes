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
S3 runtime implementation has not started.

## S3: Core boundaries and official native plugins

The server keeps game simulation, protocol handling and unconditional memory and representation checks in the core.
New administration tools and configurable server policies belong in official plugins shipped with the server.
Some official plugins are required and always active. Others are enabled by default or optional.
These activation classes are distinct. Define the required module list before implementation.

The goal is to update an administration or policy plugin independently when the installed core API remains compatible.
Official plugins remain separate versioned modules rather than features compiled into the server executable.
An update that needs a new API capability can also require a core update.

Implement in this order:

1. Classify A7 through A10, B5, C3, C4, D11, D12 and persistent `ban_time` by core invariant or plugin policy.
   Check which compatible base fixes also belong in OpenSubRosa24. See [placement notes](ideas.md).
   Define A10, D11 and D12 before implementation. Their inherited identifiers still have no description.
2. Add the minimum native C API and loader needed by an official plugin.
   Define events, handles, command registration, API versions, dependency checks and required-plugin startup behavior.
   Public ABI types must not depend on internal game record layouts.
3. Build administration commands, permission services, ban policy/persistence, RCON and log output as official plugins.
   Core code supplies validated operations and lifecycle events. Plugins supply the policy and user-facing features.
   Define authentication and request limits before exposing remote administration.
4. Package and version official plugins separately. Check API and module dependencies before installation or activation.
   Prove that a compatible plugin update needs no core rebuild and preserves private settings and plugin data.
   A clean server restart is acceptable for installation. Live module replacement is not required for S3.
5. Test required and optional activation, missing dependencies, API mismatches, plugin updates and runtime compatibility.
   Required plugins use the same API and lifecycle as other plugins. Missing required modules must prevent normal startup.
   Keep build, launcher Start/Stop, focused tests and observed gameplay limits in the milestone evidence.

Loading a required RCON plugin does not enable a public listener by itself. Module activation and endpoint configuration are separate decisions.
Define the exact command list, permissions, ban expiration rules and log outputs before implementing each official plugin.
This section is a plan. No plugin API, loader or official plugin is implemented by the documentation change.

## S4: Lua, WASM and external SDK

Extend the native plugin foundation from S3 with Lua, then WASM and an SDK.
Keep one public C API as the contract for official and third-party modules.
Document compatibility, examples and author tools against that tested contract.
Optional modes, integrations and other custom features use the same extension system.
See [plugin design](plugins.md).

## Later

Add dynamic pools, 64-bit builds, other platforms, an independent server list and server anti-cheat.
Check protocol limits before increasing pools. Client-dependent features belong to OverRosa.
