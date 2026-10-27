# Plugin platform

Status: approved design. S3 remains open. Implementation evidence must state which operations and hosts exist.
See [the milestones](plano.md) and [system coverage](plugin-api-map.md).
The coverage catalog describes the intended reach. It does not claim a complete public API.

## Reach and client compatibility

Expose every feasible server capability over time. Administration is one use of the platform.
Plugins can define NPC behavior, missions, weapons, object interactions, economy, modes and external services.
Use the original client wherever its protocol and behavior support the feature.
Use OverRosa for features that require new client assets, interfaces, type definitions or client calculations.
Each operation must state its client requirements and remaining test limits.

Existing C functions are implementation seams. Their signatures and internal layouts are not the public ABI.
Known type numbers and spare pool slots do not prove support for arbitrary new types on the original client.
Map geometry changes need a specific synchronization check. Server memory changes alone do not prove a matching client world.

## Core and official plugins

The core owns simulation, game storage, protocol parsing, the loader and unconditional representation checks.
The core validates requests before they can reach internal game state.
Basic startup, failure and shutdown diagnostics work before plugins load.

Official plugins own administration commands, permission policy, moderation persistence, remote administration and optional log output.
Official and third-party plugins use the same public API and lifecycle.
Keep inherited administration available until each replacement has focused checks.
Do not remove working behavior merely because a future official module is planned.

Activation classes belong to the selected server profile:

- Required: active whenever that profile runs. Missing or incompatible modules prevent normal startup.
- Default enabled: initially active, but the administrator can disable the module when dependencies allow it.
- Optional: active only after the administrator selects it.

The first native proof enables the status module but does not require it. This does not establish the final required service catalog.
A remote listener stays off until its endpoint and authentication are configured.
Reserved items A10, D11 and D12 still lack definitions. They are not inferred requirements or completed work.

## Common C contract

One versioned C contract serves native modules and the later Lua and WASM adapters.
Public records use explicit sizes and versions. Native entry points have a documented calling convention.
An unsupported version or missing capability produces an error before activation.
The contract never exposes pointers into internal game storage.
Native function pointers and buffers at the ABI boundary do not give scripts direct engine pointers.

Entity references carry a type and generation. Packed pools also require relocation tracking for surviving entities.
Deletion, reuse and reset must not turn an old reference into a different entity.
Account identity, connection identity and entity lifetime are separate concepts.

Each operation declares its execution phase, inputs, effects, network updates, bounds, failures and resource cost.
Mutations enter at defined safe points. Derived state needs a complete operation, such as moving every body part during teleport.
Inventory operations update both sides of a link. Queries copy shared scratch results before another query can overwrite them.
Unknown fields remain unknown until their behavior is understood.

## Events and ownership

Events distinguish observations from decisions that can replace or cancel default behavior.
Each event documents its order and permitted changes. A plugin cannot cancel mandatory memory or protocol checks.
Callbacks use a stable priority order. A return value can report failure without changing unrelated state.
Do not promise rollback of effects that already completed.

The host owns command registrations, subscriptions and scheduled resources on behalf of each plugin.
Disable stops future callbacks and removes these registrations at a safe point.
Already-created game entities need an explicit retention or cleanup policy.
Plugin data remains on disk unless the administrator requests its removal.

## Native loading and updates

Native DLLs must match the Windows x86 server and its ABI.
Do not load an unapproved DLL to inspect its metadata. Loading can execute native initialization code.
The loader checks package metadata and content identity before `LoadLibrary`.
The administrator sees a clear warning before approving a third-party native binary.
Approval identifies the actual bytes. Changed content requires a new approval or another configured trust policy.
The server enforces approval even when it runs without the launcher.
A hash proves content identity, not safety or authorship.

Native code in the process can read process data, corrupt memory, block indefinitely or crash the server.
An exception handler cannot undo corrupted state. A watchdog cannot safely interrupt arbitrary native code.
Do not continue a damaged process under a claim that removing the plugin repaired it.
Runtime measurements can identify slow callbacks after they return.
Recoverable reported errors can disable an optional plugin and its registrations.
Failure of a required service needs a documented controlled-stop rule.

Live enable and disable apply only to modules that support those transitions.
Disabled native code can remain loaded until shutdown. Native replacement and some installations require a restart.
Check API versions and dependencies before installation or activation.
Preserve private settings and data during updates. Keep the previous package available for rollback.
A compatible module update does not require a core rebuild.

## Lua and WASM

Lua and WASM hosts follow the common API coverage proof and remain S4 work.
Both hosts use the tested common contract. Their planned role does not imply that either host exists.
Prioritize Lua and WASM for third-party code that needs containment.
Each plugin has separate runtime state, memory limits, execution limits and authorized host services.
Lua needs protected calls, an allocator budget and an instruction budget.
WASM needs trap handling, bounded memory and an enforceable execution budget.
A script budget does not bound a blocking or unsafe C host function.
Every exported host operation still needs its own checks.

Select each runtime only after a Windows x86 build and focused resource-limit proof.
Do not copy unrestricted `os`, `io`, native library access or FFI into a script sandbox.
Do not claim a fixed performance ratio before measuring the selected runtime and workload.
The SDK must include a buildable example, capability requirements, errors and a repeatable test command.

## Delivery and acceptance

Deliver the platform in this order:

1. Complete the existing native framework, status proof and framework tests.
   Cover loader and manager states, dependencies, content approval, command cleanup and compatible module replacement after restart.
   The status DLL and test fixtures are framework proof. They do not start production gameplay or administration plugin work.
2. Complete the common API across every mapped system, with focused tests for each capability.
   Include reads, mutations, decisions that plugins can replace or cancel, and resources owned by plugins.
   Prove handle lifetimes, packed relocation, execution phases, bounds, failures and resource limits.
   Record implemented coverage, unsupported operations and original-client limits before production plugin work starts.
3. Only after this API proof, build production gameplay and administration plugins.
   Demonstrate two NPC behaviors, a custom mission, an effect with an existing weapon representation and an external snapshot consumer.
   These examples exercise the contract. They are not its final ceiling.

The current proof includes 57 native checks plus codec, handle, launcher and installation checks.
See [S3 evidence](s3.md) for recorded results and limits. This proof does not close S3 or the full API scope.

Check invalid references, exhausted resources, wrong execution phases, conflicting commands, failed activation and dependency failures.
Check controlled shutdown, launcher Start/Stop and absence of residual processes.
Keep the deferred original-client playtest explicit until observed evidence exists.
Lua and WASM acceptance additionally requires controlled loop, allocation and host-call failure cases.

## References

Use [RosaServer types](https://github.com/RosaServer/RosaServerCore/blob/master/.meta/template/types.lua)
and [hooks](https://github.com/RosaServer/RosaServer/wiki/Hooks) as organizational examples, rather than a compatibility contract.
Use [Shimmie2 extensions](https://github.com/shish/shimmie2/wiki/Extension-Development) as an example of separately maintained features.
See the [Lua manual](https://www.lua.org/manual/5.4/manual.html),
[WASM security model](https://webassembly.org/docs/security/)
and [Windows SEH documentation](https://learn.microsoft.com/en-us/windows/win32/debug/structured-exception-handling) for their actual guarantees.
