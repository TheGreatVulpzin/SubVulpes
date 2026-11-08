# Bot, driver and possession API notes

Branch `codex/n-bots`, merged into `main` on 2026-11-08.
The bounds, errors and cleanup of each operation are in [bot API additions](../game/bot-controls.md). This page records status and evidence.

## Needs status

| Need | Status | Notes |
| --- | --- | --- |
| N58 | Done | `aim_mode` sets player `move_mode` 2 and clears it again. A bot that aims walks at half speed. The owner must accept this. |
| N59 | Done | `chase_mode` switches four effects together. N65 asks for separate flags. |
| N60 | Done | The property `human.despawn`. The plugin author has not confirmed that this field is what the need means. |
| N27 | Done | `street_near` gives one position. N67 asks for candidates and filters. |
| N45 | Done | `possess` gives a player the body of a bot. The old body stays without an owner. N66 asks for the reverse call. |
| N61 | Not started | Waits for the plugin author. |

## Tests

| Source | Cases |
| --- | --- |
| `tests/n_bots/main.c` and `tests/n_bots/plugin.c`, run by `tests/test_n_bots.py` (CTest `n-bots-api`) | The new methods and the property, their bounds and their error results, the cleanup when the plugin stops, and the possession links. |
| `tests/plugin_game_fixture.c` | The property table has 38 rows. |
| `tests/abi_snapshot.txt` | `SvGameAi` has 14 methods and a size of 64. `SvAiStreetPosition` has a size of 44. |

## Evidence

| Check | Result |
| --- | --- |
| Full CTest suite after the merge of the three branches | 29 of 31 passed. The two failures were the ABI snapshot and `game-api`. |
| The same two tests after the fixture and snapshot fixes | Both passed. |
| `n-bots-api` | Passed. |

The fixes in `tests/plugin_game_fixture.c` and `tests/abi_snapshot.txt` came from the merge. The bot code did not need a change.
No in-game test ran. The test room has no case for these operations yet (N69).

## Limits and owner decisions

The original client was not tested for any of these operations. Client effect unknown.
Decisions that are open: is half-speed walking acceptable while a bot aims, does the death counter mean `despawn_timer`, and should the old body of a possession stay ownerless.
