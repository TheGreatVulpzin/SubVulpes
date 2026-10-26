# Continuation checkpoint

Checkpoint date: 2026-10-26. Work remains in progress.
The user requested this note, then authorized continued work. This checkpoint does not mark S3 complete.

## Read first

Read `CLAUDE.md`, `AGENTS.md`, `docs/plano.md`, `docs/plugins.md`, and this file.
The approved order is framework tests, the whole mapped game API with tests, then production gameplay and administration plugins.
The status DLL and test fixtures are framework probes. Do not start NPC or mission plugins before the mapped API is ready.
Lua and WASM hosts follow the common contract in S4.
Keep the original client usable wherever feasible. Client changes and new assets require explicit OverRosa support.
Do not modify the sibling OpenSubRosa24 repository.
Make small English local commits without AI coauthor trailers. Ask for push permission only at project completion.
No push or tag is authorized now.

## Git and saved work

The implementation base at this checkpoint is `2c39217`. The branch is six commits ahead of `origin/main`.
Those local commits are:

- `7ed70c3` correct native system documentation.
- `03a8fd9` define the plugin scope and system coverage.
- `bba5b84` bound replicated health and item counts, with actual codec tests.
- `e74cbb8` add generation-safe handles and packed-pool tests.
- `f00a0cb` define the native ABI and status probe.
- `2c39217` validate native metadata, files, approval and loading.

Important working changes remain unstaged and untracked. Do not discard, clean, stash or overwrite them.
The working tree contains the tested runtime. HEAD alone does not yet contain its complete integration.
The CMake index was deliberately staged in small pieces for earlier commits. The full working CMake file must be preserved.
Ignored files under `build/plans/CMake-*-stage.txt` are old staging snapshots, not the current source of truth.

Modified tracked files at this checkpoint:

```text
CMakeLists.txt
docs/plano.md
docs/plugins.md
src/server/main_loop.c
src/standalone/main.c
tools/launcher/Launcher.cs
tools/launcher/build.bat
tools/release/make_package.ps1
```

New files still outside commits:

```text
docs/plugin-sdk.md
docs/s3.md
src/plugins/plugin_control.c
src/plugins/plugin_game.c
src/plugins/plugin_host.c
tests/launcher_update_test.cs
tests/plugin_dependency_fixture.c
tests/plugin_dependency_helper.c
tests/plugin_fixture.c
tests/plugin_host_test.c
tests/plugin_install_test.ps1
tests/test_plugins.py
tools/plugins/install_plugin.ps1
```

## Implemented foundation

`src/plugins/sv_plugin.h` defines the x86 C ABI. It currently offers status, log output, owned commands and tick/stop subscriptions.
It does not yet expose game mutation APIs or internal game record pointers.
`plugin_game.c` reads status. Its actor-admin function currently denies nonlocal actors. There is no plugin chat bridge yet.
`plugin_host.c` manages dependencies, lifecycle, required modules, safe-point commands, callback errors and floating-point control restoration.
`plugin_control.c` reads bounded standard input on a worker and runs commands on the server thread.
The main entry point and loop contain working integration changes that still need commits.

The loader checks manifest values, x86 PE DLL format and SHA-256 approval before loading code.
Approval files are `plugins/native-approvals.ini` and `user/plugins/native-approvals.ini`.
The user explicitly requested `--allow-unapproved-native`. It skips content approval only and defaults off.
It does not skip ABI, metadata, path, dependency or PE checks. A DLL still has full process access and can crash or block.
Implicit native dependencies use SYSTEM32 only. Adjacent helper DLLs have no approved dependency contract in this version.
The approved entry DLL stays locked until unload. Disabled native modules remain loaded until shutdown.
Private state is in `user/plugins/state.ini`. A host lock serializes the runtime, offline changes and installation.
Required modules cannot be disabled. Non-live state changes are saved for the next restart.

The public status probe is `sv.status`, version 0.1.0. The default profile enables it and does not require it.
The production required-module catalog remains undefined.
The test-only 0.1.1 status DLL demonstrates a separate compatible module build.
No gameplay or administration plugin was created.

The installer accepts a ZIP with exactly a manifest and its native DLL. It validates before writes and never executes or approves it.
It preserves private data and existing profile classes, installs new modules disabled, and saves the prior module as a rollback ZIP.
The inspection mode returns module and package hashes. The launcher pins the inspected package hash before installation.
The launcher has plugin listing, live/offline state changes, explicit native approval and a default-off bypass option.
It also has a plugin ZIP installation flow. These interface changes have no visual acceptance yet.

## Observed checks

The latest stable normal build passed with `tools/build_hook.bat`.
The latest `ctest --test-dir build/server --output-on-failure` passed all five groups:

| Group | Observed result |
| --- | --- |
| Native lifecycle | 52 checks passed |
| Real packet codecs | Both object writers passed integer extremes, oversized lists and trailing alignment |
| Entity handles | Invalid and stale references, relocation, retirement and 2,048 packed operations passed |
| Launcher update helpers | 21 path cases plus argument quoting and backup-junction checks passed |
| Plugin installation | 111 checks passed, including rollback, private-data preservation, lock release, device names and inspected-content changes |

Local evidence is `build/server/Testing/Temporary/LastTest.log`.
`build/plans/plugin-build.log` contains the build output.
Storage validation passed for 138 independent objects and 499799832 declared bytes.
The native dependency check passed with 72 sources and no reconstructed image or address dispatch.
Repository privacy checks passed repeatedly. The most recent checked 251 files with zero problems.
No server or launcher process was running when the checkpoint inventory was captured.

These checks do not prove client gameplay equivalence or the new launcher Start/Stop flow.
The new release package has not yet been produced or tested.
`docs/s3.md` still records the older 23-check baseline and needs the current evidence and limits.

## Independent review and fixed defects

Read-only reviewers found native helper loading outside the hash contract and offline state checks before their lock.
Both fixes exist in the working tree and the expanded native tests pass.
Invalid metadata also stays invalid after a disable/re-enable attempt.

Installer fixes cover cleanup failures that skipped lock disposal, indented profile headers and Windows device-name aliases.
Launcher fixes cover stale folder rows, controls after updates, backup paths through junctions and overlapping offline operations.
The last two launcher helpers have regression tests. Visual flow acceptance remains pending.

Do not edit build inputs while a build runs. One earlier test failure used an executable older than its updated test script.
The last stable build and five-group run passed after that race was removed.

## Visual control and next steps

The `computer-use` skill was read in full with its guidance, confirmations and relevant API reference.
The supported entry point is `mcp__node_repl__js` with `@oai/sky`.
Initialization succeeded, but `sky.list_apps()` failed twice with this exact error:

```text
Computer Use native pipe is unavailable: failed to connect native pipe: O sistema não pode encontrar o arquivo especificado. (os error 2)
```

No app was selected or controlled. No screenshot or visual result exists.
Do not invent a helper protocol or claim that the interface was observed.
If this remains unavailable, the user explicitly requested manual validation.
Prepare a concrete isolated package with public default settings before requesting that check.

Next work:

1. Finish the remaining runtime and launcher/installer commits. Keep the working source, build and test results consistent.
2. Update SDK and milestone evidence, including the approved framework-before-API-before-plugins order.
3. Produce and inspect a package from the checked public data under `build/data-checked/data`.
4. Run real server lifecycle checks and inspect exit codes and residual processes. Then obtain visual/manual launcher acceptance.
5. Implement the mapped game API with coverage and failure checks before new production plugins.

The full mapping is `docs/plugin-api-map.md`. Earlier ignored drafts are in `build/plans/`.
The entity handle component is tested but not yet wired into game entity creation, deletion, reuse or reset.
Hook every lifecycle seam, including direct bot expiration in `logic_sim.c`, rather than relying only on periodic scans.
Packed pools need relocation mapping. Generation counters alone do not preserve survivor identity.
Unchecked contact-bond writers and legacy resource constructors still require review before broad creation or physics API exposure.
Do not call mission constructors blindly: full mission lists return slot 16, and several constructors dereference allocation failures.
Do not claim that spare type slots or server geometry edits automatically work on the original client.
A10, D11 and D12 remain undefined reserved identifiers.

The existing storage generators and normal server remain independent from the original executable.
The old live-client playtest deferral for S1/S2 is not proof of new plugin behavior.
