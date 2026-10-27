# Mission API

Capability 5 uses version 1 and `SvGameMission`.
The table appends three methods after size and version.
All methods require the current owner callback, the server thread and a ready world.
They check context before arguments and return `SV_STATE` for invalid context.
Errors leave the game, the scenario counter and output buffers unchanged.

## Methods

| Method | Contract and errors |
| --- | --- |
| `slots(owner, team, out)` | Copy all 16 slots of team 0 to 4. Set the output size first. Invalid team, null output or short output returns `SV_INVALID`. |
| `start(owner, team, input, out)` | Fill one free slot with an existing kind and an existing objective item. Invalid input returns `SV_INVALID`. A wrong item reference returns `SV_NOT_FOUND`. An expired item, unavailable core integration or scenario-id collision returns `SV_STATE`. A full set of eight client slots or an exhausted scenario counter returns `SV_LIMIT`. An item already used by a mission returns `SV_CONFLICT`. |
| `cancel(owner, reference)` | Cancel every slot linked to that scenario through the existing game function. Invalid reference size, team or slot returns `SV_INVALID`. An inactive, stale or wrong reference returns `SV_NOT_FOUND`. A linked native stake or an unrepresentable linked slot returns `SV_STATE`. |

The snapshots contain active state, kind, partner, reward, expiry, site, stake and the item reference.
They contain no internal pointer or account identity.
An inactive slot has no reference. An active slot can contain item reference zero if its item is gone.
A mission reference contains its team, slot, scenario, reset sequence and item reference.
The host checks all of them again for cancellation.
It refuses a slot that now holds another scenario or another item.

The start record must cover `SvMissionStart`.
Kinds are 0 to 6. Partners are teams 0 to 4.
Rewards are 0 to 8388607, within the existing packed reward representation.
Expiry must exceed the current world clock and must not exceed `SV_MISSION_EXPIRE_MAX`.
It uses the raw world clock, not milliseconds or simulation ticks.
The existing packet carries expiry divided by 60.
The site must fit 0 to 32766.
Kind 0 also requires a site below the current building count.
Kind 1 uses the native site sentinel 255. Kinds 2 to 6 use site zero.
The objective must be a live item of an existing mission type, 17 to 22, with positive lifetime.

The caller creates the objective with the item API first.
The start method creates no bot, vehicle, building or new item type.
It uses the existing kinds for display. A plugin gives that display its objective and meaning.
It creates no native stake or price-growth rule.
It keeps the objective alive and reserves one scenario id only after every check passes.
The scenario counter must be nonnegative and below `INT32_MAX`.
The method refuses a counter value that already belongs to an active scenario.
The backing array has 16 slots. Creation uses only slots 0 to 7, which event 12 can identify without overlap.
It handles `scenario_getnextmission` returning 8 to 16 before it indexes either slot table.

Cancellation reuses `scenario_setmissioninactive`.
It refuses a scenario with an active slot from 8 to 15, because its update would alias another team.
It removes the scenario's objectives through the existing lifetime path and sends the existing mission updates.
It refuses linked stake changes until the core has a general guard for that finance path.
Any valid native plugin callback can cancel a valid mission reference, as with the other game write capabilities.
An API-created mission also records its owner for cleanup when that plugin stops.
Those records are host resource metadata, not plugin feature counters or unused game fields.

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

## Original client and hazards

The API reuses mission event 12 and the existing item lifetime path.
It adds no protocol event, field, model, sound or menu.
The original client shows the existing mission kind and numeric data.
It cannot receive arbitrary mission text through event 12.
A plugin can announce custom text with the existing message API instead.
The original client was not tested. These are source findings, not a live client proof.

Event 12 encodes `slot * 2 + active + team * 16` in the same low byte.
With equal payloads, team 0 slot 8 and team 1 slot 0 produce the same event.
For kind 5 and item type 18, both produce parameter `0x00120511`.
The remaining three event integers can also be equal.
No client can distinguish those two updates from the packet alone.
This API therefore reads all 16 server slots but starts missions only in the first eight.
It also refuses cancellation or guarded completion when a linked active slot is 8 or higher.

The implementation read `missions.c`, `scenario.c`, `economy.c`, `logic.c`, `logic_sim.c`,
`logic.h`, the mission event writer and its packet case before adding methods.

- A full team returns slot 16. The client also aliases slots 8 to 15 with other teams.
  The API applies the stricter limit of eight before any write or counter change.
- Native builders can create several entities before a later constructor fails.
  The API uses a caller-created objective and does not expose those unchecked builders directly.
- Native mission code stores item indices. The API also stores item references and rejects stale identities.
- A zero or reused reference can otherwise identify the wrong objective.
  Cancellation compares the current resolved item with the slot, and lifecycle cleanup protects a reused item.
- Scenario ids can overflow or collide with active missions. The API refuses both cases.
- Rewards share one packed integer with other fields. The API bounds the reward before sending the existing event.
- Native completion adds money without a range check. The proposed core guard checks the full linked payout first.
- Native completion can split unrelated companies after it changes prices.
  The guard refuses unsafe prices before this path runs for an API-owned mission.
- Stake cancellation changes prices and shared stakes. The public cancel method refuses that path instead of guessing its policy.
- Core cleanup needs a nonzero owner. The host guard refuses zero before it examines unowned bindings.

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
