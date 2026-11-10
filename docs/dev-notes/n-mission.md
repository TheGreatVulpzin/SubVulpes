# Shared acquisition notes

Branch `feature/mission-acquisition`, from `main` at `e71f61c` (release 0.3.0 plus N65 and N66).
The bounds, errors and cleanup are in [mission API](../plugin-mission.md). This page records status and evidence.

## Needs status

| Need | Status | Notes |
| --- | --- | --- |
| N78 | Done | `SvGameMission.acquisition` starts the native acquisition of kind 0 or 1 for the teams in a mask. |

## Tests

| Source | Cases |
| --- | --- |
| `tests/n_mission/main.c` and `tests/n_mission/plugin.c`, run by `tests/test_n_mission.py` (CTest `n-mission-api`) | Every error result, a wrong thread and the missing capability. No building and no road give `SV_FAILED` and leave the counter. A chase for two teams gives one slot of the same scenario and objective to each. A team with eight client slots in use gives `SV_LIMIT` before any change. Cancel ends the slots of both teams. Stop ends them, but not after a world reset or after the game ended the scenario and gave its number to another. |
| `tests/abi_snapshot.txt` | `SvGameMission` has 4 methods and a size of 24. |

## Removal proofs

Each check was taken out, the test was built again, and `n-mission-api` failed.

| Check taken out | Failed at |
| --- | --- |
| Free client slot of each team | `tests/n_mission/plugin.c`, mode `full` |
| Kind bound | `tests/n_mission/plugin.c`, setup |
| Team mask bound | `tests/n_mission/plugin.c`, setup |
| End of the slots on stop | `tests/n_mission/main.c` line 94 |
| Reset sequence on stop | `tests/n_mission/main.c` line 76 |
| End of the record with its scenario | `tests/n_mission/main.c` line 88 |

## Evidence

| Check | Result |
| --- | --- |
| Full CTest suite after a fresh rebuild (MSVC 14.44, `/wd4127`) | 25 of 25 passed after the rebase on `main`, with `n-mission-api`, `n-roads-api` and `abi-additive`. The tests of the official plugins left with their source. |
| `python -I tools/ci/check_abi_additive.py` against the snapshot of `main` | The snapshot only adds. |
| `python -I tools/ci/check_repo.py` | Clean. |

## Limits

The original client was not tested. Client effect unknown.
No in-game test ran.
