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
S3 runtime implementation is in progress. See [S3 evidence](s3.md).

## S3: Core boundaries and official native plugins

**In progress. S3 remains open.** The first native foundation and default-enabled status module exist.
See [S3 evidence](s3.md), [plugin design](plugins.md) and [system coverage](plugin-api-map.md).
The coverage catalog defines intended reach. The current status proof does not establish that reach.

The server keeps game simulation, protocol handling and unconditional memory and representation checks in the core.
New administration tools and configurable server policies belong in official plugins shipped with the server.
Some official plugins are required and always active. Others are enabled by default or optional.
These activation classes are distinct. The first proof profile enables the status module but does not require it.
The production module catalog remains undefined. Define required modules before implementing those services.

The goal is to update an administration or policy plugin independently when the installed core API remains compatible.
Official plugins remain separate versioned modules rather than features compiled into the server executable.
An update that needs a new API capability can also require a core update.

Continue implementation in this order:

1. Complete the existing native framework, status proof and framework tests.
   Prove loader and manager states, versions, dependencies, content approval, command ownership, events and required-module failures.
   The status DLL and test fixtures prove the framework. They are not new production plugin work.
2. Complete the common API across every system in the [coverage catalog](plugin-api-map.md), with focused tests.
   Cover reads, mutations, decisions that plugins can replace or cancel, and resources owned by plugins.
   Add stable handles, relocation tracking, execution phases, lifecycle events, bounds and resource limits where each system needs them.
   Keep public ABI types independent of internal game record layouts.
   Record each mapped capability, its tested behavior and any unsupported operation or client limit before production plugin work starts.
3. Only after the API coverage proof, build production gameplay and administration plugins.
   Include administration commands, permissions, ban policy and persistence, RCON and optional log output.
   Then demonstrate two NPC behaviors, a custom mission, an effect with an existing weapon representation and an external snapshot consumer.
   Core operations supply validated game access and lifecycle events. Plugins supply policy and features.

Classify A7 through A10, B5, C3, C4, D11, D12 and persistent `ban_time` before their corresponding implementation.
Separate core invariants from plugin policy. Check compatible base fixes for OpenSubRosa24. See [placement notes](ideas.md).
Define A10, D11 and D12 first. Their inherited identifiers still have no description.

The current proof includes 57 native checks plus codec, handle, launcher and installation checks. See [S3 evidence](s3.md) for results and limits.
These checks do not close the full API coverage requirement or S3.

Package and version production plugins separately. Check API and module dependencies before installation or activation.
Prove that a compatible plugin update needs no core rebuild and preserves private settings and plugin data.
A clean server restart is acceptable for installation. Live module replacement is not required for S3.
Check the normal build, controlled shutdown, launcher Start/Stop and runtime compatibility. Record focused tests and observed gameplay limits in the milestone evidence.
Required plugins use the common API and lifecycle. Missing required modules must prevent normal startup.

Loading a required RCON plugin does not enable a public listener by itself. Module activation and endpoint configuration are separate decisions.
Define authentication and request limits before exposing remote administration.
Define the exact command list, permissions, ban expiration rules and log outputs before implementing each official plugin.
The native foundation does not close the game API or official service work.
The complete mapped API remains future work. Production plugins and the gameplay examples follow its coverage proof.
Keep these examples within the original client limits stated in the system coverage catalog.
Lua, WASM and the external SDK remain later work in S4.

## S4: Lua, WASM and external SDK

After the common API proof in S3, add Lua, then WASM hosts and an SDK.
Both hosts use the tested common API. Their implementation and acceptance remain S4 work.
Keep one public C API as the contract for official and third-party modules.
Document compatibility, examples and author tools against that tested contract.
Optional modes, integrations and other custom features use the same extension system.
See [plugin design](plugins.md).

## Cleanup subplan

Small style cleanups of inherited code are tracked in [cleanup](cleanup.md). They do not block a milestone.

## Later

Add dynamic pools, 64-bit builds, other platforms, an independent server list and server anti-cheat.
Check protocol limits before increasing pools. Client-dependent features belong to OverRosa.
