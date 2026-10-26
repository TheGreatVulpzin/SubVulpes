# OpenSubRosa24 server synchronization

## Scope and provenance

This synchronization starts at SubVulpes commit `b0086b9` and reads the sibling OpenSubRosa24 checkout without changing it.
The upstream source is `6e56d22a56ccf2a633fb7557b131f4b312fa6209`.
The upstream checkout remains clean.

Imported changes:

- Upstream `ed4aef5f56533e0e7395a7964cb2c9c08fc2b766`: direct C calls. Local commit: `552ba2b`.
- Upstream `6e56d22a56ccf2a633fb7557b131f4b312fa6209`: owned typed storage. Local commit: `f1d2c1c`.

This repository has a separate import history, so the relevant server changes are imported without the upstream verification directories.
SubVulpes executable names, launcher, default configuration, package name and update destination stay in use.
No S3 rules, administration features or plugins are added.
No push or tag occurs in this synchronization.

## Runtime and generator changes

The normal server uses direct C calls and 138 separate storage objects with 499,799,832 declared bytes.
Record sizes, capacities, floating point expressions and account formats stay unchanged.
The dependency check inspects 67 target sources, include paths, game access expressions and linked map symbols.
It finds no reconstructed image, RVA dispatch, lifted implementation or recording entry instrumentation in the server target.
The x87 thunks now live in `src/platform/x87_math.c`.
The unused `src/lifted/` files, image data and function table are removed from this repository.

The imported `src/server/storage.c` and `src/include/sr_storage.h` match upstream generated text exactly.
The local storage generator uses retained metadata and `analysis/structs/storage_defaults.json`.
The defaults come from the checked C initializers at the upstream source commit. No game executable is an input here.
The added `data_globals.csv` contains type metadata, not game assets or private settings.
The old frozen header moves to `analysis/structs/layout_reference.h` as a generator-only size reference.
Generated type headers retain their size and offset assertions.

The local startup file omits upstream recording branches, as the initial SubVulpes import does.
CI now checks storage regeneration and the built server dependency closure.

## Local verification

```text
tools\build_hook.bat
python -I tools/re/gen_types.py
python -I tools/re/gen_storage.py --check
python -I tools/ci/check_native.py
python -I tools/ci/check_repo.py
git diff --check
```

Both normal Release x86 targets build.
The standalone server builds with only native game storage and platform math sources.
The launcher build and updater endpoint remain unchanged.
Storage generation reports 138 objects and matches the checked outputs.
The native dependency check passes after the old runtime files are removed.

A local test executable links the migrated game objects with the upstream storage test.
Its temporary comparison image is test-only under ignored `build/sync/`; it is absent from the server target and package.
The test checks all 138 initial objects against the prior public image data.
It produces 276 matching table/entity digests, passes nine boundary cases and saves/loads 65,536 synthetic client accounts.
Its complete output matches the upstream native-storage verification output.
The test log is `build/logs/storage-sync-native.log`.

Direct-call startup and a named stop request pass with exit code 0 before the storage import.
Separate 75-second idle runs compare the saved S0 server and the migrated server with public data and synthetic settings.
The saved server reports 56.2 ticks/s. The migrated server reports 62.5 ticks/s.
Both start and stop with exit code 0.
These are single idle observations, not evidence of a speedup or multiplayer performance.
Logs: `build/logs/runtime-sync-before.log` and `build/logs/runtime-sync-after.log`.

## Package and launcher

`build/package/SubVulpes-server-v0.0.0-s2.zip` contains 85 entries: 79 public data files and six root files.
The packaged executables match the current build. The packaged config matches the public default.
The SHA-256 sidecar matches:

```text
5348c280eda7adfacd12a1105b729613ebddc1e31be382e274885a3b03dae0af
```

The package records source commit `f1d2c1c`. Later documentation changes do not change its code.
The isolated manual folder uses synthetic settings on UDP port 37585.
Computer Use remains unavailable with `Computer Use native pipe is unavailable: failed to connect native pipe`.
The user confirms the requested manual Start, Stop, restart and close check.
The process monitor independently records server PID 22184 with exit code 0 and launcher PID 23588 with exit code 0.
No process remains in the manual package folder after closure at `2026-10-26T12:19:53Z`.
The monitor captures one server cycle. The complete visual sequence relies on user confirmation.
There is no screenshot or second captured server PID.
Process evidence is in `build/logs/launcher-sync-manual.log`.

Repository privacy checks report 224 files and no problems.
Before the documentation commit, the history audit covers nine commits and 324 unique path/blob pairs with no problems.
The audit checks known forbidden paths, binary signatures and credential patterns. It does not prove every possible secret format is absent.
The whitespace check passes. No private test output is staged.

## S1 and S2 closure review

The closure review covers `b0086b9..d78dfb6` on 2026-10-26.
The primary review checks the import, portable generators, target dependencies and compatibility boundaries.
A separate agent reviews the runtime diff independently and makes no file changes.
Neither review identifies an actionable introduced defect. No runtime correction is needed.

The independent review checks direct-call targets and signatures, storage capacities and offsets, and pointer aliases.
It also checks accesses that previously crossed image regions, startup, x87 math, timers and packet capture.
The packet reader ignores the capture header base, so a zero base does not change packet parsing.
The review uses the source diff and local verification evidence. It is not a multiplayer load or full gameplay proof.

Closure rechecks pass:

- `python -I tools/re/gen_storage.py --check`: 138 objects and 499,799,832 declared bytes.
- `python -I tools/ci/check_native.py`: 67 sources, without image storage or address dispatch.
- Native storage executable: 138 initial objects, 276 state digests, nine boundary cases and 65,536 synthetic client records.

The native test output again matches the upstream verified output exactly.
Earlier build, package, runtime and manual launcher results still apply because the reviewed runtime code does not change.
S1 and S2 are closed for the structural migration with the user's acceptance of the available proof.
The user defers the live client playtest. That check remains a documented follow-up and does not block milestone closure.
No S3 work starts as part of this review.

## Evidence boundaries and rollback

The upstream migration report records a passing full gate on 13 recordings and 2,087 matching calls, plus synthetic suites.
Those differential tools are not imported or rerun in this repository.
The local storage test checks the newly built SubVulpes game objects rather than only reusing upstream executables.
The upstream report lists pre-existing raw differential failures and requests additional model review.
The closure review below covers the imported runtime changes. It does not review the excluded upstream verification tools.
See its `docs/server-c-migration.md` at source commit `6e56d22` for those limits.

No client joins during the local synchronization checks.
These checks do not establish complete gameplay equivalence or performance under multiplayer load.
The server still needs Windows x86 and public game assets. The client remains a separate project phase.
Private settings, test accounts, logs, binaries and downloaded assets stay ignored under `build/`.

Rollback uses local commits: revert the documentation update, then `f1d2c1c`, then `552ba2b`.
No persistent data conversion or account format change needs rollback.
