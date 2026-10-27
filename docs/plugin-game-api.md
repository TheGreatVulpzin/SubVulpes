# Game read API

Status: first read slice implemented. The build, fixture and real server checks passed within the limits below.
This first game API adds reads, and a small [write capability](#write-capability-version-1) follows. It does not complete the [mapped API](plugin-api-map.md) or close S3.
Most mutation and gameplay hook capabilities remain future work.
The user ordered framework tests, then the full mapped API, then production plugins.

## Capability lookup and compatibility

Include `sv_game_read.h`. It includes `sv_game.h` and the common native ABI.
The read table uses capability `SV_API_GAME_READ` (`1`) and version `SV_GAME_READ_VERSION` (`1`).
Its table type is `SvGameRead`, with explicit `size` and `version` fields.

`SvHost.get_api` is an optional append after the original 24-byte Windows x86 prefix.
`SV_HOST_V1_SIZE` identifies that prefix. The base plugin ABI remains `1`.
Before accessing `get_api`, check that `host->size` covers the field and that the function pointer exists.
Earlier hosts can supply only the original prefix.

For the current header, a lookup inside a valid callback can use:

```c
const void *table = NULL;
const SvGameRead *read;
if (host->size < sizeof(SvHost) || !host->get_api)
    return SV_INCOMPATIBLE;
if (host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) != SV_OK)
    return SV_INCOMPATIBLE;
read = (const SvGameRead *)table;
if (read->size < sizeof(*read) || read->version != SV_GAME_READ_VERSION)
    return SV_INCOMPATIBLE;
```

Lookup needs the current callback's owner on the server thread, with no stop transition in progress.
An invalid owner, a worker-thread call or a call outside that context returns `SV_STATE`.
A null output-pointer argument returns `SV_INVALID` after the context check.
For a valid context and output pointer, lookup clears `*api` before checking the capability and version.
An unknown capability returns `SV_NOT_FOUND`. An unsupported version returns `SV_INCOMPATIBLE`, with `*api` still null.
A context failure leaves the output pointer unchanged.

Successful lookup does not mean the world is ready.
Read methods additionally require world readiness. They return `SV_STATE` before readiness and during stop callbacks.
Use the table only inside permitted callbacks, even if the plugin retains its address.

## Copies and references

Every snapshot copies data into caller-owned storage. No record contains an internal game pointer.
Set each output record's `size` before the call. The buffer must cover the current record size.
A larger buffer is permitted, but the method writes only the current record size.
Snapshot errors leave the caller's buffer unchanged.
Context failure returns `SV_STATE`, unresolved or wrong-kind references return `SV_NOT_FOUND`, and invalid buffers or indices return `SV_INVALID`.
Malformed internal counts return `SV_FAILED` where the method checks them.

`SvEntity` is an opaque 64-bit reference. Entity kinds are player, human, item, object and account.
Zero denotes no reference. Relations can contain zero when the related record is absent or has no valid reference.
Do not treat a reference as a pool index, persistent account identifier or connection identity.
Retirement, slot reuse and world reset invalidate the affected references.
Resolve each reference again through the relevant method. A previous successful read does not guarantee later validity.

## Entity pages

`entities(owner, kind, cursor, entities, capacity, count, next)` scans physical pool slots.
Start at cursor zero and continue with the returned `next` cursor.
The capacity must be between 1 and `SV_ENTITY_PAGE_MAX` (`256`). Supply all three output pointers.
The cursor must not exceed the selected pool's capacity. An unknown kind returns `SV_INVALID`.

Each successful call scans at most 256 physical slots and returns at most `capacity` references.
`next` identifies the next physical slot to examine.
Empty or short pages can precede later active entities. Continue until `next` equals the selected pool capacity.
A call at that final cursor succeeds with count zero and the same cursor.
Errors leave the entity array, count and next outputs unchanged.
Separate pages do not form an atomic world snapshot.

| Entity kind | Pool capacity |
| --- | --- |
| Player | 256 |
| Human | 512 |
| Item | 1,024 |
| Object | 1,024 |
| Account | 65,536 |

## Snapshot methods and bounds

| Method | Copied state |
| --- | --- |
| `player` | Name, account/human references, money, stocks, team, flags, input, look selectors and appearance vectors |
| `human` | Player/account/vehicle references, seat, health, bleeding, timers, position, velocity, input, stance and appearance |
| `item` | Parent relations, type, ammo, timers, mass, motion, child references, phone state and bills mask |
| `object` | Driver reference, type, health, color value, timers, motion, bounds, wheel counts, seat count and engine state |
| `account` | Name, money, `playtime_raw` and three look selectors |
| `bone` | One human bone's motion, rotation, mass and radius |
| `inventory` | One human slot's count and item references |
| `world` | Tick/reset sequence, mode/round/state, timers, raw clock, server name, limits, spawn position/sizes, type counts and the number of news lines |
| `headline(index, text, capacity)` | Copies one news line. The index is below `headline_count` of the world snapshot. The buffer needs `SV_HEADLINE_BUFFER` (125) bytes. `SV_NOT_FOUND` for a line that does not exist, `SV_INVALID` for a null or small buffer |

Bone indices are `0..14`: 15 bones. Inventory slot indices are `0..7`: eight slots.
Each inventory slot and item child list holds at most eight references.
Invalid inventory or child counts return `SV_FAILED` without modifying the snapshot buffer.
Object reads reject `wheel_count` outside `0..32`, `wheel_body_count` outside `0..16` and `seat_count` outside `0..7`.
`wheel_count` excludes the separately reported `wheel_body_count`.

## Line queries

Two methods follow the table of snapshot methods. They answer one question: what does a line hit first? They do not change the game.

| Method | Effect | Result codes besides `SV_STATE` |
| --- | --- | --- |
| `line(from, to, flags, ignore, hit)` | Finds the nearest hit on the line. `flags` picks the kinds: `SV_LINE_LEVEL` (ground, blocks and city objects), `SV_LINE_OBJECTS` (the faces and wheels of vehicles) and `SV_LINE_HUMANS` (a sphere around each of the 15 bones). `ignore` is a human to skip, or 0. | `SV_INVALID` for a null or short `hit`, no flag or an unknown flag, a point that is not finite or is beyond 32768 in an axis, a line of zero length or more than 4096 units. `SV_NOT_FOUND` for an `ignore` that is not a human |
| `aim(human, distance, flags, hit)` | The same query from the middle of the head of the human, along the direction that its head faces (the negative third row of the rotation of bone 2), for `distance` units. The human is never a hit. | `SV_INVALID` for a distance that is not above 0 or not finite, or above 4096, and the same `hit` and `flags` rules. `SV_NOT_FOUND` for a wrong reference |

A line that hits nothing is a normal answer. The result is `SV_OK`, the kind is `SV_HIT_NONE`, the position and normal are 0 and the fraction is 1.
`SvLineHit` holds the kind, the entity (the vehicle or the human, 0 for the level), the bone of a human (or -1), the hit point, the normal of the surface,
the fraction of the line before the hit and the length of that part. The nearest hit wins across all the kinds in `flags`.
A wheel counts as a hit on its vehicle. The game tests share one scratch record for their answers. The query saves it before the tests and puts it back after them,
so a plugin callback does not change what the game wrote there.
The cost is one pass over the active vehicles and the active humans. Call it a few times for each tick, not once for each body.
The head direction is the one that the legacy tool used. It is read from the body and it is not a client value.

## Association and raw-field meanings

`SvHumanState.account` comes from the body record. It can differ from the associated player's account.
`SvHumanState.seat` is `-1` when the snapshot has no valid vehicle reference.
`SvItemState.parent_slot` is `-1` when the snapshot has no valid human parent reference.

`SvObjectState.driver` identifies a player whose active, living human occupies vehicle seat zero.
The player's human reference must also point to that body.
The API derives this relation from current reciprocal records, rather than the raw last-driver index.

The API omits `SvItemState.vehicle`, `SvItemState.linked_item` and `SvObjectState.linked_object`.
Those historical raw indices can target a different entity after slot reuse.
Future support needs generation-safe associations, not a conversion of the current raw index into a fresh reference.

`SvHumanState.despawn_counter` reports raw engine state. It is not a remaining-duration promise.
`SvWorldState.spawn_sizes` copies the three raw spawn sizes. They do not define symmetric extents around the spawn position.
For item and object `time_to_live`, `65535` is the unlimited sentinel.
Item lifetime can decrease in both game logic and time-to-live (TTL) processing. One decrement per simulation tick is not guaranteed.

Player and account names use 32-byte arrays. They contain at most 31 copied bytes plus a null terminator.
The server name uses 72 bytes, with at most 71 copied bytes plus a terminator.
The API does not copy passwords or persistent account identifiers.
An account reference identifies only a live API record.

`playtime_raw`, `world_clock_raw`, appearance vectors, selectors and color values retain their current engine representation.
This contract does not assign invented time units or RGB meaning to raw values.
The world type counts currently report 27 item types and 14 object types.
Those counts do not establish arbitrary new client type support.

## Validation and remaining limits

The fixture command is:

```powershell
ctest --test-dir build/server -R game-api --output-on-failure
```

The implementation is committed as `92854b1`.
The normal build exited with code 0. All six CTest groups passed in 34.71 seconds.
A focused `game-api` rerun after the build completed passed in 0.79 seconds.
Its local evidence is `build/server/Testing/Temporary/LastTest.log`.
The fixture reported `PUBLIC_READ_API_PASS` and `STOP_READ_DENIED`, and exercised real constructors and entity retirement paths.

The native dependency check passed with 74 sources.
Storage validation passed for 138 independent objects and 499799832 declared bytes.
The repository privacy check found zero problems in 262 files.
These checks describe this read slice, not later API work.

### Real server runs

Two real server cycles used the previous public framework package for data and its original status DLL.
The test setup used the current server executable and added the `test.read` fixture.
This was an isolated runtime check, not a new release package.
Both cycles reported `PUBLIC_READ_RUNTIME_PASS` and `STOP_READ_DENIED`, exercised live disable/enable and exited cleanly with code 0.

| Cycle | PID | Duration | Tick sample |
| --- | --- | --- | --- |
| First | 16928 | 64.53 seconds | 62.3 ticks/s, one sample |
| Second | 16076 | 3.54 seconds | No periodic sample |

The single tick sample is not a performance comparison or proof of sustained loaded-server behavior.
The result record is `build/game-read-review/94cd5dbd49d647ee816e5121ed4afbd3/result.json`.
The same directory contains `cycle-1.log` and `cycle-2.log`.
Its base package is `SubVulpes-server-v0.0.0-s3-framework.1.zip`.
The result records these exact SHA-256 identities:

| Component | SHA-256 |
| --- | --- |
| Current server | `6191ea5849e8662dd46c1721c8176b94cdef7075459ce77777670e5862ac77f8` |
| Read probe | `b6ac3f5927fbc608ca168850feee3094008cadd09fb944c0bc229e1b261d5a74` |
| Previous package's status DLL | `e5f2171160988276c6fa7994bd3be1f116dfbcc0c0972e110dc5932370242b11` |

The older status DLL remained usable with its original ABI prefix during these runs.
This does not establish compatibility for every third-party module.

### Remaining limits

The read slice introduces no public game mutation operations or original-client behavior changes.
The write capability below is a separate slice with its own limits.
Original-client gameplay and manual launcher acceptance remain pending.
The full mapped API and S3 remain open. Production gameplay and administration plugins follow the complete mapped API proof.
These results do not establish client gameplay parity, broader hook coverage, Lua or WASM support.
See [native SDK](plugin-sdk.md) and [S3 evidence](s3.md) for the framework contract and earlier package evidence.

## Write capability (version 1)

Include `sv_game_write.h`. The table type is `SvGameWrite`, with capability `SV_API_GAME_WRITE` (`2`) and version `SV_GAME_WRITE_VERSION` (`1`).
Look it up with `get_api` as for the read table. An unsupported version returns `SV_INCOMPATIBLE` with a null output.

A write call has the same context rule as a read. It needs a host callback on the server thread, a ready world and no stop transition.
Otherwise it returns `SV_STATE`. The host starts callbacks between ticks, so a write never runs inside the simulation.
Each call checks its input first. An error leaves the game unchanged. Entity arguments are the references from the read API.

| Method | Effect | Result codes besides `SV_STATE` |
| --- | --- | --- |
| `message(kind, text)` | Sends a message event to every client. Kind `0` is a server message for every client. Kind `4` reaches only admins, like the replies of the chat commands. The text has 1 to 63 printable ASCII bytes. | `SV_INVALID` for another kind, a null or empty text, a long text or a control byte |
| `kick(player)` | Adds the address of the connection to the kick list and sets its idle counter to the drop value, as `/kick` does. | `SV_NOT_FOUND` for a wrong kind or a player with no connection, `SV_LIMIT` when the kick list is full |
| `add_money(account, amount)` | Adds the amount to the account and to each active player of it, as `/give` does. | `SV_NOT_FOUND`, `SV_INVALID` when any sum leaves the 32-bit range (no balance changes) |
| `set_looks(player, looks)` | Sets the five look selectors (highest values 2, 7, 3, 1 and 1, as the game makes them) of the player and of its human, and sends the player and human updates. | `SV_NOT_FOUND`, `SV_INVALID` for a null pointer or one value out of range (nothing changes) |
| `message_to(kind, player, text)` | Like `message`, but only the connection of that player gets the text. The other clients get an empty slot in the event list. | As `message`, and `SV_NOT_FOUND` for a wrong kind of reference |
| `item_create(type, position, item)` | Makes an item of type 0 to 26 at rest at the position, and sends the item update. The reference goes to `*item` only on success. Type 26 (newspaper) is refused, because the news system counts its newspapers. | `SV_INVALID` for a bad type, a null pointer or a bad position, `SV_LIMIT` when all 1,024 item slots are in use |
| `item_remove(item)` | Sets the lifetime to zero. The game removes the item in its next item lifetime pass. The item leaves its human and its parent, its children leave it, and a key takes its vehicle with it. | `SV_NOT_FOUND` |
| `item_equip(item, human, slot)` | Puts the item in slot 0 to 6 of a human, with the slot rules of the game: one item in a hand, only guns in slot 2, no guns in slots 3 to 6. | `SV_NOT_FOUND`, `SV_INVALID` for a slot of 7 or more, `SV_CONFLICT` when the rules refuse it |
| `item_attach(parent, child)` | Puts the child in the parent, such as a magazine in a gun. | `SV_NOT_FOUND`, `SV_INVALID` for the same item twice, `SV_CONFLICT` when the child already has a parent item, when the types do not fit or when the parent is full |
| `item_detach(item)` | Takes the item out of its human or its parent item. | `SV_NOT_FOUND`, `SV_STATE` when the item has neither |
| `object_create(type, position, yaw, color, object)` | Makes an object of type 0 to 11 at the position, turned by the yaw angle in radians, with color 0 to 5, and sends the creation event. Types 12 and 13 are the pieces of the train, which only the track system makes. The reference goes to `*object` only on success. | `SV_INVALID` for a bad type, color, position or angle, or a null pointer. `SV_LIMIT` when all 1,024 object slots are in use |
| `object_remove(object)` | Sets the lifetime to zero. The game removes the object in its next object lifetime pass, and the humans inside get out. | `SV_NOT_FOUND`, `SV_DENIED` for a piece of the train |
| `npc_create(position, yaw, player, human)` | Makes a bot player named NPC in team 6, like the bots of the game, with a human on foot at the position and turned by the yaw angle. The bot has no route and no end time. Both references go out only on success. The position of the human reads the place at once. | `SV_INVALID` for a bad position or angle or a null pointer. `SV_LIMIT` when no player slot, human slot or bond is free, with nothing left behind |
| `npc_remove(player)` | Gives the bot an end time in the past. The bot expiry of the game removes the human and the player in the next world logic pass. | `SV_NOT_FOUND`, `SV_DENIED` for a player who is not a bot |
| `vehicle_exit(human, stop)` | Takes the human out of its vehicle the way the exit button does: the view angles become free, the vehicle gets the idle drive state and a bone that the exit would put in a wall moves to the near side of it. With `stop` 1 the speed of each bone becomes 0, so the body does not fly at the speed of the vehicle. | `SV_NOT_FOUND`, `SV_INVALID` for a `stop` other than 0 or 1, `SV_STATE` for a human who sits in no vehicle |
| `sound(index, position, volume, pitch)` | Plays a sound of the game at the position for the clients. The index must be below the size of the sound list (47 in this server, so 0 to 46). The volume is 0 to 2 and the pitch 0.1 to 4. These two limits are a choice of this API. The game uses volumes 0.5 to 1 and pitches 0.5 to 1. | `SV_INVALID` for an index, a volume, a pitch or a position out of range, or a value that is not finite |
| `headline(index, text)` | Replaces news line `index`, or adds a line when the index equals `headline_count`, and sends it to the clients. The text has 0 to 124 printable ASCII characters. The game splits it at character 63 into the two parts of the line. See [News lines](#news-lines). | `SV_INVALID` for a null text, a text that is too long or has a control or non-ASCII byte, or an index that leaves a gap. `SV_LIMIT` when 1,024 lines exist |
| `teleport(human, position)` | Moves the 15 bones by the same offset so bone 0 reaches the position. It copies the new place to the previous place of each bone. The velocity does not change. | `SV_NOT_FOUND`, `SV_INVALID` for a value that is not finite or beyond 32768 in an axis, `SV_STATE` for a human in a vehicle |

### News lines

The newspaper of the game has up to 1,024 lines of 256 bytes. The server loads them from `readme.txt` at each round, adds "Top 10 list" and the ten richest accounts, and sends each line to the clients as two events of kind 3. The first event holds the characters 0 to 62 and the second holds the rest, from character 63. A line can hold 124 characters, which is the length that the loader reads.
The events of kind 3 are the only text events that a client gets whole when it joins late. The game turns other old text events into placeholders after 600 ticks. The event list restarts at each round, so a client that joins later replays the lines, including the ones that a plugin changed.
A change lasts for the current round. The next round loads `readme.txt` again, so a plugin that wants a line in every round writes it again at each round start.
A line that the plugin replaces sends two new events with the same index. The original client most likely overwrites the old text with them, but this was not run. A new line at the end depends on the client count of lines, which was not checked either. Both go on the list of things to test with the original client.
The server reads the lines only to send them. A change affects only what the clients show.
The test starts with three lines. It checks each error code, a refused call that changes nothing, a replaced line, a line of 100 characters that splits into 63 and 37, an empty line, the fill of all 1,024 lines and the refusal of the next one. It also reads the events and the storage, and it checks that a client that is 1,000 ticks late gets a headline whole and a message as a placeholder.
Removing one check at a time made `game-api` fail each time (`build/logs/mutation22.log`): the gap in the index, the limit of 1,024 lines, the text length, the line count on read, the buffer capacity on read, the null buffer and the null text, the printable-text check, and the context checks of both methods.

The tests call every method from a real plugin callback against records that real constructors made.
They check each error code and a wrong-thread call. The test program then reads the game records.
It checks the two message events, the balances, the flag, the health, the bone offsets, the kick list and the idle counter.
Removing one check at a time (message bytes, money range, flag range, health range, finite position, position limit, vehicle) made `game-api` fail each time.
For the item methods, removing the slot bound, the type bound, the check for a second parent item or the newspaper refusal made `game-api` fail each time (`build/logs/mutation10.log`).
For the object methods, removing the type bound, the color bound, the refusal to remove a train piece and the angle check made `game-api` fail each time (`build/logs/mutation11.log`).
Keys keep the index of their vehicle after the vehicle goes. This is the same as when the game removes a vehicle by itself.
The exit method has a test of every error code. The test program seats two humans in one car. One leaves with `stop` 1 and one with `stop` 0, and the program checks the idle drive state, the bone speeds and a bone under the ground that the exit lifts to the ground.
The line queries have a test of every error code, of a hit on a vehicle face, on the ground and on a human, of the ignore rule, of the nearest of three kinds and of the aim. The ground is flat at height 0 in this test, so it does not prove the shapes of the real level. A second test sets a mark in the shared record and checks that a query gives it back.
The flags, health, stun, protection, team and admin numbers moved to the property table. Their tests moved with them. The earlier removal proofs of the range checks of the old methods (`mutation4.log`) were for code that no longer exists, and the table has its own proofs above.
The upper bound of 3,600 ticks (one minute) is a choice of this API. The game itself sets the timer to 3 or 6 times the damage of a hit.
For the bots, removing the cleanup after a failed human and the bot check made `game-api` fail each time (`build/logs/mutation12.log`). A local command of the fixture leaves too few bonds for a human and checks that no player stays behind.
`createhuman` of the game does not set the position of a human until its first tick. `npc_create` sets it at once, so a read right after the call shows the place.
The game keeps no check of a loop of parent items, and the types of the tests cannot make one. That check has no test.
For the checks that guard rights or privacy, the same removal made `game-api` fail too: the private-message filter, the stale-target test, the bot check and the connection admin flag of `set_admin`.
The packet writer clamps the sent health to the range -10 to 117 and leaves the server value as written. So a client sees the same value for -100 and -10, and for 100 and 117. See [network limits](game/network.md#representation-limits).

Not covered: a full kick list (41,368 addresses), more than one connection for a player, a message when the event ring is full, and a client check of a teleport or a health value.
The original client was not run. A teleport reaches the client only through the normal human state updates.

## Property table (capability 10)

Include `sv_game_props.h`. The table type is `SvGameProps`, with capability `SV_API_GAME_PROPS` (`10`) and version `SV_GAME_PROPS_VERSION` (`1`).
It replaces one method for each number of the game. A property is a number with a name, an entity kind, a type, a count of slots and a range.
The five methods are `find` (name to id), `info` (describe an id), `get`, `set` and `add`.
A plugin calls `find` once and keeps the id. Ids start at 1 with no gap, so `info` with a rising id lists every property.
The context rule is the one of the write table. An error leaves the game unchanged.
The checks run in this order, and a row may add its own after them:

| Code | When |
| --- | --- |
| `SV_STATE` | No host callback, a wrong thread, a world that is not ready, or a stop in progress |
| `SV_NOT_FOUND` | The id is unknown, or the entity is not of the kind of the property |
| `SV_INVALID` | A world property with an entity (the entity must be 0), a slot out of range, a null pointer, or a value out of range or not finite |
| `SV_DENIED` | The property does not offer the operation (`set` on a velocity, `add` on a flag) |

Rows of the first version. The range is the range that a `set` or an `add` may leave in the field:

| Name | Entity | Type and slots | Operations | Range | Notes |
| --- | --- | --- | --- | --- | --- |
| `player.godmode` | player | int | get, set | 0 to 1 | |
| `player.admin` | player | int | get, set | 0 to 1 | A bot is refused (`SV_INVALID`). Each connection of the player follows the flag, as the password login does |
| `player.team` | player | int | get, set | 0 to 7 | The human of the player follows. The clients get the player update |
| `human.health` | human | int | get, set, add | -100 to 100 | -100 is the dead value of the game |
| `human.bleeding` | human | int | get, set | 0 to 1 | A bleeding body loses health over time |
| `human.stun` | human | int | get, set, add | 0 to 3600 | Ticks. The torso is weak while it runs, so the body staggers. The game counts it down by 1 each tick. The upper bound is a choice of this API |
| `human.protect` | human | int | get, set, add | 0 to 21600 | The spawn protection counter in ticks. 21600 is the longest value that the game gives (6 times 3600) |
| `human.bone_velocity` | human | vector, 15 slots | get, add | change of at most 64 | The slot is the bone. An add changes the velocity of that bone |
| `object.velocity` | object | vector | get, add | change of at most 64 | An add changes every particle of the object. A piece of the train gives `SV_DENIED` |
| `human.damage_taken_scale` | human | float | get, set, add | 0 to 100 | Multiplies each damage that the human takes. See "Damage scales" |
| `human.damage_dealt_scale` | human | float | get, set, add | 0 to 100 | Multiplies each bullet damage that the human deals to another human |
| `object.damage_taken_scale` | object | float | get, set, add | 0 to 100 | Multiplies each damage to the health of the object, and the damage to its wheels and windows |

### Damage scales

A scale is a decimal number, 1 by default. The core multiplies each damage by the scales that apply, rounds to the nearest whole number (a half goes up), and caps the result at 100000. A scale of 0 cancels the damage. An amount of 0 or less is not changed. At the value 1 the number stays as the game made it.
Where it applies:

- A bullet on a human: the taken scale of the victim times the dealt scale of the human of the shooter. The stun, the bleeding chance, the team kill tax and the killer of the death events follow the scaled number, so a cancelled hit gives none of them.
- A hit by a vehicle on a human (the 120 points of damage): the taken scale of the human.
- The health of an object, from a bullet, a crash or any other cause (`applydamageobject`): the taken scale of the object. A bullet on a wheel and on a window uses the same scale.

The scales live in a table beside the game records, because the record layouts are fixed. A new body and a new object start at 1.
The plugin sets a scale at a safe point, so a rule that depends on a position can lag by one tick. The safe spawn area is one example: set the scale of a human to 0 while it stands in the area. A rule that must see each hit needs a decision inside the simulation (see [plugin events](plugin-events.md)).

A plugin area adds its own group of rows with `sv_props_add_group` (see `plugin_props.h`) from its install function.
The table refuses a row with no name, a name that is too long or repeated, no field function, no operation, a vector with `set`, or a vector with `add` and no add function.
The kind `SV_ENTITY_WORLD` (0) is for a number of the world that belongs to no record. The entity argument is then 0.
A world property for each type of a list (for example each object type) uses the slot for the type.

Tests: the fixture calls every row and every code, and the test program checks the records and the events after the tick. A group added by the test program shows the world kind, and the refusal of each bad row.
Removing one check at a time made `game-api` fail each time (`build/logs/mutation14.log`, `mutation14b.log`, `mutation15.log`, `mutation15b.log`): the slot bound, the range of a set, the range of an add, the length and the finite check of a push, the bot check of the admin flag, the refusal of a train piece, the entity kind, the name length, the field function, the vector rules, the repeated name and the world entity rule.
The damage scales: removing the cap of the result, the index checks of the human, the shooter and the object tables, and the upper range of a float made `game-api` fail each time (`build/logs/mutation18.log`).
Not proven by removal: the bounds on the particle list of an object (the test world has no object with a bad list), and the choice of the order of the two scales.

## Chat commands

A plugin registers a command with `host->command(owner, name, permission, callback)`. A chat text that starts with `/` goes to the plugin commands before the inherited ones.
The callback gets an actor, a nonzero reference of the player who typed the text, and the text after the name.
Actor zero is the local operator on the server console. The chat never sends it.

| Permission | Who can run it from the chat |
| --- | --- |
| `SV_COMMAND_PUBLIC` | Every player |
| `SV_COMMAND_ADMIN` | A player with the `is_admin` flag, which `/admin PASSWORD` sets |
| `SV_COMMAND_LOCAL` | Nobody. Only the server console runs it |

The host refuses a denied command and logs `Plugin command permission denied`. The host reserves the names `plugins` and `admin`.
The tests run the chat function against a real player record. They check the three levels, an unknown command, and a player with no reference.
Removing the routing, the admin flag check, the zero-actor guard or the `admin` reservation made `game-api` fail each time.
A plugin answers one player with `message_to`. The server filters that event for each connection, so the original client gets a normal message. The [legacy module list](legacy-modules.md) names this as N1.
