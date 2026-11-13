# Arm pose notes

Branch `feature/mission-and-arms`, from `main` at `ca4ecb2`.
The bounds, errors and cleanup are in [game API](../plugin-game-api.md#arm-pose). This page records status and evidence.

## Needs status

| Need | Status | Notes |
| --- | --- | --- |
| N79 | Done | The rows `human.arm_pose` and `human.arm_target`. The hook is at the end of `hand_target` in `human_arms.c`. |

## Tests

| Source | Cases |
| --- | --- |
| `tests/n_arms/main.c` and `tests/n_arms/plugin.c`, run by `tests/test_n_arms.py` (CTest `n-arms-api`, modes normal, reuse and reset) | Every error result of both rows and a wrong thread. With the pose of one arm on, the arm code gets the target of the plugin and turns the shoulder muscle another way; the other arm keeps the target of the game. Pose 0 gives the arm back. A new human in the slot and a world reset start with no pose. A disabled plugin gives the arms back while the world goes on. |
| `tests/plugin_game_fixture.c` | The property table has 47 rows. The two rows are the last ones. |

## Removal proofs

Each check was taken out, the test was built again, and `n-arms-api` failed.

| Check taken out | Failed at |
| --- | --- |
| The hook in `hand_target` | `tests/n_arms/main.c` line 52 (the shoulder turn did not change) |
| Range of `human.arm_pose` (widened) | `tests/n_arms/plugin.c` (error checks) |
| Length of `human.arm_target` (widened) | `tests/n_arms/plugin.c` (error checks) |
| The end of the pose when its plugin stops | `tests/n_arms/main.c` line 78 |
| The human reference and world check of the hook | `tests/n_arms/main.c` line 64 |

The side bound of `sv_props_arm_target` has no removal proof: the mask (0 to 3) already refuses side 2. The bound stays
so that a negative side never makes a negative shift.

## Evidence

| Check | Result |
| --- | --- |
| Full CTest suite after a fresh rebuild (MSVC 14.44, `/wd4127`) | 26 of 26 passed on `ca4ecb2` plus N78 and N79, with `n-arms-api` in its three modes. |
| `python -I tools/ci/check_abi_additive.py` against the snapshot of `main` | The snapshot only adds. |
| `python -I tools/ci/check_repo.py` | Clean. |

## Limits

The original client was not tested. A playtest with a punch plugin on this hook showed the guard and the punch on
the 0.24 client, and the hands hit NPCs. The removal proofs above were taken on `e71f61c`, before the rebase.
