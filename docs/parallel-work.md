# Parallel work on the plugin API

Two editors can work at the same time if they never edit the same file. This page says who owns what.
Use a git worktree for the second editor: `git worktree add ..\SubVulpes-SVS-codex -b codex/systems-api`.

## System areas

Seven capabilities have a reserved id, a stub header, a stub table and a stub test. Each area has one owner.

| Area | Capability id | Operation in [legacy-modules.md](legacy-modules.md) | Owner |
| --- | --- | --- | --- |
| `ai` | 3 | N20 AI control | Second editor |
| `economy` | 4 | N21 economy | Second editor |
| `mission` | 5 | N22 missions | Second editor |
| `settings` | 6 | N14 settings of the core | Second editor |
| `storage` | 7 | N18 plugin storage | Second editor |
| `engine` | 8 | N17 engine parameters | Second editor |
| `shop` | 9 | N15 shop and phone | Second editor |

The ids 1 (read), 2 (write) and 10 (property table) stay with the first editor, with N16, N19, N23 and the headline method (N4 colors have no support, and the headline method is done).

## The property table

The areas `settings` (N14) and `engine` (N17) must not make one method for each setting. They add rows to the property table.
Write the rows in your own file (`plugin_settings.c` or `plugin_engine.c`) as a static array of `Prop` (see `src/plugins/plugin_props.h`), and add them with `sv_props_add_group` from an install function of your area.
A setting of the world uses the kind `SV_ENTITY_WORLD` (0). A value for each object type uses the slot for the type. Do not edit `plugin_props.c` or `sv_game_props.h`.
The first editor calls your install function from the start of the game at the merge. Give the rows a name that starts with your area, for example `settings.max_players` or `engine.truck_power`.

## Files of an area

The owner of an area edits only these files, where `<area>` is the name in the table:

- `src/plugins/sv_game_<area>.h`: the public table. Add methods after `version`. Do not reorder fields.
- `src/plugins/plugin_<area>.c`: the methods and the table `sv_game_<area>`.
- `tests/systems/<area>_plugin.c`: the plugin side of the test. It runs inside the tick and calls the public API. Return 0 or the failed line.
- `tests/systems/<area>_host.c`: the test program side. `<area>_prepare()` sets up the game before the tick. `<area>_verify()` checks the game after it and prints `<AREA>_PASS`.
- `docs/plugin-<area>.md`: the contract and the evidence.

The stubs print `<AREA>_PASS` and only check that the table exists. Replace them.

## Files with one owner

The first editor owns these files. The second editor writes a request in the report and does not edit them:
`CMakeLists.txt`, `tests/systems/areas.h`, `tests/systems/main.c`, `tests/systems/fixture.c`, `tests/systems/world.c`,
`src/plugins/plugin_game.c`, `src/plugins/plugin_game_internal.h`, `src/plugins/plugin_write.c`, `src/plugins/sv_game_write.h`,
`tests/plugin_game_fixture.c`, `tests/plugin_game_test.c`, `docs/s3.md`, `docs/continuation.md` and `docs/plano.md`.

## How to run

- Fast build, after `vcvars32`: `cmake --build build\server --target sv-systems-test sv-systems-fixture`.
- Test: `ctest --test-dir build/server -R systems-api --output-on-failure`.
- Before a commit: `tools\build_hook.bat`, then `ctest --test-dir build/server --output-on-failure`.
- Run one build at a time on the machine. A `WinError 1455` means low virtual memory. Stop and tell the user.

The test program makes one player, one human, one item, one object and one account with the real constructors (`tests/systems/world.c`).
Add more state in your `<area>_prepare()`.

## Merge

The user merges the branch with `git merge`. Nobody pushes or tags without a request from the user.
