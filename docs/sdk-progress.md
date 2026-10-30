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
| 2. Trust store | Done | The owner allows approval-only edits. The focused loader tests and four removal proofs pass. |
| 3. Unified dialog | Done | Combined installation and approval, update classification, real-control colors and the native-checkbox removal proof pass. |
| 4. Development switch | Done | One-run scope, locked files, minute warning and three removal probes pass. |
| 5. Clean-machine approval path | Done | The clean SDK uses inspection, the shared approved installer and the scoped approval file. |
| 6. Launcher UX audit | Open | Follows task 5. The audit document does not exist yet. |

Milestone M2 stays open.

The brief for this work restricts edits to SDK, launcher, inspection and test files.
Task 0 imports `main` at `7681f99` by a clean fast-forward on `codex/sdk`.

Task 1 adds `--plugin-inspect ZIP|ID`.
The command reuses the WP1 manifest reader, the folder validator and the installer ZIP checks.
The report contains display metadata, runtime, capabilities, payload and manifest hashes, the package hash and risk text.
The [manifest page](plugin-manifest.md#inspect-a-plugin-for-approval) defines its record format.

Task 2 is complete. The owner allows approval and trust-store edits in `plugin_files.c` and `plugin_internal.h` for this task.
The loader flow and public headers stay unchanged.
The [approval page](sdk-approval.md) defines the scoped record, precedence and CLI behavior.
Payload, manifest, runtime and capability rejection probes each fail with their comparison removed and pass after restoration.

Tasks 3 through 6 remain open because they follow task 2.
The unified dialog, development switch, clean-machine approval path and UX audit are not built in this change.
The native-checkbox proof passes in task 3. The development-store proof passes in task 4.
The old approval CLI and native bypass keep their current behavior.

No visual launcher check, Start/Stop check or client playtest ran.
No push, tag or merge into `main` ran.

Task 2 checks pass: the normal build, all 17 CTest groups, storage generation, native dependencies and the repository check.
The inspection group passes 33 cases.
The ancestor-junction rejection test fails with its check removed and passes after restoration.
The hash and capability removal proofs pass in task 2. The native-checkbox proof passes. The development-store proof passes in task 4.
Local evidence stays in the ignored `build/m2-*.log` files.

The first full task 2 run stops when C: runs out of space.
The repeated run passes after generated test output cleanup.
Oversized inspection fixtures now use NTFS sparse allocation without changing their logical sizes or rejection checks.

Task 3 checks pass: the normal build, six affected launcher/install/SDK/package groups, storage generation, native dependencies and the repository check.
The real dialog control keeps each risk color after attachment to its form.
The native-checkbox rejection test fails with its guard removed and passes after restoration.
Offscreen native-window capture does not give a reliable visual result. No live launcher visual check or Start/Stop check ran.

Task 4 checks pass: the normal build, development, trust and lifecycle groups, storage generation, native dependencies and the repository check.
The development test covers 14 cases, pinned files and the real minute warning.
Persistence, outside-folder approval and folder-boundary rejection each fail with their guard removed and pass after restoration.
Development approval scopes the normal configured profile. It does not discover or install plugins from an unrelated folder.
No task 4 edit changes `plugin_files.c` or `plugin_internal.h`.

Task 5 checks pass: the normal build, all 19 CTest groups, storage generation, native dependencies and the repository check.
The clean test installs from the unpacked SDK folder, checks every scoped approval field and starts the plugin without legacy approval.
The shared installer now enters the selected server folder for approval and restores the caller location afterward.
The approved-install regression also runs from outside the server folder.
The SDK carries the approval guide. Its first steps use the unified launcher review by default.
