# Test room plugin

Status: version 0.2.8, built on 2026-10-28. It is a tool for developers. The server package does not carry it.
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
| `/test go` | Repeat the action of the test, or take its next step, for the tests that have one (sound, vehicle exit, aim, bots, missions) |
| `/test kick NAME` | Test 57. Removes the player with that name. See the note below |
| `/test again` | Say the name, the action and the expected result of the current test again |
| `/test <number>` | Go to a test |
| `/test list` | One line for each test, with its result |
| `/test report` | The count of results and the names of the failed tests |
| `/test stop` | Stop, undo the last test and show the report |

When a test makes things (a car, a bot, items) or changes your body (health, bleeding, looks), the next test, `/test stop` or the end of the list undoes it.
An API call that returns an error fails the test at once, with the error number in the chat.
Every line that the room says to you also goes to the server log, with the prefix `SAY`. A line that leaves the chat too fast stays in the log.
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
| 15 | `npc` | `npc_create` with the name Guard | A bot named Guard beside you |
| 16 | `vehicle_exit` | `vehicle_exit` after `/test go` | You leave the car without flying away |
| 17 | `aim` | `npc_create`, then `aim` for each `go` | The answer matches what you look at: the bot, the ground or the sky |
| 18 | `looks` | `set_looks`, one look setting for each `go` | Each go changes one part of your look |
| 19 | `car_teleport` | `object_create`, then `teleport` of the car for each `go` | The car jumps 10 units at once |
| 20 | `vehicle_enter` | `object_create`, then `vehicle_enter` after `/test go` | You sit in the driver seat |
| 21 | `one_round` | `item_create`, `item_attach`, `item.ammo` 1 | One shot, then the Magnum does nothing |
| 22 | `npc_name` | `npc_create` with a name, then `set_name` for each `go` | The name above the bot changes at once |
| 23 | `npc_speak` | `speak` for each `go` | A speech bubble above the bot |
| 24 | `tracer` | `bullet`, `bullet_hit` for each `go` | A tracer and a spark where you aim, with no damage |
| 25 | `strong_magnum` | `item_type.bullet_speed`, `bullet_type.mass` | The Magnum kills a bot in a few shots |
| 26 | `bomb` | `within`, `human.health`, `human.bone_velocity`, `sound`, `bullet_hit` for each `go` | The bot flies up and loses health |
| 27 | `chat_prefix` | The chat decision with `replacement` | Your chat text appears with the prefix `[TEST]` while it is on |
| 28 | `notice_events` | The events round, vehicle, shot, item and vehicle gone | A chat line for each event of yours |
| 29 | `damage_dealt_zero` | `human.damage_dealt_scale` 0 | A bot that takes no damage from your Magnum |
| 30 | `collapse_threshold` | `SvGameEngine.set` of `human.death_start` 60, `human.health` 50 | Your body falls limp at half health |
| 31 | `ai_walk` | `SvGameAi.destination`, then `override_input` with a yaw of 3.0 for each `go` | The bot walks 25 units. After a go it stops and turns about 90 degrees from its path |
| 32 | `phone_claim` | The phone call decision (`SV_DENIED` for the number 1234) | A chat line, and the phone does not ring |
| 33 | `storage_counter` | `SvGameStorage.get`, `put`, then `erase` for each `go` | A counter that grows, also after a restart |
| 34 | `read_account` | `account` | Your name and money |
| 35 | `read_inventory` | `inventory` for 7 slots | The count of items in each slot |
| 36 | `read_line` | `line` down through you | Kind 1 (level) at your feet |
| 37 | `read_headline` | `headline` read and `world` | The first news line |
| 38 | `props_list` | `SvGameProps.info` for all ids | At least 34 properties |
| 39 | `add_money` | `add_money` 100, then minus 100 at the undo | Your money rises by 100 |
| 40 | `disown` | `item_create`, `disown` | The item stays after `/test stop` |
| 41 | `ai_threats` | `threats` for each `go` | The count rises after you wave in front of the bot |
| 42 | `ai_waypoints` | `waypoints` with 3 points, then 0 points for each `go` | The bot walks a triangle, then stops |
| 43 | `ai_target` | `target` with your body | The bot turns to you |
| 44 | `ai_duel` | `target`, `shot_chance`, `perception`, `aim_mode` | Shots at the head, then no shots, then shots at the torso |
| 45 | `ai_seated` | `vehicle_enter` of a bot, `seated_target` | A bot in seat 1 shoots a bot outside the car |
| 46 | `economy_read` | `SvGameEconomy.company` for 5 companies | Prices and shares like the stock list |
| 47 | `economy_change` | `set_price`, then `set_shares` and `add_stocks` for each `go` | Price 123 and 10 more of your stocks |
| 48 | `mission_read` | `SvGameMission.slots` for 5 teams | The counts of active missions |
| 49 | `mission_start` | `item_create` of an objective, `start`, then `cancel` for each `go` | A mission in the list, then it leaves |
| 50 | `shop_catalog` | `SvGameShop.catalog`, `catalog_replace` | The first row of each shop costs 1 |
| 51 | `shop_phone` | `item_create` of a cell phone (item type 23), `phone`, `phone_idle` for each `go` | You hold a cell phone. After you dial, the state of the call shows, then the call ends |
| 52 | `setting_list` | `SvGameSettings.describe`, `get` | Three settings with ranges |
| 53 | `setting_bots` | `SvGameSettings.set` of `bot_count`, the setting event | A chat line, and 2 bots in the city |
| 54 | `engine_list` | `SvGameEngine.describe`, `get` | Three parameters with values |
| 55 | `config_read` | `SvConfig.get_int`, `get_float`, `get_text`, the config event | The values hello, 3 and 1.50, and a chat line after a change |
| 56 | `plugins_calls` | `SvHost.subscribe` with an extended bit, `refuse`, `find` | All three answers match the wanted codes |
| 57 | `kick` | `kick` for the player that `/test kick NAME` names | The player leaves and cannot join again |
| 58 | `pitch_scale` | `sound` with 25 pitches from 0.5 to 2.0, one every 15 ticks | A rising scale in even steps |
| 59 | `item_grid` | `item_create` 16 times, then `teleport` of every item each tick | The items move as one smooth picture |
| 60 | `headline_frames` | `headline` on line 0, one new text every 60, 20 or 6 ticks | A moving pattern in the open newspaper |
| 61 | `entity_tag` | `SvGameStorage` with the scope `SV_STORAGE_ENTITY` on a car, then `object_remove` | The tag reads back, and after the car goes the same reference finds nothing |
| 62 | `sound_tour` | `sound` for the ids 0 to 46 at pitch 1, one every 1.5 seconds, with a `TOURSOUND` line in the log | 47 sounds. See [the audio page](audio-analysis.md) |
| 63 | `pitch_octaves` | `sound` of id 27 at the pitches 0.125 to 4, twice | Six octaves. Two steps that sound the same show a limit of the client |

Tests 30 to 63 have not run against the original client yet.
Tests 58 to 60 measure what a music and picture plugin needs: the sound pitch, many moving items and a newspaper that changes while it is open. They use the tick event. Each one stops by itself and reports the number of steps or the first error. Test 60 leaves line 0 of the news changed until the next round.
Test 57 refuses the tester, bots and administrators. The kick puts the IP address of the player in the kick list of the server. The list lives in memory only. It blocks that address until the server restarts or an administrator types `/clearkick`. A friend on your own network shares your address, so you stay blocked too until you type `/clearkick`.
Each test that changes money, stocks, a company price, a mission, a shop catalog or the bot count puts the old value back in the next test or in `/test stop`.
Test 55 reads the three settings that the plugin declares in its `plugin.ini` (`greeting`, `count` and `scale`).

## First play test

The user ran tests 1 to 19 with the original client on 2026-10-27. Tests 4 to 19 worked, including a replaced and a new news line and the aim query.
Two things stay open: whether the chat line of test 1 reaches other players and the private line of test 2 stays private (one player cannot show this), and what look settings a, b and c change.
The first run found that a Magnum could not go in a hand that still held the bandage of the test before. The plugin now detaches its items before it removes them.

## Not covered yet

The damage decision, the chat and join decisions, the object and item velocity rows and the line query on a car need a later step.
The name of an item (N43) needs a table row that does not exist yet.
These methods have no test here:

- `SvGameAi.street_route`. A plugin cannot read the road graph yet (work item R3), so it cannot build a valid route.
- `SvPlugins.provide` and `find` with a real provider. They need a second plugin. The host test covers them.
- `SvGameProps.add` and the vector rows that no test above uses.
