# Development notes for the plugin-engine page

These sections describe how the host implements this API, the checks that proved it and the open work.
They are not part of the SDK reference.

## Core seams

The production host does not install these hooks in this branch. Writes therefore return `SV_STATE` there.
The fixture installs adapters and checks native power copies and the global getter. It does not prove the production call sites.
The first editor must add the following calls in owned files:

- After object-type setup, call `sv_engine_install(SV_ENGINE_OWNER_CLEANUP)` only after owner cleanup exists in `plugin_host.c`.
  Add `SV_ENGINE_GLOBAL_CONSUMERS` only after all global consumers below exist.
  Installation refuses unknown flags, a missing cleanup flag, a repeated install and any native power outside the declared range.
  It checks the whole default snapshot before it changes install state.
- In `plugin_host.c`, call `sv_engine_release_owner(owner)` after every module disable or failed callback/enable, including dependent-module failure and shutdown.
  Keep the module's disable callback before this cleanup. A module can still change a value during its ordinary disable callback.
- In `src/game/bonds.c`, gate only the dead-bone identity-reset block in `setup_bodyrotbody` with
  `sv_engine_integer(SV_ENGINE_RAGDOLL_RESET, 1)`. Keep matrix arithmetic unchanged.
- Use `sv_engine_integer(SV_ENGINE_DEATH_START, 0)` for the inclusive early-collapse threshold.
  Wire the health-at-zero branches in `humans.c`, `human_pose.c`, `human_arms.c` and `bonds.c`.
  Include the positive-health living branches, standing/contact choices, item actions, ragdoll choices, balance and vehicle exit.
  Update crossing-threshold damage checks in `humans.c` and `bullet_sim.c` as one consistent transition.
- The existing packet camera choices in `src/net/netpack.c` also use the health-at-zero split.
  The core owner must account for the chosen threshold through the existing packet fields.
  This area changes no packet field or network source. Original-client presentation at positive health needs a client test.

The native getter accepts a row index and fallback. It checks the index, global scope and type before reading the row.
Before global installation, it returns the call site's fallback. This preserves S0 before wiring.
Keep the strict-negative joint-mode test in `human_bond_modes` distinct: health below zero differs from health at zero.
Keep final death at health -5, its bleed stop, hospital/bandage gate, player release and despawn timing distinct from early collapse.
The nonbleeding living guard in `human_bleed` must use the early threshold, or a positive-health collapsed body can stop losing health.
Do not change the weakness thresholds 25 and 10, or other unrelated health rules.
Use the existing death observation mechanism. This API adds no death event.

## Hazards read before implementation

- `Object.engine_max` is assigned in `object_create.c`, but no code reads it. A write would not implement a speed range.
- `vehicles.c` fixes torque normalization at 7000 rpm, gear ratios and automatic shift thresholds in code.
  A speed control needs a verified consumer contract. This API does not guess which constant the legacy tool meant.
- Power has a native type value and a scaled object copy. Updating only one would split existing and future vehicles.
- Native engine acceleration has no input bound. The API bounds the type power and conversion before any mutation.
- Type indices and metadata indices can index arrays. The API checks them before it reads offsets or defaults.
- The global health threshold controls many branches. Replacing only item drop or ragdoll checks would create inconsistent living/dead behavior.
- Early collapse, strict-negative joints, weakness and final death have different native boundaries.
- Positive-threshold collapse still sends positive health. Existing server and client camera/animation choices need explicit compatibility work.
- A stop helper must ignore other writers and unowned rows. It must restore existing copies and the native type together.

## Work report

Changed files: `src/plugins/sv_game_engine.h`, `src/plugins/plugin_engine.c`,
`tests/systems/engine_plugin.c`, `tests/systems/engine_host.c` and this page.
Inspection used `Get-Content`, `rg` and read-only `git show main:docs/legacy-modules.md`.
A wildcard probe failed on Windows. Explicit paths and `rg -g` searches supplied the native functions and consumers.
No shared source, native record layout, protocol field or x87 control changed. No merge, push or tag ran.

The tick fixture calls every method and result code with wrong owners, another thread, invalid targets,
short records, null/empty/overlong names, unknown names, wrong tags and out-of-range values.
Float cases include a finite value that would overflow the native power conversion, NaN and both infinities.
Failed reads preserve the complete output. Successful endpoint writes are read back, including model 13.
The host uses real object constructors before and after the tick.
It checks native type power and existing/new copies, and one isolated native engine pass compares the resulting rpm step.
The host also checks invalid default snapshots, global native getters, owner-zero cleanup, unrelated-owner cleanup and restored defaults.
A command callback checks metadata after writes and cleanup. The host checks its explicit success marker.

The initial fast build found a signed/unsigned comparison in a test. An explicit cast corrected it.
The next fast build and systems test passed.
Seventeen distinct removals compiled and made the systems test fail.
The cases cover callback context, name/target/record bounds, tags and numeric ranges, the native getter bound,
invalid default capture, owner zero, unrelated-owner cleanup and default restoration.
The missing native getter bound caused an access fault. The exact restored source built and passed again.
The script checks every compiler result and records `git diff` after each byte-exact restoration.
A targeted log read confirmed the expected assertions or access fault in all negative runs.
No failing variant, log or crash artifact is part of the commit.

| Command | Result |
| --- | --- |
| `call <vcvars32.bat>` then `cmake --build build\server --target sv-systems-test sv-systems-fixture` | Passed after the test comparison cast. |
| `ctest --test-dir build/server -R systems-api --output-on-failure` | Passed with the native engine and copy checks. |
| `python -I build/prove_engine.py` | Seventeen removals compiled and failed the test. Exact restored source built and passed. |
| `tools\build_hook.bat` | Passed for the normal server, launcher and all test targets. |
| `ctest --test-dir build/server --output-on-failure` | All eight tests passed in 70.72 seconds. A temporary safe-directory environment allowed Git metadata reads. |
| `python -I tools/re/gen_storage.py --check` | Passed. Checked 138 storage objects and 499799832 bytes. |
| `python -I tools/ci/check_native.py` | Passed. Checked 82 native sources. |
| `python -I tools/ci/check_repo.py` and `git diff --check` | Passed. Checked 312 files without private-file or whitespace errors. |

Proof scripts, original-source backups and logs stay under ignored `build/` paths.
The one-shot fixture does not test an OWNER_CLEANUP-only install's global-write gate or two loaded plugins overwriting the same row.
The original client, positive-health collapse presentation, production global consumers, all-parameter full-solver stability,
type-table reload and a same-process engine reinstallation were not tested.

## For OverRosa

- Speed-range control is not built. The apparent `engine_max` field has no reader, and rpm/gearing remain constants.
  This is a missing native consumer and semantics dependency, not proof that a new client is required.
- Global ragdoll and death-start consumers remain core integration requests in files this area cannot edit.
  The API has typed rows and guarded state, but production writes stay blocked until those consumers exist.
- Positive-health collapse has client effect unknown. This branch adds no protocol extension and claims no original-client animation result.
- No parameter changes a vehicle model, adds a sound or requires a new client event.

## More test notes removed from the reference page

The fixture does not prove a same-process server restart with a new native type setup.
