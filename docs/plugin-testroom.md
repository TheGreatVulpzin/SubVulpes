# Test room plugin

Status: version 0.1.2, built on 2026-10-27. It is a tool for developers. The server package does not carry it.
The plugin `sv.testroom` runs the plugin API against a live server, with a person who watches the result in the game.
The automatic tests prove the return codes and the game records. They cannot prove what the original client shows. This plugin covers that part.

## Use

1. Build the plugin: `cmake --build build\server --target sv-testroom`. The output is `build\server\plugins\sv.testroom\`.
2. Stop the test server. Run `tools\plugins\deploy_testroom.ps1 -ServerDir <folder>`. It packs the ZIP, installs it, approves the exact DLL and enables the module (see the [plugin SDK](plugin-sdk.md)). It approves native code, so use it only on a test server.
3. Start the server, join with the client and type `/admin PASSWORD` in the chat. The command needs an administrator.
4. Type `/test start`.

The plugin sends each line to you only. It says the number and the name of the test, what it does and what you should see.

| Command | Effect |
| --- | --- |
| `/test start` | Starts at test 1 and clears the old results |
| `/test yes` | The test worked. Go to the next test |
| `/test no` | The test did not work. Go to the next test |
| `/test skip` | Skip the test |
| `/test go` | Repeat the action of the test, for the tests that have one (sound, vehicle exit, aim) |
| `/test <number>` | Go to a test |
| `/test list` | One line for each test, with its result |
| `/test report` | The count of results and the names of the failed tests |
| `/test stop` | Stop, undo the last test and show the report |

When a test makes things (a car, a bot, items) or changes your body (health, bleeding, looks), the next test, `/test stop` or the end of the list undoes it.
An API call that returns an error fails the test at once, with the error number in the chat.
Each result goes to the server log as `TESTROOM <number> <name> PASS|FAIL|SKIP code=<error>`.

## The tests

| Id | Name | What the plugin does | What you should see |
| --- | --- | --- | --- |
| 1 | `chat_all` | `message` | A line in the chat |
| 2 | `chat_private` | `message_to` | A line for you only |
| 3 | `chat_admin` | `message` of the admin kind | A line, because you are an administrator |
| 4 | `sound` | `sound`, one index for each `go` | A different sound each time |
| 5 | `headline_replace` | `headline` on line 0 | The first line of a newspaper has the test text |
| 6 | `headline_add` | `headline` at the end | A new last line in a newspaper |
| 7 | `teleport` | `teleport` 25 units to the side | You jump sideways |
| 8 | `health` | `human.health` 50 | The health bar falls to half |
| 9 | `bleeding` | `human.bleeding` 1 | Blood |
| 10 | `stun` | `human.stun` 300 | Your body staggers for 5 seconds |
| 11 | `push` | `human.bone_velocity` on 15 bones | You fly up |
| 12 | `item_in_hand` | `item_create`, `item_equip` | A bandage in your hand |
| 13 | `magnum` | `item_create`, `item_attach`, `item_equip` | A loaded Magnum in your hand |
| 14 | `car` | `object_create` | A car beside you |
| 15 | `npc` | `npc_create` | A bot beside you |
| 16 | `vehicle_exit` | `vehicle_exit` after `/test go` | You leave the car without flying away |
| 17 | `aim` | `npc_create`, then `aim` for each `go` | The answer matches what you look at: the bot, the ground or the sky |
| 18 | `looks` | `set_looks`, one look setting for each `go` | Each go changes one part of your look |
| 19 | `damage_dealt_zero` | `human.damage_dealt_scale` 0 | A bot that takes no damage from your Magnum |

## Not covered yet

The damage decision, the chat and join decisions, the notice events, the plugin settings, the object and item velocity rows and the line query on a car need a second step.
The name of an item (N43) needs a table row that does not exist yet.
