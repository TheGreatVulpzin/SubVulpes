# Development notes for the plugin-mission page

These sections describe how the host implements this API, the checks that proved it and the open work.
They are not part of the SDK reference.

## Host guards and core seams

Production installation remains pending outside this area's file ownership.
The fixture installs and calls the proposed seams directly.
Without `sv_mission_install()`, `start` returns `SV_STATE` after its input and capacity checks.
Reads and validated cancellation do not require installation.
Install only after the core owner completes these integrations:

1. Call `sv_mission_install()` at game-host startup.
2. Call `sv_mission_before_tick()` at the safe boundary and after objective retirement, before that pool slot can be reused.
3. Before native completion of an API-owned mission, call `sv_mission_before_complete(team, slot)`.
   If it returns zero, leave the mission and balances unchanged.
4. Call `sv_mission_release_owner(owner)` before the host clears that plugin's resources.
5. Add mission notice and decision rows to the shared N16 event mechanism described below.

The completion guard sums all linked rewards for the winning team in 64 bits.
It checks every active member's money before the native function adds the reward.
It refuses changed stakes, growth factors and unsafe stock prices for an API-owned scenario.
It passes existing unmanaged missions without replacing their S0 behavior.
The fixture runs the native completion function only after this guard succeeds.
The native game float and x87 code remains unchanged.

The lifecycle guard cancels expired API missions and clears bindings whose scenario or reset changed.
For a retired item, it clears the mission's raw item index before native cancellation.
That prevents cancellation from killing a new item that reused the old slot.
The item-retirement call site must run before reuse and before the world's mission lifetime pass.
The fixture tests the guard directly. It does not prove those production call sites.

## Shared events

Read-only inspection covered `main:docs/plugin-events.md`, `sv_game_events.h`, `plugin_events.c` and `plugin_game_events.c`.
The shared mechanism from commits `32c5f1f` and `bbd09cc` is not in this branch.
This area adds no second queue, subscription system or callback dispatcher.
The existing spawn, death and leave notices must remain the shared notices.
A leave reference is stale at delivery by design.

These requested N22 event rows remain integration work:

- Start, completion and expiry notices must use the existing N16 queue and safe delivery before the tick callback.
  Capture the mission reference and copied state before the core clears the slot or retires the objective.
- A reward decision must validate the final amount against the packed limit and the completion guard before any payout.
  A callback inside the simulation cannot use the full write API.
  Move that decision to a safe boundary or use the shared restricted decision phase when the core implements it.
- The decision before `createscenario_world` chooses a builder must use the existing safe decision dispatcher.
  The current builder runs inside game logic. Do not call unrestricted plugin callbacks from that calculation.
  The core must prepare the choice at a safe boundary and preserve the default path without subscribers.

The common event queue currently accepts only its declared notice records.
The core owner must add the mission payload cases and event masks there and in `sv_plugin.h`.
Those files have one owner. This branch does not copy or modify them.

## Tests and work report

Changed files are `src/plugins/sv_game_mission.h`, `src/plugins/plugin_mission.c`,
`tests/systems/mission_plugin.c`, `tests/systems/mission_host.c` and this page.
The test uses the real systems world, real item constructors and existing player constructors.
The plugin calls every public method in the tick and checks every result code.
It checks wrong-thread calls, wrong references, short records, full slots and unchanged outputs.
The host checks the exact scenario counter, mission event 12, completion, expiry and owner cleanup.
It also checks money overflow and item reuse through the proposed core guards.
The AI test changes the reset sequence before mission verification.
The mission test first proves the old reference stale, then starts fresh missions in a safe command callback.

Inspection used `Get-Content` and `rg` on the listed implementation paths.
Read-only `git show main:<path>` inspected the four shared event files without a merge.
No file in `src/net`, no write-table file and no generated record changed.
No push or tag ran.

| Command | Result |
| --- | --- |
| `call <vcvars32.bat>` then `cmake --build build\server --target sv-systems-test sv-systems-fixture` | Passed for the initial API and expanded host checks. |
| `ctest --test-dir build/server -R systems-api --output-on-failure` | Passed after both revisions. |
| `python -I build/prove_mission.py` | Eighteen removals compiled and made the systems test fail. The restored build and test passed. |
| `python -I build/prove_mission.py full-slots cancel-client-slot completion-client-slot` | Reproved the stricter slot bound and proved both linked client-slot guards. Mission total: 20 guards. Restored test passed. |
| `tools\build_hook.bat` after the client-slot correction | Normal server, launcher and all test targets passed. |
| `ctest --test-dir build/server --output-on-failure` | All eight tests passed in 56.55 seconds. A temporary Git safe-directory environment allowed worktree metadata reads. |
| `python -I tools/re/gen_storage.py --check` | Passed. Checked 138 storage objects. |
| `python -I tools/ci/check_native.py` | Passed. Checked 82 native sources. |
| `python -I tools/ci/check_repo.py` and `git diff --check` | Passed. Checked 309 files without private-file or whitespace errors. |

The proof script records `git diff` after each restoration.
Build scripts, binaries and test logs stay in ignored `build/` paths.
The original client, a full live world, the production hook sites and shared mission event delivery were not tested.
The fixture does not prove arbitrary legacy stake unwinding or the native builders under pool exhaustion.

## For OverRosa

- No implemented method needs a new client protocol.
  Arbitrary mission HUD text and new mission kinds are not built because the original client does not receive them.
  Existing messages and existing kinds are the supported alternative.
- Mission notices, reward decisions and scenario-choice decisions remain unbuilt in the shared core files.
  They require N16 payload cases and safe hook sites, not a new client.
- Start, cancel or complete a scenario through slots 8 to 15 is not built.
  Existing event 12 aliases those indices with another team. A distinct representation needs a client change.
- Native stake cancellation is not built in this slice because its price and shared-stake arithmetic needs a general guard.
  This is a core safety dependency, not a known client limitation.
- AI still needs the four core integrations listed in `plugin-ai.md`.
  Automatic seat exit remains unavailable in this branch because the shared exit exists only on `main`.
  No merge or private replacement of that exit is built here.
- Settings, storage, engine and shop remain the next areas at this checkpoint.
The user requested continuation in all four after this mission commit.
  They need the general setting/parameter tables, private storage namespaces and shared phone decisions from the updated rules.
  Engine force and speed range have client effect unknown. No client effect is claimed for those unbuilt parameters.

The AI checkpoint is commit `e9a0bd7`. The economy checkpoint is commit `6a92b84`.
Their contracts, commands and limits are in `plugin-ai.md` and `plugin-economy.md`.
This report stays in the area's allowed documentation file.
The current file-ownership rule prevents creating the earlier requested separate `docs/codex-report.md`.
