# Continuation checkpoint

Checkpoint date: 2026-10-26. The framework and first game read slice are committed.
Lifecycle integration is `3fbcbf3`. The read API implementation is `92854b1`.
The full mapped API and S3 remain open. Core bond capacity guards are committed and tested within the limits below.

## Read first

Read `CLAUDE.md`, `AGENTS.md`, `docs/plano.md`, `docs/plugins.md`, and this file.
The approved order is framework tests, the whole mapped game API with tests, then production gameplay and administration plugins.
The status DLL and test fixtures are framework probes. Do not start production NPC, mission or administration plugins before API coverage proof.
Lua and WASM hosts follow the common contract in S4.
Keep the original client usable wherever feasible. Client changes and new assets require explicit OverRosa support.
Do not modify the sibling OpenSubRosa24 repository.
Make small English local commits without AI coauthor trailers. No push or tag is authorized.

## Git and saved work

The framework implementation, lifecycle integration and first read API are committed.
The SDK and read-contract documentation updates are ready but uncommitted at this checkpoint.
Core bond capacity guards are committed with their test. See the bond guard result below.
Inspect the current working tree before changes. Preserve any new API work and unrelated user changes.
Ignored staging snapshots under `build/plans/` are historical notes, not current source inputs.

The tested framework includes the native ABI, loader, runtime integration, standard input control, installer, launcher controls and focused tests.
Its package and real server evidence appears in [S3 evidence](s3.md).
The tested package records source commit `0b9d7b2`, which differs from the later documentation checkpoint.
Do not replace that artifact's recorded provenance with the current HEAD.

## Implemented foundation

`src/plugins/sv_plugin.h` defines the x86 C ABI and optional capability lookup after its original prefix.
The host offers status, console logs, owned commands and tick/stop subscriptions.
The first read capability copies player, human, item, object, account, bone, inventory and world state.
It uses opaque generation-safe references connected to core entity lifetimes.
The read methods enforce callback ownership, the server thread and world readiness.
They expose no game mutations or internal game pointers. Nonlocal actor-admin requests remain denied, and no plugin chat bridge exists yet.
`plugin_host.c` manages dependencies, lifecycle, required modules, safe-point commands, callback errors and floating-point control restoration.
`plugin_control.c` reads bounded standard input on a worker and runs commands on the server thread.
The main entry point and loop integration are committed.

The loader checks metadata, x86 PE DLL format and SHA-256 approval before loading code.
Approval files are `plugins/native-approvals.ini` and `user/plugins/native-approvals.ini`.
The user-requested `--allow-unapproved-native` flag skips approval only and defaults off.
It preserves ABI, metadata, path, dependency and PE checks. Native code still has full process access and can crash or block.
Implicit native dependencies use System32 only. Arbitrary adjacent helper DLLs have no approved dependency contract.
The entry DLL stays locked until unload. Disabled native modules remain loaded until shutdown.
Private state is in `user/plugins/state.ini`. The host lock serializes runtime, offline changes and installation.
Required modules cannot be disabled. Transitions without live support require restart.

The status probe is `sv.status`, version 0.1.0: default-enabled and non-required.
The required production module catalog remains undefined.
The test-only 0.1.1 status DLL proves a separate compatible module build.
No production gameplay or administration plugin exists.

The ZIP installer validates a manifest and DLL without executing or approving them.
It preserves private data and profile classes, installs new modules disabled, and saves the previous module as a rollback ZIP.
Inspection returns module and package hashes. The launcher pins the inspected package hash before installation.
The launcher offers listing, live/offline state changes, native approval, a default-off bypass and ZIP installation.
These controls still lack visual and manual acceptance.

## Observed checks and package

The latest stable normal build exited with code 0 after `tools/build_hook.bat`.
All six CTest groups passed in 34.71 seconds, including the new `game-api` fixture.
A focused `game-api` rerun after build completion passed in 0.79 seconds.
The framework groups cover native lifecycle, actual codec bounds, handles, launcher update helpers and installation.
The handle checks include 2,048 packed-pool operations. Launcher checks include 21 path cases, argument quoting and backup-junction rejection.
Local evidence is `build/server/Testing/Temporary/LastTest.log`.
Storage validation passed for 138 independent objects and 499799832 declared bytes.
The native dependency check passed with 74 sources and no reconstructed image or address dispatch.
The repository privacy check found zero problems in 262 files.
These results cover the committed read slice. They do not cover the current bond guard work.

The canonical package is `build/package/SubVulpes-server-v0.0.0-s3-framework.1.zip`.
[S3 evidence](s3.md) records its SHA-256, source commit and public-default test conditions.
The earlier framework package cycles remain recorded in S3 evidence.
The current [read API evidence](plugin-game-api.md) records two further real server cycles, exact durations, markers and binary hashes.
Both exited with code 0 after runtime-read, stop-denial and live disable/enable checks.
They used the previous framework package's public data and old-ABI status DLL with the current executable and added read fixture.
This setup is not a new release package.
The first read cycle produced one 62.3 ticks/s sample. It is not a performance comparison or sustained-load proof.
No client gameplay equivalence follows from these checks.

## Review fixes and work discipline

Committed fixes reject native helper loading outside the hash contract and lock offline state checks before changes.
Invalid metadata remains invalid after disable/re-enable attempts.
Installer tests cover cleanup failures, lock release, indented profiles, device names and changed inspected packages.
Launcher fixes cover stale folder rows, controls after updates, backup junctions and overlapping offline operations.
Path and backup helpers have focused tests. Visual flow acceptance remains pending.

Do not edit build inputs while a build runs. An earlier test failure used an executable older than its test script.
The last stable build and six-group run passed after that race was removed.
Run the normal build and focused tests after API changes. Run `python -I tools/ci/check_repo.py` before each commit.

## Manual review and current work

The visual tool initialized, but `sky.list_apps()` failed twice because its native pipe was unavailable.
No app control, screenshot or visual result exists. Do not invent another helper protocol or claim an observed interface flow.
The user requested manual validation if visual control remained unavailable.
Launcher PID 14684 opened from this isolated package copy:

```text
build/framework-review/db203db7e5e542a28e330f1a825d1f09/SubVulpes-server-v0.0.0-s3-framework.1
```

The manual watcher ended after ten minutes without observing a server process. Launcher PID 14684 remains open.
The manual question remains pending. The user did not yet accept the launcher flow or manual Start/Stop result.
API work can continue without claiming manual success.

Continue with [the full API mapping](plugin-api-map.md), its coverage and failure checks.
Most mapped read, mutation and hook operations remain unimplemented.
The read slice now uses entity lifecycle integration, including retirement and reset.
Keep creation, deletion, reuse and packed-pool relocation consistent as the API expands.
Generation counters alone do not preserve packed survivor identity.

The bond capacity guards check `bonds_available()` before creation and physics writers. See the bond guard result below.
Review all contact-bond writers and legacy resource constructors before broad creation or physics exposure.
Full mission lists return slot 16, and several constructors dereference failed allocations. Do not expose those paths without validated operations.
Spare type slots and server geometry edits do not prove original-client support.
A10, D11 and D12 remain undefined reserved identifiers.
The storage generators and normal server remain independent from the original executable.
The deferred S1/S2 client playtest does not prove new plugin behavior.

## Bond guard result

The guards add `bonds_available()`, a `Bond *` result for `contact_bond`, a 28-bond check in `createhuman` and rollback of a failed spawn.
After a normal build, all six CTest groups passed, with `game-api` at 0.90 seconds.
`gen_storage.py --check`, `check_native.py` and `check_repo.py` passed.
`tests/physics_bounds.c` covers these cases directly: contact bonds at full, one free and out-of-range counts; `createhuman` at 27 and 28 free bonds; the 32-joint and 128-muscle lists;
door and gun holds in both hands; the human contact and door bond; a full player list and one free player slot; and rollback of traffic, chase and guard spawns when humans, bonds or objects are full.
It does not cover the level and object contacts in `humans.c` and `objects.c`, the wheel and track bonds in `wheels.c`, or a failed `createhuman` in `game_setup.c` and `logic_sim.c`. They need level geometry.
The last-slot case of `chase_crew` and a full spawn with a free slot are also not run. A failed spawn can leave guards made before the failure.
One behavior changed: `scenario_createchase` returns -1, not 0, when the city has no roads, and no mission is made then. See `docs/game/physics.md`.
A first test run failed only because a new test cleared the humans that an older test used. The order is fixed.

## OpenSubRosa24 launcher and package import

Upstream `770193c`, `f856e3c` and `50a8177` are imported as local commits `abb364b`, `c70854c` and `be11b1f`. See [synchronization evidence](upstream-sync.md).
The launcher makes an admin password when `config.txt` has none and refuses a blank one. The package has no `config.txt`.
After a normal build, all seven CTest groups passed, including the new `package-contents` group (24 checks).
The user checked the launcher window by hand on 2026-10-27, in a copy of the package without `config.txt`. The package held test fixture data, not real game data.
The launcher made a password on the first run, refused Start with a blank password, and accepted Start with a typed one. The user reported all three as expected.
Start with real game data was not checked in that run.
`74dda0e..980705e` was pushed to `origin/main` at the user's request. No tag was made.
