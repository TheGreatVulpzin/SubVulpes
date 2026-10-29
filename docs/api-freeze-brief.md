# API freeze and next API work: brief for the second editor

Date: 2026-10-28. The first editor ran out of budget. This page says what is done, what is left, and in which order.
Read [the ABI rules](plugin-abi.md) first. Then read [the SDK plan](sdk-plan.md). Do the work of this page before WP1 of the SDK plan, because the SDK ships the headers, and a header that changes after that costs more.

## Done by the first editor

- A request for a table version accepts a range (`SV_VERSION_ACCEPTED`). Today the range is 1 to 1 for every table.
- A 64-bit event mask in `SvHost.subscribe`, the macros `SV_EVENT_EXT(i)` and `SV_EVENT_EXT_BIT(i)`. Test: the mode `ext` in `tests/plugin_fixture.c`.
- The ABI rules in [plugin-abi.md](plugin-abi.md).
- The file ownership table in the SDK plan. With the first editor away, the second editor may change any file. Keep each change small and tested.

## Freeze work that is left (do first, about 1.5 days)

### F1 ABI snapshot test (0.5 day)

Write `tests/abi_snapshot.c`. It prints `sizeof` for every public record and table and prints the count of function members of each table. Compare with `tests/abi_snapshot.txt` in a CTest test (`abi-snapshot`).
The public records are in the headers `sv_plugin.h`, `sv_config.h`, `sv_plugins_api.h`, `sv_game.h`, `sv_game_read.h`, `sv_game_write.h`, `sv_game_props.h`, `sv_game_events.h`, `sv_game_ai.h`, `sv_game_economy.h`, `sv_game_engine.h`, `sv_game_mission.h`, `sv_game_settings.h`, `sv_game_shop.h` and `sv_game_storage.h`. The list of names is: SvStatus, SvEvent, SvHost, SvPlugin, SvConfig, SvConfigEvent, SvPlugins, SvPlayerState, SvHumanState, SvItemState, SvObjectState, SvAccountState, SvBoneState, SvInventoryState, SvWorldState, SvLineHit, SvGameRead, SvGameWrite, SvValue, SvPropInfo, SvGameProps, SvGameEvent, SvChatEvent, SvDamageEvent, SvAiThreat, SvAiThreats, SvAiInput, SvAiStreetPoint, SvGameAi, SvCompany, SvGameEconomy, SvEngineValue, SvEngineInfo, SvGameEngine, SvMissionRef, SvMissionState, SvMissionSlots, SvMissionStart, SvGameMission, SvSettingValue, SvSettingInfo, SvSettingEvent, SvGameSettings, SvPhoneEvent, SvPhoneState, SvShopCatalog, SvShopCatalogWrite, SvGameShop, SvStorageKey, SvStorageValue and SvGameStorage.
Also print `offsetof` of the first and the last member of each table. Build it with MSVC x86 like the other tests. A removal proof: change a field type and see the test fail.

### F2 Header audit (0.25 day)

Read every public header against the rules of `plugin-abi.md`. Write the findings in `docs/plugin-abi-audit.md`: a record without `size`, a unit that differs, an enum value that two names share, a comment that disagrees with the code. Fix what is safe (comments, names of internal macros). Put a break that would need a new ABI in the report and do not change it.

### F3 Gate for actions that wait for a plugin (0.5 day)

Today only the phone call waits for the decision of a plugin (`sv_shop_phone_call_wait` and `sv_shop_poll` in `plugin_shop.c`). Make that pattern general before another decision is added.
Create `src/plugins/plugin_actions.h` and `.c`:

```c
typedef SvResult (*SvActionDeliver)(int32_t key);   /* builds the event and calls sv_plugins_decide */
enum { SV_ACTION_PROCEED = 1, SV_ACTION_HOLD = 0, SV_ACTION_DENIED = -1 };
int sv_action_gate(uint32_t event_kind, int32_t key, SvActionDeliver deliver);
void sv_actions_poll(void);   /* called from the poll hook in plugin_game.c */
```

The core calls `sv_action_gate` where a decision belongs. With no plugin that asked for the kind, it returns PROCEED at once. Otherwise it queues the pair (kind, key) and returns HOLD. At the next poll the host calls `deliver(key)`. A result of `SV_DENIED` makes the next gate call return DENIED once. Any other result makes it return PROCEED once. The table holds 256 entries. When it is full, the gate returns PROCEED, so the game never stops. A stale key is the problem of `deliver`: it returns a result other than `SV_DENIED`.
Move the phone call wait to it, and keep `hold_check` in `tests/systems/shop_host.c` passing. A gate for an action that the player starts with a button (an edge) must run the action itself when the answer allows it, because the button is gone in the next tick. Document that rule in the header.
Test it with a fake event in `tests/plugin_game_test.c`: no subscriber, a subscriber that allows, a subscriber that denies, a full table.

### F4 Cost of a tick (0.25 day)

No number exists for the cost of the plugin hooks. Write a timing run (for example in `tests/plugin_game_test.c`, printed, not a gate) with `QueryPerformanceCounter`:
10,000 calls of `sv_shop_poll`, `sv_mission_before_tick`, `sv_game_event_poll`, `sv_actions_poll`, and `sv_ai_before_player` for 256 players; one `sv_plugins_tick` with one idle plugin; one `within` query of radius 4096 with 512 humans and 1,024 items.
Write the numbers in `docs/plugin-abi.md` under "Cost". A target: all hooks together under 100 microseconds for each tick with 256 bots and no plugin that subscribes. If a loop is slower, fix it (for example keep a count of waiting phones so the poll can skip the scan).

## API work that is left (the first editor does it, after the ABI reset)

The owner decided on 2026-10-28: the first editor builds R1 to R9, and not the second editor. They start after the planned ABI change on the branch `codex/abi-reset` is merged. Each item still follows the rules below.

| Id | Work | Design notes |
| --- | --- | --- |
| R1 | Reads of the connection | A method `connection(player, SvConnectionState*)` at the end of `SvGameRead`: ip (as 4 bytes), port, idle ticks, spectate flag. Source: `Connection` in `sr_types.h` (`ip`, `port`, `idle_ticks`, `spectate`). Do not show it to a player without the admin check; that is the job of the plugin. |
| R2 | Occupants of a vehicle | Add `occupants[8]` (entities of humans, 0 for an empty seat) at the end of `SvObjectState`. Scan the humans whose `vehicle_id` is the object. Done and tested in the unit test (2026-10-29). |
| R3 | Road graph (N27) | Three reads: `intersection(index)`, `road(index)` and `street_near(point, radius, flags)`. The data is `g_city.intersections[1024]` and `g_city.roads[512]` (`sr_types.h`). Add `road_count` and `intersection_count` at the end of `SvWorldState`. `street_near` gives the nearest lane point to the position, the direction and the next intersection, and it skips a lane that has a vehicle within 6 units. `scenario_createcar` in `scenario.c` shows how the game puts a car on a lane. Mark the lane mapping "read from code, not tested". |
| R4 | Bone mass (N19) | A row `human.bone_mass`: float, 15 slots, get, set and add, range 0.5 to 500. The bonds read `Human.bones[k].mass` live, so a write works at once. |
| R5 | Reset the round (N26) | A write method `world_reset(void)` that sets a flag. The main loop runs `resetgame()` at the start of the next tick, outside the simulation. |
| R6 | Account save event (N33) | Post a notice from `saveaccountsserver` and from the single-account save, with the account reference. Use a numbered event (`SV_EVENT_EXT(0)`). |
| R7 | Delete the body of a real player (N8) | A write method that kills and removes the body at a safe point, with the links of the player kept whole. |
| R8 | Block reads and edits (N39, N40) | Material at a hit and a block cell (read). Block set and clear (write). Mark "client effect unknown". |
| R9 | Timers and plugin random (N41) | `after(owner, ticks, id)` and `every`, delivered as a numbered event, and a random state for each plugin. It lives in `plugin_host.c`. Replay contract (owner decision, 2026-10-29; see the replay idea in [ideas.md](ideas.md)): (1) The random state of a plugin is its own. It never calls `os_rand()`, so a plugin cannot move the random stream of the game. (2) The seed comes from the seed of the game and the ID of the plugin. It never comes from a clock, `time()` or an address. (3) The state is one or two numbers for each plugin, so a snapshot can hold it. Timers count ticks, not milliseconds. Say in the header that a native plugin which reads a clock, a file or the network is outside this contract, and that only a sandboxed host can enforce it. |
| R10 | Launch an item and spin it | Rows `item.velocity` and `item.angular_velocity` (vector, 1 slot, GET, SET and ADD, same bound as `object.velocity`). An item at rest gets an exact launch speed from one set. A plugin that holds items in place with `teleport` (the test 59 of the test room) sets the speed to zero after each move, because `teleport` keeps the speed and gravity builds it up between two moves. Test: the test room launches an item at an angle and a plugin reads the speed back. The row `item.frozen` (int, 0 to 1) holds an item in the air through the rest flag. Done in the unit test; the test room case is open. |

Every item follows the rules of `plugin-abi.md`: it goes at the end, it has `size`, it has a focused test, it has a removal proof if it is a security check, and it adds a line to `docs/plugin-game-api.md` and to `docs/plugin-coverage.md`. Each new method needs the ABI snapshot to change in the same commit.

## Checks before each commit

`tools\build_hook.bat`, `ctest --test-dir build/server`, `python -I tools/re/gen_storage.py --check`, `python -I tools/ci/check_native.py` and `python -I tools/ci/check_repo.py`. Lint the prose with `ste-lint.py --fail-over 2.5`. No push, no tag, no merge, no AI coauthor line.
