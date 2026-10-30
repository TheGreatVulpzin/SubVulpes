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

The requested native workflow is complete.
The launcher redesign waits for the owner's layout choice.
Lua, WASM and public API changes remain outside this work.

| Task | State | Evidence |
| --- | --- | --- |
| 0. Update the branch | Done | Clean fast-forward to `7681f99`. The initial CTest run passes all 15 groups. |
| 1. Server inspection | Done | `--plugin-inspect ZIP|ID`, 33 inspection cases and the ancestor-junction removal proof pass. |
| 2. Trust store | Done | Separate commit `cd704b1` adds scoped records and four independent comparison proofs. |
| 3. Unified dialog | Done | One install/update review, native consent, real-control risk colors and shared installer tests pass. |
| 4. Development switch | Done | Fourteen development cases, file locks, minute warnings and three guard-removal probes pass. |
| 5. Clean-machine approval path | Done | The unpacked SDK uses the inspected hashes and shared approved installer, then checks the saved record and activation. |
| 6. Launcher UX audit | Done | [The audit](launcher-ux.md) gives source findings, a plain-text layout, priorities and open choices. No redesign code is added. |

### Approval and compatibility

The [approval guide](sdk-approval.md) defines the record and commands.
The store binds the ID, payload hash, manifest hash, runtime and complete capability set.
It also keeps the version, ZIP hash and UTC time.
A scoped mismatch or invalid record rejects content before DLL execution, even when a legacy hash matches.
The same ID, runtime and capability set uses one version-change review.
A new capability requires the full review.

Both legacy approval files remain readable.
`--plugin-approve ID HASH` keeps its explicit full native-content meaning and exact current hash check.
`--allow-unapproved-native` keeps its content-approval bypass.
The owner authorizes `plugin_files.c` and `plugin_internal.h` for task 2 approval code only.
The loader-flow function stays unchanged. Later tasks do not edit those files.
No public `sv_*.h`, `plugin_read.c` or `plugin_props.c` file changes.

Development mode scopes the normal configured profile to a plain local folder.
It does not discover or install plugins from an unrelated folder.
Outside content needs prior approval, and profile files, manifests and DLLs stay pinned for the run.
No development approval is saved. Normal storage locks, logs and explicit operator changes keep their usual behavior.

### Removal evidence

| Check | Result |
| --- | --- |
| Payload hash match | Rejection fails with the comparison removed and passes after restoration. |
| Manifest hash match | Rejection fails with the comparison removed and passes after restoration. |
| Runtime match | Rejection fails with the comparison removed and passes after restoration. |
| Capability set match | Rejection fails with the comparison removed and passes after restoration. |
| Native checkbox | The real dialog test fails without the consent guard and passes after restoration. |
| Development no-save guard | A persistent CLI action is accepted with the guard removed. The restored guard rejects it. Ordinary development runs leave approval unchanged. |
| Development outside-folder approval | The rejection probe fails without its guard and passes after restoration. |
| Development folder boundary | A sibling-prefix probe fails without its separator guard and passes after restoration. |

The earlier inspection ancestor-junction proof also passes.
All mutated source files return to their original bytes before the final builds.
Local proof and test logs stay in ignored `build/m2-*.log` files.

### Final checks and limits

The normal build, all 19 CTest groups, storage generation, native dependencies and the repository check pass.
The clean-machine test runs outside the repository with compiler state cleared.
It uses no legacy approval for its installed template.
The shared installer also passes from a working folder outside the selected server folder.
Reviewed updates keep private plugin data.

The first full task 2 run stops when C: runs out of space.
The repeated run passes after generated output cleanup.
Oversized inspection fixtures use NTFS sparse allocation without changing logical sizes or rejection checks.

Automated checks inspect the real dialog controls and rich-text colors.
Offscreen native-window capture does not give a reliable visual result.
No live launcher visual check, Start/Stop check or client playtest ran.
The changed prose passes the ASD-STE100 gate below 2.5 findings per 100 words.

All work stays in local commits on `codex/sdk`.
No push, tag or merge into `main` ran.

### Open UX choices

1. Navigation: retain tabs for the first pass, or use a left sidebar. The audit recommends tabs.
2. Console: keep it on Server, or use a drawer available from every page.
3. Diagnostics: keep an explicit advanced toggle, or give diagnostics a permanent navigation entry.

Choose navigation first. The visual redesign waits for the owner.

### Capability table review

`src/plugins/plugin_manifest.c` owns one capability table.
`plugin_caps.h` declares its name, danger level and danger text fields.
Manifest parsing, canonical capability text and inspection use the same rows.
The API approval check uses the shared capability count.

A search of all `src/` files finds no other capability name catalog.
The dispatch table in `plugin_game.c` keeps API versions and function pointers.
It cannot replace those fields with review metadata.
Public API IDs stay unchanged because plugins use them as an interface contract.
Launcher reviews read danger text from the server inspection report.
The native runtime warning and `unsafe_events` flag remain separate from the 12 capability bits.

`capability-bits` checks each parsed name against its fixed expected bit and public API ID.
It also checks the canonical name for each bit.
The removal proof swaps the `read` and `write` rows in the production table.
The test fails with `read: expected bit 0x001, got 0x002.`
After source restoration and rebuild, the test passes.
The proof log stays in ignored `build/caps-removal-proof.log`.

The existing `tests/sdk_inspect_test.py` stays unchanged and passes.
The normal build, focused tests, storage generation, native dependencies and repository check pass.
