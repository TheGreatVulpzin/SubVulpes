# Arm pose notes

Branch `feature/arm-target`, from `main` at `e71f61c`.
The bounds, errors and cleanup are in [game API](../plugin-game-api.md#arm-pose). This page records status and evidence.

## Needs status

| Need | Status | Notes |
| --- | --- | --- |
| N79 | Done | The rows `human.arm_pose` and `human.arm_target`. The hook is at the end of `hand_target` in `human_arms.c`. |

## Tests

| Source | Cases |
| --- | --- |
| `tests/n_arms/main.c` and `tests/n_arms/plugin.c`, run by `tests/test_n_arms.py` (CTest `n-arms-api`, modes normal, reuse and reset) | Every error result of both rows and a wrong thread. With the pose of one arm on, the arm code gets the target of the plugin and turns the shoulder muscle another way; the other arm keeps the target of the game. Pose 0 gives the arm back. A new human in the slot and a world reset start with no pose. A disabled plugin gives the arms back while the world goes on. |
| `tests/plugin_game_fixture.c` | The property table has 41 rows. |

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

## Limits

The original client was not tested. Client effect unknown. No in-game test ran.
