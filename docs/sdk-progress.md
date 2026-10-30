# SDK progress

## M1: native SDK

WP0 is complete. WAMR passes the x86 build, memory cap and loop-stop checks.
The runtime pins and their sources are in [the runtime report](sdk-runtimes.md).

The first WP1 change adds display metadata to the launcher.
The list accepts the old rows and the new rows with runtime, capabilities and display fields.
The display name and the stable ID have separate columns.
Every plugin action uses the ID.
The review shows the runtime, capabilities, author, description, homepage and hash.
Native approval always shows the full-process warning, even when a cached row names another runtime.
The file dialog accepts `.svplugin` and `.zip`.

The launcher test covers old rows, new rows, identity, bounds, characters, runtime, capabilities, homepage and hashes.
Nine removal checks fail with their check removed and pass after restoration.
The test catches errors and returns 1, so expected failures do not open a Windows crash dialog.
No visual launcher check, Start/Stop check or client playtest ran for this change.

WP1 is complete (2026-10-28). The server reads `runtime`, `capabilities`, `name`, `description`, `author` and `homepage`. The capability list is a wall at `get_api`. The command `--plugin-validate-folder` prints every problem, and `tools/plugins/validate_plugin.ps1` checks a folder or a package. The installer asks the server before it copies a file. See [the manifest page](plugin-manifest.md). Three removal proofs hold.
One change from the plan: the server command checks a folder only. A ZIP needs a reader in C, so the PowerShell script unpacks a package and calls the server.

WP2 is complete (2026-10-28). `sv_add_plugin` builds a plugin, `tools/sdk/pack.ps1` makes `ID-VERSION-RUNTIME.svplugin`, and `plugins/official.ini` drives the approval file and the server package. See [the packaging page](sdk-packaging.md). One removal proof holds.

WP3 is complete (2026-10-28). `make_sdk.ps1` makes the SDK package, and the test builds a plugin from the unpacked package only. See [the packaging page](sdk-packaging.md). One removal proof holds.

WP4 is complete (2026-10-28). `subvulpes_plugin_test.exe` links the game sources, builds a small world, loads a plugin with the probe plugin and runs a `.svtest` scenario. The SDK package carries the runner, the probe, the script and the scenario of the template. See [the runner page](sdk-test-runner.md). One removal proof holds.

Milestone M1 is complete: an author builds, tests and packs a native plugin from the SDK. All 14 groups of `ctest` pass. M2 (the one-dialog approval in the launcher) and M3 (Lua) are next. No SDK package is published, and nothing was pushed.

Build state after the checks: idle. The marker file is `build/state.txt`.

## SDK polish (2026-10-29)

| Task | State | Evidence |
| --- | --- | --- |
| 1. Merge `main` into `codex/sdk` | Done | Commit `22b8357` imports `main` at `19ae8b8` without conflicts. The normal build and all 14 CTest groups pass after the merge. |
| 2. Native approval commands | Done | The SDK README gives `--plugin-hash`, `--plugin-approve`, `--plugin-enable` and `--plugins-check`. The clean test runs each command on the installed DLL. |
| 3. MSVC discovery | Done | The template finds MSVC with `vswhere`. `VCVARS` selects an explicit `vcvars32.bat`. The SDK checks cover discovery and the override. |
| 4. First steps | Done | [The first steps](sdk-first-steps.md) cover the ZIP, build, scenario, install and approval. The README and [the package page](sdk-packaging.md) link this page. The ZIP carries it. |
| 5. Clean-machine check | Done | `tests/sdk_clean_test.ps1`, CTest group `sdk-clean`, uses a new folder outside the source tree. It clears compiler state and source paths, then builds, packs, installs, approves and tests the template. |
| 6. Release flow | Done | [The release page](sdk-release.md) gives the version, build, tests, exact ZIP check, content checks and checksum commands in order. |

The merge imports the 64-bit subscription mask, the list item sizes and `SV_STORAGE_ENTITY` from `main`.
No SDK change alters a public API header. The launcher dialog, Lua and WASM remain outside this task.

Final checks: the normal build, all 15 CTest groups, storage generation, native dependencies and the repo check pass.
The exact SDK ZIP also passes the clean test through `-SdkZip`.
The prose lint stays below 2.5 findings per 100 words for each changed page and the SDK README.
All work stays in local commits. No push, publish or tag command ran.

## M2: approval and launcher UX (2026-10-30)


| Task | State | Evidence or dependency |
| --- | --- | --- |
| 0. Update the branch | Done | Clean fast-forward to `7681f99`. The baseline CTest run passes all 15 groups. |
| 1. Server inspection | Done | The inspection group and the ancestor-junction removal proof pass. |
| 2. Trust store | Open, blocked | Approval enforcement needs the forbidden `plugin_files.c` file. |
| 3. Unified dialog | Open | Follows task 2. |
| 4. Development switch | Open | Follows task 3. |
| 5. Clean-machine approval path | Open | Follows task 4. |
| 6. Launcher UX audit | Open | Follows task 5. The audit document does not exist yet. |

Milestone M2 stays open.

The brief for this work restricts edits to SDK, launcher, inspection and test files.
Task 0 imports `main` at `7681f99` by a clean fast-forward on `codex/sdk`.

Task 1 adds `--plugin-inspect ZIP|ID`.
The command reuses the WP1 manifest reader, the folder validator and the installer ZIP checks.
The report contains display metadata, runtime, capabilities, payload and manifest hashes, the package hash and risk text.
The [manifest page](plugin-manifest.md#inspect-a-plugin-for-approval) defines its record format.

Task 2 is blocked by the brief's file restriction.
The loader reads content approval in `sv_plugin_approved` and enforces it in `sv_plugin_load`, both in `src/plugins/plugin_files.c`.
That file is forbidden.
The loader currently reads only DLL hashes from `native-approvals.ini`.
It does not read `user/plugins/approvals.ini` or bind approval to the runtime, manifest hash and capability set.
A launcher-only trust file cannot enforce that match when the server starts.
Task 2 needs permission to change `plugin_files.c`.
Its focused loader tests can stay under `tests/sdk_*`.
The brief says to stop and record this need when a task needs a forbidden file.

Tasks 3 through 6 remain open because they follow task 2.
The unified dialog, development switch, clean-machine approval path and UX audit are not built in this change.
The hash-match, capability-match, native-checkbox and development-store removal proofs remain open with those tasks.
The old approval CLI and native bypass keep their current behavior.

No visual launcher check, Start/Stop check or client playtest ran.
No push, tag or merge into `main` ran.

Final checks pass: the normal build, all 16 CTest groups, storage generation, native dependencies and the repository check.
The inspection group passes 33 cases.
The ancestor-junction rejection test fails with its check removed and passes after restoration.
The four WP5 approval removal proofs stay open with tasks 2 through 4.
Local evidence stays in the ignored `build/m2-*.log` files.
