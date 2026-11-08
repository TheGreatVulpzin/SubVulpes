# Level API notes

Branch `codex/n-world`, merged into `main` on 2026-11-08.
The contracts of each call are in [level API](../game/level-api.md). This page records status and evidence.

## Needs status

| Need | Status | Notes |
| --- | --- | --- |
| N26 | Done | `world_reset` already existed. The row was out of date. |
| N52 | Done | `building_count`, `building`, `base` and `area`. |
| N39 and N10 | Done | `block` reads one cell. `surface` gives the material of a level hit. |
| N40 | Done | `block_set` clears or sets a solid block. The host restores the cells when the plugin stops. Client effect unknown. |
| N8 | Not done | The session did not decide if a safe delete of the body of a real player exists. The plugin still uses `set_health`. |
| N29 | Not done | The session did not check whether a health above 100 is safe for a human. |
| N49 | Not done | No train read or write exists. The session ended before this row. |

## Tests

| Source | Cases |
| --- | --- |
| `tests/plugin_world_test.c` and `tests/plugin_world_fixture.c`, run inside `game-api` | The new reads, the block edit, its limits, the conflict between owners and the restore at stop. The markers `WORLD_API_PASS` and `WORLD_RESTORE_PASS` must print. |
| `tests/abi_snapshot.txt` | The new records and the new methods of `SvGameRead` and `SvGameWrite`. |

## Evidence

| Check | Result |
| --- | --- |
| Full CTest suite after the merge of the three branches | 29 of 31 passed. `game-api` failed because of the fixtures of the other branches, not because of the world code. |
| `game-api` after the fixture fixes | Passed, with both world markers. |

## Limits and owner decisions

A block edit changes the server only. The original client may not show it, and its prediction may disagree with the server. Client effect unknown.
The test room must check N40 in the game. The row N69 holds that work.
The edits are not saved. They end when the plugin stops or the round resets.
