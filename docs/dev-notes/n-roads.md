# Street map, trigger and aim snap notes

Branch `feature/roads-trigger-aim`, from `main` at `d9cbbf4`, with `main` at `125be0f` merged.
The bounds, errors and cleanup of each operation are in [bot API additions](../game/bot-controls.md). This page records status and evidence.

## Needs status

| Need | Status | Notes |
| --- | --- | --- |
| N63 | Done | `road` and `intersection` copy the street map, with the light of each side. N64 still asks for open routes. |
| N75 | Done | The property `item.trigger_ticks`. No cooldown row: `item_type.fire_delay` already sets the cooldown of a type. |
| N77 | Done | The policy `aim_snap`. N68 still asks for `aim_at`, accuracy and perception limits. |

## Tests

| Source | Cases |
| --- | --- |
| `tests/n_roads/main.c` and `tests/n_roads/plugin.c`, run by `tests/test_n_roads.py` (CTest `n-roads-api`) | The reads and their copies, the property and the policy, their bounds and error results, a wrong thread, a bad graph, the missing capability, slot reuse, world reset and stop. A held 9 mm fires once, and a write of 0 lets it fire again two updates later. With the policy a bot turns to its target in one update; without it, by 1/16. |
| `tests/plugin_game_fixture.c` | The property table has 39 rows. |
| `tests/abi_snapshot.txt` | `SvGameAi` has 17 methods and a size of 76. `SvAiRoad` has a size of 52 and `SvAiIntersection` 68. |

## Removal proofs

Each check was taken out, the server and the test were built again, and `n-roads-api` failed at the line shown.

| Check taken out | Failed at |
| --- | --- |
| Road index bound | `tests/n_roads/plugin.c` line 29 |
| Intersection index bound | `tests/n_roads/plugin.c` line 32 |
| Graph count check | `tests/n_roads/plugin.c` line 154 |
| Output size check of `road` | `tests/n_roads/plugin.c` line 37 |
| Value check of `aim_snap` | `tests/n_roads/plugin.c` line 71 |
| Range of `item.trigger_ticks` (widened to -100 to 100000) | `tests/n_roads/plugin.c` line 51 |

## Limits

The original client was not tested for any of these operations. Client effect unknown.
No test room case covers them yet (N69).

## Evidence

| Check | Result |
| --- | --- |
| Full CTest suite after a fresh rebuild (MSVC 14.44, `/wd4127` for warning C4127 of this compiler in older tests) | 34 of 34 passed, with `n-roads-api`, `n-bots-api`, `systems-api`, the ABI snapshot and `abi-additive`. |
| `python -I tools/ci/check_abi_additive.py` against the snapshot of `main` | The snapshot only adds. |
| `python -I tools/ci/check_repo.py` | Clean. |

No in-game test ran for these operations. The tests above run the game code of the weapon and the bot AI without a client.
