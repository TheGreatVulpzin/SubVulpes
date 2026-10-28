# SDK, test runner, easy approval and script bridge: plan

Status: plan of 2026-10-28. No code exists for it. The owner pulled this work forward from S4 and gave it to the second editor.
It follows [the plugin platform](plugins.md), [the SDK page](plugin-sdk.md) and [plugin libraries](plugin-libraries.md).

## Goal

A plugin author goes from an empty folder to an approved, tested plugin on a test server in minutes. The plugin can be C, Lua or WASM.
The author needs no source tree of the server, no hash typed by hand, and no game client to run the first tests.

## What blocks authors today

| Block | Today |
| --- | --- |
| Release | `tools/release/make_package.ps1` packages only `sv.status`, written by hand. The approval file takes one ID. `tests/package_test.ps1` expects exactly 15 files. |
| Plugin ZIP | Only `install_plugin.ps1` reads a ZIP. No script builds one. |
| SDK | The server package has no header, no template, no build file. |
| Test | A test needs a fixture written for it. No tool loads the real DLL of a plugin into a test world. |
| Approval | The administrator copies a SHA-256 by hand, or uses a bypass flag. The dialog says nothing about what the plugin does. |
| Scripts | No Lua host, no WASM host, no limits for them. |

## Decisions for the owner

| Id | Question | Recommendation |
| --- | --- | --- |
| D1 | Where do the Lua and WASM sources live? `CLAUDE.md` bars `third_party/` from the repository. | A script downloads a pinned version and checks its SHA-256 into `build/runtimes/`, as the game data download works. The licenses (MIT) fit the GPL. |
| D2 | Which WASM runtime? | Decide after work package 0. The test is a 32-bit MSVC build, a memory cap, a way to stop a loop, and size. |
| D3 | Trust model. | First: hash plus a declared capability list. Later and only if wanted: publisher signatures, so an update of the same publisher needs no new review. |
| D4 | File name of a plugin package. | `SubVulpes-plugin-<id>-<version>.zip`, like `SubVulpes-server-<version>.zip`. |

## Work packages

Each package ends with tests, a short page in `docs/`, and a commit. Estimates are days of the second editor.

### WP0 Spikes (0.5 day)

Build Lua 5.4, wasm3 and WAMR (fast interpreter) as 32-bit MSVC code with `/arch:IA32`. For each one measure the size, a memory cap, a stop for an endless loop, the cost of one call, and the effect on the x87 control word.
Write `docs/sdk-runtimes.md` with the numbers and one recommendation for D2. Do not commit any third-party source.

### WP1 Manifest with capabilities (0.5 day)

Add the keys `runtime` (`native`, `lua` or `wasm`; `native` by default) and `capabilities` (a list such as `read,write,props,storage`) to `plugin.ini`.
The host refuses `get_api` for a capability that the manifest did not declare. Native code can still bypass this, so the list is for the approval screen and for honest plugins. For scripts it is a hard wall.
Add `subvulpes_server.exe --plugin-validate PATH`. It prints every problem of a folder or ZIP with the same rules as the loader, so the tools never copy the rules.
Update `tools/plugins/install_plugin.ps1` and the launcher parser for the new keys.

### WP2 Packaging (1 day)

- A CMake function `sv_add_plugin(NAME ID VERSION SOURCES ...)` makes the target, stages the files, writes the approval entry and builds the ZIP.
- `tools/sdk/pack.ps1` makes `SubVulpes-plugin-<id>-<version>.zip` from a folder. It calls `--plugin-validate` and fails on a problem.
- `plugins/official.ini` lists the official plugins and their default state. `make_package.ps1`, the approval file and `tests/package_test.ps1` read it.
- Official plugins get their own version and their own ZIP in the release notes. Tags `plugin-<id>-v<version>` exist only when the owner asks.
- Move `sv.testroom` to the same path as an example, still out of the server package.

### WP3 SDK package (0.5 day)

`SubVulpes-sdk-<version>.zip` holds `include/` (the public `sv_*.h` headers), `template/` (a C plugin with `build.bat` and `CMakeLists.txt`), `tools/` (pack, validate, test runner), `docs/` and `examples/`.
A CMake target `sdk-package` builds it. A test unzips it to a temporary folder under `build/`, builds the template with MSVC x86, packs it and runs the test runner on it.

### WP4 Test runner (1.5 days)

`subvulpes_plugin_test.exe` links the same game sources as the server. It builds a synthetic world (the code of `tests/systems/world.c` moves to a shared file), loads a plugin from a folder or a ZIP, and runs a scenario file.
The scenario file `.svtest` is plain text, one step on each line:

```text
spawn player Ana team=1 admin=1
tick 5
say Ana /heal
expect log "healed"
expect prop human.health Ana 100
expect no-failure
```

Steps: `spawn`, `tick N`, `say PLAYER TEXT`, `console TEXT`, `give`, `set PROP`, `expect log|prop|count|failed|no-failure`, `seed N`. The random seed is fixed, so a run repeats.
The runner prints a line for each step, returns 0 or 1, and works for native, Lua and WASM plugins. It needs no game data and no client.
Write scenarios for `sv.status` and for the template. Keep `tests/systems` as the proof of the core areas.

### WP5 Easy approval (1.5 days)

- `subvulpes_server.exe --plugin-inspect ZIP|ID` prints identity, version, runtime, capabilities, hashes and the danger text, in a form that the launcher parses.
- The launcher has one dialog for install and update. It shows the review with the danger color by capability (storage and read low, write and kick high, native code highest) and one button, "Install and approve". Native code needs one extra checkbox.
- The trust store `user/plugins/approvals.ini` keeps the hash, the capability set and the time. An update with the same ID, runtime and capability set asks one question and shows the version change. A new capability needs the full review.
- Scripts need no DLL approval. They need the approval of the capability set and the bundle hash. `unsafe_events` always needs its own line in the review.
- A development switch `--dev-plugins FOLDER` approves the plugins of that folder for one run, prints a warning each minute, and does not save anything. It is for test servers.
- The CLI `--plugin-approve ID HASH` and `--allow-unapproved-native` keep their meaning.

### WP6 Runtime adapter (1 day)

A header `src/plugins/runtime.h` defines one interface: `load`, `enable`, `disable`, `event`, `command`, `unload` and the budgets. The loader picks the adapter from `runtime`.
Each script plugin is its own `SvModule`, so owners, cleanup, commands, settings, libraries and conflicts work as for native code.
Budgets for each callback: memory, work (instructions or fuel), depth and wall time. A broken budget fails the module with a refusal reason, counts in the metrics, and does not stop the server.
Save and restore the x87 control word around every script call, as the host does for native code.
A script package may hold folders: at most 256 files and 16 MiB. The installer rule "two root files" applies to native packages only.

### WP7 Binding generator (1 day)

`tools/sdk/gen_bindings.py` reads a list of the public methods (generated from the headers, so it cannot drift) and writes the Lua glue, the WASM import table, the C header for WASM plugins and the Lua stubs for editors.
Entity references are 64-bit integers. Records are Lua tables or WASM structs with generated offsets and size checks. Texts are copied with a length limit.
Every binding calls the public C table, so each check stays where it is. `tools/sdk/check_bindings.py` fails the build when a public method has no binding and no written reason.

### WP8 Lua adapter (1.5 days)

Lua 5.4 with a custom allocator for the memory cap and a count hook for the work cap. Load text chunks only. Remove `io`, `os`, `package`, `debug`, `dofile`, `loadfile`, `require` and binary chunks.
The `sv` module gives events (`sv.on`), commands (`sv.command`), settings, storage, the API tables, and a timer in the adapter (`sv.after(ticks, fn)`), until the host has its own timers.
Errors inside a callback are caught, logged with the plugin ID, and counted. Too many errors in a minute disable the plugin, as for slow native decisions.

### WP9 WASM adapter (2 days)

The runtime of D2. Imports under the module name `sv`. Exports `sv_enable`, `sv_disable`, `sv_event` and `sv_command`. A memory cap in pages, fuel or a stop for loops, no WASI except a log call.
`sdk/wasm/` gets the C header, a template in C and a note on `clang --target=wasm32`. No compiler ships with the SDK.

### WP10 Parity examples and limit tests (1 day)

The plugin "health commands" (`/heal`, `/kill`, `/imortal`) exists in C, Lua and WASM. One scenario file passes for all three.
Limit tests: an endless loop, a memory bomb, a call to a capability that the manifest lacks, an error in every callback, and a script that tries `os.execute`. Each ends with a failed module and a reason in under one second, and the server keeps its tick.

### WP11 Documents (0.5 day)

`docs/sdk.md` (quick start), `docs/sdk-lua.md`, `docs/sdk-wasm.md`. Update `docs/plugins.md`, `docs/plugin-sdk.md` and the S4 text of `docs/plano.md`. Follow the style rule: the documents pass the prose lint.

## Order and milestones

| Milestone | Packages | Result | Days |
| --- | --- | --- | --- |
| M1 | WP0, WP1, WP2, WP3, WP4 | A native author builds, tests and packs a plugin from the SDK. | about 4 |
| M2 | WP5 | One dialog to install and approve. | about 1.5 |
| M3 | WP6, WP7, WP8 | Lua plugins, with limits. | about 3.5 |
| M4 | WP9, WP10, WP11 | WASM plugins, parity and documents. | about 3.5 |

Total: about 12 days. Stop after each milestone, run the full checks and let the owner merge.

## Acceptance

1. On a clean folder, unzip the SDK, build the template, pack it, install it on a test server with the launcher in one dialog, and run its scenario. All steps pass.
2. The same "health commands" scenario passes for the C, Lua and WASM versions.
3. The limit tests of WP10 pass.
4. `ctest`, `gen_storage --check`, `check_native` and `check_repo` pass. No third-party source is in the repository.
5. A removal proof exists for each security check: the capability wall, the script sandbox, the budgets and the bounds of the bindings.

## Risks

- **The x87 control word.** The game needs fixed x87 settings. A runtime that changes them breaks the physics. Save and restore them around each call, and test it.
- **Stopping a loop.** Lua has a count hook. WASM depends on the runtime. WP0 decides.
- **Drift of the test world.** The runner builds from the same sources as the server. Do not copy structures.
- **Native plugins stay unsafe.** The capability list cannot stop native code from touching memory. Say so in the review dialog.
- **Size of the SDK.** Ship headers and text only. The user brings a compiler.

## Two editors at the same time

The first editor keeps working on the game API in the main checkout while the second editor builds the SDK. Each file has one owner.

| Owner | Files |
| --- | --- |
| Second editor (SDK) | `plugin_host.c`, `plugin_files.c`, `plugin_internal.h`, `plugin_control.c`, `plugin_config.c`, the new `runtime*.c`, `tools/sdk/`, `tools/plugins/`, `tools/release/`, `tools/launcher/`, `tests/systems/world.c` and its users, `tests/sdk/`, `CMakeLists.txt`, `docs/sdk*.md`, `docs/plugins.md`, `docs/plugin-sdk.md`, `docs/plano.md` |
| First editor (API) | `plugin_props.c`, `plugin_read.c`, `plugin_write.c`, `plugin_events.c`, `plugin_game*.c`, `plugin_made.c`, `plugin_collapse.h`, the `sv_game_*.h` headers, the game sources in `src/game` and `src/net`, `plugins/testroom/`, `tests/plugin_game_*.c`, `docs/plugin-game-api.md`, `docs/plugin-events.md`, `docs/plugin-coverage.md`, `docs/legacy-modules.md`, `docs/game/` |

- The timers of N41 live in `plugin_host.c`. They wait until the second editor ends M1.
- A new API method of the first editor has no binding in the Lua and WASM glue until WP7. When `check_bindings.py` exists, the first editor adds the entry in the same commit, or writes the reason "not exposed".
- The second editor does not move `plugins/testroom/` in WP2. The first editor still owns it.
- Both editors build in their own worktree, and each one has its own `build/` folder. Two builds at the same time can run out of memory (error 1455). Build one at a time.
- The owner merges `codex/sdk` into `main`. Files that only one editor changed merge without a conflict.

## Rules for the second editor

- Work in a new worktree: `git worktree add ..\SubVulpes-SVS-codex-sdk -b codex/sdk`. Do not use the old folder `..\SubVulpes-SVS-codex`, which the owner must delete by hand. Keep every temporary file under `build/` so the owner can delete the worktree.
- Read `CLAUDE.md` first. English for code, comments, documents and commits. Short commits. No AI coauthor lines. No push, no tag, no merge. The owner merges.
- Keep the behavior of S0. Do not edit generated files by hand.
- Run `tools\build_hook.bat`, `ctest`, `python -I tools/re/gen_storage.py --check`, `python -I tools/ci/check_native.py` and `python -I tools/ci/check_repo.py` before each commit.
- Lint the prose: `python3 <skills>/asd-ste100/scripts/ste-lint.py --fail-over 2.5 FILE`.
- You are the only editor now, so you may change `plugin_host.c`, `plugin_files.c`, `plugin_internal.h`, `Launcher.cs`, `CMakeLists.txt` and the release scripts. Keep each change small and tested.
- Prefer a long build or test in the background and prepare the next step while it runs.
