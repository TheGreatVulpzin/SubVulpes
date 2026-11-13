# Game read API

The game API gives a plugin copied snapshots of game records, a small set of checked writes and a table of named properties.
See [the write capability](#write-capability-version-1) for the writes.

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
if (host->size < offsetof(SvHost, get_api) + sizeof host->get_api || !host->get_api)
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
| `connection` | Address or masked bytes, host-order port, idle ticks, spectate, flags and profile identifiers |
| `bone` | One human bone's motion, rotation, mass and radius |
| `inventory` | One human slot's count and item references |
| `world` | Tick/reset sequence, mode/round/state, timers, raw clock, server name, limits, spawn position/sizes, type counts and the number of news lines |
| `within(kind, center, radius, entities, distances, capacity, count)` | Finds the humans, items or vehicles whose position is within the radius of the center. The references go in the order of the game slots, with the distances in a second array (optional). The radius is above 0 and at most 4096. The capacity is 1 to 256. `SV_LIMIT` when more records matched than fit (the first ones are given). `SV_INVALID` for another kind, a null array or count, a bad capacity or radius, or a center that is not finite or is beyond 32768. One pass over the slots, so it costs the same as a read of every record |
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
The head direction comes from the body. It is not a client value.

## Association and raw-field meanings

`SvHumanState.account` comes from the body record. It can differ from the associated player's account.
`SvHumanState.seat` is `-1` when the snapshot has no valid vehicle reference.
`SvItemState.parent_slot` is `-1` when the snapshot has no valid human parent reference.

`SvObjectState.driver` identifies a player whose active, living human occupies vehicle seat zero.
The player's human reference must also point to that body.
The API derives this relation from current reciprocal records, rather than the raw last-driver index.

`SvObjectState.occupants[8]` names the human in each seat of the vehicle, and holds 0 for an empty seat. The index is the seat of the game, and seat 0 is the driver.
Only the first `seat_count` slots can hold a human. The host scans the active humans whose vehicle is the object, so a bot counts, and so does a dead body that still sits in a seat.
Read the health of the human to tell them apart. Two humans never share a slot: the first one found keeps it.
A vehicle with no seats (`seat_count` is 0) has no occupants.

The API omits `SvItemState.vehicle`, `SvItemState.linked_item` and `SvObjectState.linked_object`.
Those historical raw indices can target a different entity after slot reuse.
A raw index cannot become a safe reference, so the API does not expose it.

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
| `item_create(type, position, item)` | Makes an item of type 0 to 26 at rest at the position, and sends the item update. The reference goes to `*item` only on success. Type 26 (newspaper) is refused, because the news system counts its newspapers. | `SV_INVALID` for a bad type, a null pointer or a bad position, `SV_LIMIT` when all 1,024 item slots are in use or the table of made things is full |
| `item_remove(item)` | Sets the lifetime to zero. The game removes the item in its next item lifetime pass. The item leaves its human and its parent, its children leave it, and a key takes its vehicle with it. | `SV_NOT_FOUND` |
| `item_equip(item, human, slot)` | Puts the item in slot 0 to 6 of a human, with the slot rules of the game: one item in a hand, only guns in slot 2, no guns in slots 3 to 6. | `SV_NOT_FOUND`, `SV_INVALID` for a slot of 7 or more, `SV_CONFLICT` when the rules refuse it |
| `item_attach(parent, child)` | Puts the child in the parent, such as a magazine in a gun. | `SV_NOT_FOUND`, `SV_INVALID` for the same item twice, `SV_CONFLICT` when the child already has a parent item, when the types do not fit or when the parent is full |
| `item_detach(item)` | Takes the item out of its human or its parent item. | `SV_NOT_FOUND`, `SV_STATE` when the item has neither |
| `object_create(type, position, yaw, color, object)` | Makes an object of type 0 to 11 at the position, turned by the yaw angle in radians, with color 0 to 5, and sends the creation event. Types 12 and 13 are the pieces of the train, which only the track system makes. The reference goes to `*object` only on success. | `SV_INVALID` for a bad type, color, position or angle, or a null pointer. `SV_LIMIT` when all 1,024 object slots are in use or the table of made things is full |
| `object_remove(object)` | Sets the lifetime to zero. The game removes the object in its next object lifetime pass, and the humans inside get out. | `SV_NOT_FOUND`, `SV_DENIED` for a piece of the train |
| `npc_create(position, yaw, name, player, human)` | Makes a bot player in team 6, named `name` (0 to 31 printable ASCII bytes, or NULL for NPC; the first update event already has the name), like the bots of the game, with a human on foot at the position and turned by the yaw angle. The bot has no route and no end time. Both references go out only on success. The position of the human reads the place at once. | `SV_INVALID` for a bad position, angle or name, or a null pointer. `SV_LIMIT` when no player slot, human slot or bond is free or the table of made things is full, with nothing left behind |
| `set_name(player, name)` | Gives a player, a bot included, a new name of 0 to 31 printable ASCII bytes and sends the update event. The name above the head of a body is the name of the player that owns it. The name of the account stays | `SV_NOT_FOUND`, `SV_INVALID` for a null pointer, a long name or a byte that is not printable ASCII |
| `speak(human, text, range)` | Makes a human say a text of 1 to 63 printable ASCII bytes. It is the message event of kind 1 that the chat of a player makes, so the clients near the human see it above the head. The range is 0 short, 1 normal or 2 long | `SV_NOT_FOUND`, `SV_INVALID` for a bad range or text, `SV_STATE` for a human that is dead (the game sends the chat of a dead player as a plain message) |
| `npc_remove(player)` | Gives the bot an end time in the past. The bot expiry of the game removes the human and the player in the next world logic pass. | `SV_NOT_FOUND`, `SV_DENIED` for a player who is not a bot |
| `vehicle_exit(human, stop)` | Takes the human out of its vehicle the way the exit button does: the view angles become free, the vehicle gets the idle drive state and a bone that the exit would put in a wall moves to the near side of it. With `stop` 1 the speed of each bone becomes 0, so the body does not fly at the speed of the vehicle. | `SV_NOT_FOUND`, `SV_INVALID` for a `stop` other than 0 or 1, `SV_STATE` for a human who sits in no vehicle |
| `sound(index, position, volume, pitch)` | Plays a sound of the game at the position for the clients. The index must be below the size of the sound list (47 in this server, so 0 to 46). The volume is 0 to 2 and the pitch 0.1 to 4. These two limits are a choice of this API. The game uses volumes 0.5 to 1 and pitches 0.5 to 1. | `SV_INVALID` for an index, a volume, a pitch or a position out of range, or a value that is not finite |
| `headline(index, text)` | Replaces news line `index`, or adds a line when the index equals `headline_count`, and sends it to the clients. The text has 0 to 124 printable ASCII characters. The game splits it at character 63 into the two parts of the line. See [News lines](#news-lines). | `SV_INVALID` for a null text, a text that is too long or has a control or non-ASCII byte, or an index that leaves a gap. `SV_LIMIT` when 1,024 lines exist |
| `bullet(type, position, velocity, weapon)` | Tells the clients about a shot that the server does not simulate (the event of kind 0 that a real shot makes). Type 0 to 3 is one of the four bullet types. The velocity is in units for each tick, at most 64 long. The weapon is an item. The clients draw a tracer. No bullet exists in the server, so nothing is hit and no damage is made | `SV_NOT_FOUND` for a weapon that is not an item, `SV_INVALID` for a bad type, a position or velocity that is not finite, or a velocity above 64 |
| `bullet_hit(kind, position, normal)` | Tells the clients about the effect of a hit (the event of kind 1): kind 0 a block, 1 a human, 2 a vehicle, 3 a drip of blood. It makes no damage | `SV_INVALID` for a kind of 4 or more, a position that is not finite or a normal with a part above 4 |
| `teleport(entity, position)` | Moves a human, a vehicle or a free item so that its place reaches the position, and gives every part the same offset. A human: the 15 bones, so bone 0 reaches the position. A vehicle: the center of the object (`position` of the object snapshot), with every particle, wheel, wheel body, rotor point and the box. An item: its position. The previous places move too, so the move adds no speed, and the speeds do not change. | `SV_NOT_FOUND` for any other kind, `SV_INVALID` for a value that is not finite or beyond 32768 in an axis, `SV_STATE` for a human in a vehicle or an item that a human or another item holds, `SV_DENIED` for a piece of the train, `SV_FAILED` for a vehicle with a list out of range |
| `vehicle_enter(human, vehicle, seat)` | Seats a human on foot in a free seat of a vehicle. Seat 0 is the driver. The game does the same when the use button finds a free seat within 1.5 units. The human then sits where the seat is, and the next tick places it. | `SV_NOT_FOUND`, `SV_DENIED` for a piece of the train, `SV_INVALID` for a seat below 0 or from the seat count of the vehicle up, `SV_STATE` for a human that is dead or already sits in a vehicle, `SV_CONFLICT` when another human sits in that seat, `SV_FAILED` for a seat count out of range |
| `disown(entity)` | Gives up an item, a vehicle or a bot that this plugin made, so the host does not end it when the plugin stops. For a bot give the reference of the player. See [things that a plugin made](plugin-libraries.md#things-that-a-plugin-made). | `SV_NOT_FOUND` for a thing that this plugin did not make, or that is gone |
| `world_reset()` | Requests one round reset at the start of the next tick. Repeated requests merge. | None |

The host ends the items, vehicles and bots that a plugin made with `item_create`, `object_create` and `npc_create` when that plugin stops, unless the plugin called `disown`.

### News lines

The newspaper of the game has up to 1,024 lines of 256 bytes. The server loads them from `readme.txt` at each round, adds "Top 10 list" and the ten richest accounts, and sends each line to the clients as two events of kind 3. The first event holds the characters 0 to 62 and the second holds the rest, from character 63. A line can hold 124 characters, which is the length that the loader reads.
The events of kind 3 are the only text events that a client gets whole when it joins late. The game turns other old text events into placeholders after 600 ticks. The event list restarts at each round, so a client that joins later replays the lines, including the ones that a plugin changed.
A change lasts for the current round. The next round loads `readme.txt` again, so a plugin that wants a line in every round writes it again at each round start.
A line that the plugin replaces sends two new events with the same index. In the original client, on a running server, a replaced line changed in the open newspaper at once, and a new line at the end appeared as the last line. The client shows the server name as the header above the lines, so line 0 is the first line under the header. A graphics setting of that client drew the first lines of the newspaper black, which hid the text until the setting was changed.
The server reads the lines only to send them. A change affects only what the clients show.

Keys keep the index of their vehicle after the vehicle goes. This is the same as when the game removes a vehicle by itself.
The exit method has a test of every error code. One leaves with `stop` 1 and one with `stop` 0, and the program checks the idle drive state, the bone speeds and a bone under the ground that the exit lifts to the ground.
The line queries have a test of every error code, of a hit on a vehicle face, on the ground and on a human, of the ignore rule, of the nearest of three kinds and of the aim. The ground is flat at height 0 in this test, so it does not prove the shapes of the real level. A second test sets a mark in the shared record and checks that a query gives it back.
The flags, health, stun, protection, team and admin numbers moved to the property table. Their tests moved with them.
The upper bound of 3,600 ticks (one minute) is a choice of this API. The game itself sets the timer to 3 or 6 times the damage of a hit.
`createhuman` of the game does not set the position of a human until its first tick. `npc_create` sets it at once, so a read right after the call shows the place.
The game keeps no check of a loop of parent items, and the types of the tests cannot make one. That check has no test.
The packet writer clamps the sent health to the range -10 to 117 and leaves the server value as written. So a client sees the same value for -100 and -10, and for 100 and 117.

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

Rows. The range is the range that a `set` or an `add` may leave in the field:

| Name | Entity | Type and slots | Operations | Range | Notes |
| --- | --- | --- | --- | --- | --- |
| `player.godmode` | player | int | get, set | 0 to 1 | |
| `player.admin` | player | int | get, set | 0 to 1 | A bot is refused (`SV_INVALID`). Each connection of the player follows the flag, as the password login does |
| `player.team` | player | int | get, set | 0 to 7 | The human of the player follows. The clients get the player update |
| `human.health` | human | int | get, set, add | -100 to 1000 | -100 is the dead value of the game. The hospital heals up to 100 only, and damage and bleeding have no cap. The packet writer sends at most 117, so a client shows a full bar above that. Client effect unknown |
| `human.bleeding` | human | int | get, set | 0 to 1 | A bleeding body loses health over time |
| `human.stun` | human | int | get, set, add | 0 to 3600 | Ticks. The torso is weak while it runs, so the body staggers. The game counts it down by 1 each tick. The upper bound is a choice of this API |
| `human.protect` | human | int | get, set, add | 0 to 21600 | The spawn protection counter in ticks. 21600 is the longest value that the game gives (6 times 3600) |
| `human.bone_velocity` | human | vector, 15 slots | get, set, add | length of at most 64 | The slot is the bone. A set gives its exact velocity. An add changes its velocity |
| `object.velocity` | object | vector | get, add | change of at most 64 | An add changes every particle of the object. A piece of the train gives `SV_DENIED` |
| `human.damage_taken_scale` | human | float | get, set, add | 0 to 100 | Multiplies each damage that the human takes. See "Damage scales" |
| `human.damage_dealt_scale` | human | float | get, set, add | 0 to 100 | Multiplies each bullet damage that the human deals to another human |
| `object.damage_taken_scale` | object | float | get, set, add | 0 to 100 | Multiplies each damage to the health of the object, and the damage to its wheels and windows. A scale of 0 protects the object and its wheels (N29) |
| `item.ammo` | item | int | get, set, add | 0 to 1000 | The rounds in a magazine, or the uses of a bandage. The game does not fire at 0. The upper bound is a choice of this API |
| `item.time_to_live` | item | int | get, set | 0 to 65535 | Ticks. The game counts it down and removes the item at 0, as `item_remove` does. 65535 is the unlimited value |
| `item.velocity` | item | vector | get, set, add | length of at most 64 | A set gives an exact launch speed and wakes an item at rest. An add changes the speed. A held item or an item in a container gives `SV_STATE`, because it follows its parent. `teleport` keeps the speed, so a plugin that holds an item in place sets the speed to zero, or freezes the item |
| `item.angular_velocity` | item | vector | get, set, add | length of at most 64 | The spin in radians for each tick. The row also sets the angular momentum that the physics uses, with the inverse inertia of the item. An axis with no inertia does not spin. The same `SV_STATE` rule as the speed |
| `item.frozen` | item | int | get, set | 0 to 1 | The rest flag of the game. The physics skips an item at rest, so a frozen item stays in the air and gravity does not change it. A set to 1 also zeroes the speed and the spin. A set to 0 lets the item fall again. A `teleport` wakes the item, so set the row to 1 again after each move. An item that stops on the floor by itself reads 1. A held item, an item in a container and a door give `SV_STATE`. The client effect is unknown |
| `object.time_to_live` | object | int | get, set | 0 to 65535 | The same for a vehicle. A piece of the train gives `SV_DENIED`, because the track system ends its life |
| `object.health` | object | int | get, set, add | 0 to 1000000 | The health of a vehicle. The upper bound is a choice of this API. The game makes a vehicle with 100 |
| `item_type.price` | world | int, 27 slots | get, set, add | 0 to 1000000 | The slot is the item type. The price in the shop. Rows of the type tables restore (see "Type rows") |
| `item_type.mass` | world | float, 27 slots | get, set, add | 0.01 to 1000 | The mass of new items of the type. Items that exist keep their mass |
| `item_type.fire_delay` | world | int, 27 slots | get, set, add | 1 to 3600 | Ticks between two shots |
| `item_type.bullet_type` | world | int, 27 slots | get, set | 0 to 3 | Which of the four bullet types a gun fires |
| `item_type.bullet_speed` | world | float, 27 slots | get, set, add | 0 to 64 | Units for each tick. The damage of a bullet is its speed times its mass |
| `item_type.spread` | world | float, 27 slots | get, set, add | 0 to 1 | The random spread of a shot |
| `item_type.recoil` | world | float, 27 slots | get, set, add | 0 to 100 | The random kick that a shot gives the aim |
| `item_type.ammo` | world | int, 27 slots | get, set, add | 0 to 1000 | The rounds of a new magazine, or the uses of a new bandage |
| `bullet_type.mass` | world | float, 4 slots | get, set, add | 0 to 1 | The slot is the bullet type. The damage of a bullet to a human and to an object follows the mass |
| `bullet_type.drag` | world | float, 4 slots | get, set, add | 0 to 1 | How fast a bullet slows down |
| `world.state` | world | int | get, set | 1 to 3 | The state of the round: 1 waiting, 2 playing, 3 ended. A set skips the code that the game runs when it changes the state itself (the traffic of a new round, the doors of the bases). The round event follows one tick later. The owner of the plugin takes the risk |
| `world.timer` | world | int | get, set, add | 0 to 1000000 | Ticks left in the state. At 0 the game goes to the next state |
| `world.round_time` | world | int | get, set | 600 to 216000 | The ticks of the playing state of the next round (the option `roundtime` is in minutes times 3600) |
| `world.wait_time` | world | int | get, set | 60 to 216000 | The ticks of the waiting state of the next round |
| `world.end_time` | world | int | get, set | 60 to 216000 | The ticks of the end state, before the game resets the round |
| `train.track_target` | object | int | get | 0 and up | The track piece that a locomotive drives to (N49). The game uses two station pieces and switches between them. Read only. The value has meaning for a locomotive (type 12) only |
| `train.track_last` | object | int | get | 0 and up | The track piece under a locomotive (N49). The game copies it to the next car. Read only. The value has meaning for a locomotive (type 12) only |
| `train.stop_ticks` | object | int | get, set | 0 to 1200 | The ticks that a train has waited at a station (N49). The locomotive (type 12) leaves at 1200. A write of 0 holds the train longer, and a write of 1199 sends it on the next tick. It counts only while the train is at a station and slow. The bogies and every other object return `SV_DENIED` |
| `object.color` | object | int | get, set | 0 to 5 | The color value of a vehicle (N73). Client effect unknown: the creation event sends the color, the server sends no update event for it, and nobody has seen a change after the object exists. A piece of the train takes it too, because the color does not touch the life or the physics of the piece |
| `object.key_away` | object | int | get, set | 0 to 1 | 1 when the key of the vehicle is not with a human in it (N73). The car and helicopter code stop the engine then. The key logic rewrites it. A piece of the train returns `SV_DENIED` |
| `world.clock` | world | int | get, set | 0 to 3887999, multiples of 10 | The world clock (N72). A round starts at 1728000 and the clock grows by 10 each tick. The light of the day, the expiry of missions, the fall of account bans, the reset of purchase limits and the end of the round follow it. When the clock goes above 3887999 the game saves the accounts and resets the round. A write forward expires the missions that end before the new value. A value that is not a multiple of 10 returns `SV_INVALID`, because the periodic jobs run at multiples of 18000, 36000 and 54000. Client effect unknown: the clock goes to the client, and nobody has seen the new light |
| `account.ban_time` | account | int | get, set, add | 0 to 1000000 | Above 0 the game drops the player of the account at the join and during play. The count falls by 1 for each 36,000 ticks. The value is not saved on disk |
| `item.trigger_ticks` | item | int | get, set | 0 to 3600 | The ticks that the trigger of a held gun is down. The gun fires when it equals 2 (6 for the Magnum). A released trigger sets it to 0, so stop restores nothing. See [bot API additions](game/bot-controls.md). |
| `human.arm_pose` | human | int | get, set | 0 to 3 | The arms that follow `human.arm_target`: bit 1 the arm of bones 3 to 5, bit 2 the arm of bones 6 to 8. 0 gives both arms back to the game. The last plugin that sets it owns the pose, and its stop ends the pose. A new human in the slot or a world reset starts with no pose. See [arm pose](#arm-pose). |
| `human.arm_target` | human | vector, 2 slots | get, set | length up to 1.5 | The target of the hand of each arm (slot 0 bones 3 to 5, slot 1 bones 6 to 8), in the frame of the chest, from the shoulder. It moves the hand only while the bit of the arm is on in `human.arm_pose`. |
| `human.despawn` | human | int | get, set | 0 to 3600 | Require health at most -4. Below 2700, the next dead-body tick releases the player. Stop restores the first timer for the same human. See [bot API additions](game/bot-controls.md). |

### Arm pose

The arm code of the game (`human_calcarmtorque`) moves each hand toward a target in the frame of the chest, from the
shoulder: x to the side, y up, z to the front (negative is forward). It moves the hand at most 0.125 across and 0.0625
up or down in each tick, and the elbow bends so that the arm (two parts of 0.375) reaches the target. An empty hand
hangs; a gun points forward. With `human.arm_pose` a plugin replaces that target with `human.arm_target`, with or
without an item in the hand. The steps, the muscles and the limits stay the ones of the game, so a target farther than
0.75 gives a straight arm. A guard is about (0.15, 0.1, -0.3) for the arm of bones 3 to 5 and (-0.15, 0.1, -0.3) for
the other one; a punch is about (0.2, 0.2, -1.0).

A get of `human.arm_target` gives the last value that a plugin set for that human, or zero. A set needs a safe callback
on the server thread. The write changes no packet: the clients see the arm through the bones as for any move of the
game. The original client was not tested. Client effect unknown.

### Type rows

The rows `item_type.*` and `bullet_type.*` change the tables that the game fills when the server starts. A change reaches every item of the type that fires after it, so one row changes a weapon for all players. A new item takes the new price, mass and ammunition. The game does not set them again at each round.
The host keeps the first value of each slot that a plugin changes. When that plugin stops, or fails, the host puts the first value back. If two plugins change the same slot, the last one owns it, and the first value stays the one of the game.
The server decides damage and hits, but the original client may use the speed, the delay and the spread for its own prediction and its sounds, so a change to these rows can have an effect that the server does not control. The name of an item type is not a row, because no packet carries it.
A plugin area that adds rows can set the `restore` member of a row for a table of its own, with at most 32 slots and no vector.

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

A group added by the test program shows the world kind, and the refusal of each bad row.

## Chat commands

A plugin registers a command with `host->command(owner, name, permission, callback)`. A chat text that starts with `/` goes to the plugin commands before the inherited ones.
The callback gets an actor, a nonzero reference of the player who typed the text, and the text after the name.
Actor zero is the local operator on the server console. The chat never sends it.

| Permission | Who can run it from the chat |
| --- | --- |
| `SV_COMMAND_PUBLIC` | Every player |
| `SV_COMMAND_ADMIN` | A player with the `is_admin` flag, which `/admin PASSWORD` sets |
| `SV_COMMAND_LOCAL` | Nobody. Only the server console runs it |

The host refuses a denied command and logs `Plugin command denied`. The host reserves the names `plugins` and `admin`.
A plugin answers one player with `message_to`. The server filters that event for each connection, so the original client gets a normal message.

## Bone mass

`human.bone_mass` exposes 15 float slots with GET, SET and ADD. The range is 0.5 to 50000000, inclusive.
Writes change the live bone mass. Invalid slots, nonfinite values and results outside the range fail without a write.

## Account number

`SvGameRead.account_number` gives the number of an account, as the game commands show it. It is the value in `/players`, and the target of `/give` and `/ban`.
It is the position of the account in the account list. The game never removes an account, so the number stays the same after a restart.
It is not the account ID that the client sends. That ID is the login of the account and stays private.
A stale reference returns `SV_NOT_FOUND`. A null output pointer returns `SV_INVALID`. A call from a plugin that is not running returns `SV_STATE`. A failed call leaves the output unchanged.
A plugin finds the account of a player with `SvPlayerState.account`. To find a player by account number, read each player and compare the numbers.

## Connection read

`SvGameRead.connection` copies an IPv4 address, port, idle ticks and spectate value into `SvConnectionState`.
The IP bytes use dotted address order. The port uses host order. The spectate value keeps the game encoding, including -1.
Bots and players without a connection return `SV_NOT_FOUND`. Invalid connection counts return `SV_STATE`. Failed calls leave the output unchanged.
Plugins must restrict address disclosure to administrators.

## Account save notice

`SV_EVENT_ACCOUNT_SAVE` is `SV_EVENT_EXT(0)`. Subscribe with `SV_EVENT_EXT_BIT(0)` and read `SvAccountSaveEvent.account` and its final field, `reason`.
Reason 0 (`SV_ACCOUNT_SAVE_DISK`) follows the bulk disk save after every write and the file close succeed.
The bulk save posts one notice for all accounts, with `account = 0`. Failed saves post no notice. Only reason 0 means the data reached disk.
A plugin that needs a disk write must use reason 0.
Reason 1 (`SV_ACCOUNT_SAVE_DISCONNECT`) follows the existing money update at disconnect in `deleteplayer`. It saves to memory only.
There is no single-account disk save in this core. A disconnect notice does not promise disk persistence.
Notices enter the normal queue and run at a safe point. An account reference can be stale at delivery.
No short test room case exists because the write API does not request an account save.

An account read at delivery returns the current state, which can include later changes.

The disk notice covers all accounts, including an empty account list. Disconnect notices carry one account reference.
The `!= 0` check rejects a failed close.

## Connection byte order from the network code

The game stores `Connection.ip` and the low 16 bits of `Connection.port` in host order.
Those helpers read network bytes from `BsdSocket.addr` and convert them with `_byteswap_ulong` and `_byteswap_ushort`.
For `127.0.0.1`, the stored IP number is `0x7f000001`. `read_connection` splits it with shifts 24, 16, 8 and 0.
The snapshot port stays in host order. The existing split needs no change.
The port has unequal bytes to expose a missing swap.
The network permission below controls address visibility.

## Network capability and connection identifiers

`network` is capability bit 12, after `plugins`. It grants the real IPv4 address through the existing read table.
Danger level: high. Reads the IP addresses of players. No new table exists.
Without `network`, `ip` contains four zero bytes and `flags` bit 0 (`SV_CONNECTION_MASKED`) is set.
With `network`, the address is real and bit 0 is clear. Port, idle ticks and spectate remain available to read callers.
Both callers receive the same `address_id` and `network_id`. These are 32-bit HMAC-SHA256 hashes scoped to the profile.
The address hash uses all four IPv4 bytes. The network hash uses the first three bytes, with a separate domain byte.
The host makes a 16-byte salt with `BCryptGenRandom` once and saves it in `user/connection-salt.bin`.
The host reuses that file at each start. A missing salt creates new identifiers. A malformed file prevents startup.
Keep the salt private and preserve it to preserve the identifiers. The host never logs its contents or a real address.
The capability does not isolate native code. A native plugin can read server memory, including the salt.

## Round reset request

`SvGameWrite.world_reset` requests one reset at the start of the next server tick. It requires the normal write context and a ready world.
The main loop consumes the flag before control requests, plugin callbacks and simulation. Two requests before that point cause one call to `resetgame()`.
A normal core reset or host stop also clears the flag.

## Tick timers and plugin random

`SvHost.after(owner, ticks, id)` schedules one notice. `every` schedules a repeating notice. Delays are 1 to `UINT32_MAX` ticks.
Subscribe with `SV_EVENT_EXT_BIT(1)` and read `SvTimerEvent.id` from `SV_EVENT_TIMER`, which is `SV_EVENT_EXT(1)`.
Each owner has 32 timers. A repeated ID replaces that timer. A full table returns `SV_LIMIT` without a change.
Timers count host ticks across round resets. They run before the tick event. A timer made inside a callback starts on a later tick.
Stopping the owner or removing its timer subscription cancels its timers. Native plugin execution can still affect wall time.
`SvHost.random` advances one 32-bit state per plugin. The seed combines the game seed and plugin ID with FNV-1a.
The stream uses the LCG recurrence `state = state * 1664525 + 1013904223`, modulo 2^32. Activation restarts that stream.
Random calls never call `os_rand()` or change the game random stream. No clock or address contributes a separate seed.
The core supplies its existing game seed before the first world tick. Random calls before that point return `SV_STATE`.
Replay requires the same seed, plugin IDs, calls and ticks. A native plugin that reads a clock, file or network is outside this contract.
Only a sandboxed host can enforce the replay contract. No client playtest or replay recording ran.

## Human removal

`SvGameWrite.human_remove` ends the body of a real player, or a body that has no player. It needs the write callback context and a ready world.
The body leaves its vehicle first. The items in its slots leave them. The game ends the body record and its bonds and sends the normal event.
The player stays and has no body, as after a death. The host sends the update event of the player. No death tax runs and no team manager changes.
The body of a bot returns `SV_DENIED`. Remove a bot with `npc_remove`. A stale or wrong-kind reference returns `SV_NOT_FOUND`.
A body that sits in a vehicle that is not active, or whose player record is not active, returns `SV_STATE` and nothing changes.
The reference of the body is stale after the call. A plugin must not keep it.
The original client was not tested. The client of the player gets the normal end-of-body event. Client effect unknown.

## Core kick list

`SvGameWrite` appends `kicked_count`, `kicked_remove` and `kicked_clear` after `world_reset`.
They require the write callback context, a ready world and the `network` capability.
Check the table size before each new method. Version 1 remains valid for older callers.
The address argument has the four dotted-order bytes from `SvConnectionState.ip`.
No method lists addresses.
`kicked_count` writes the count. `kicked_remove` removes all entries for one address. No match returns `SV_NOT_FOUND`.
`kicked_clear` empties the list. A null output or address returns `SV_INVALID`.
An internal count outside 0 to `SR_MAX_KICKEDIP` returns `SV_FAILED` and changes nothing.
Without `network`, all three methods return `SV_DENIED` and leave outputs unchanged.

## Server control and console access

The public header `sv_server.h` adds two version 1 tables without changing an existing ABI 1 table.
Both capabilities have the `high` danger level. Native code retains full process access.

| Manifest capability | Bit | API ID | Table |
| --- | --- | --- | --- |
| `server_control` | 13 | 14 | `SvServerControl` |
| `console` | 14 | 15 | `SvConsole` |

### Server requests

`request(owner, request)` accepts `SV_SERVER_SAVE` or `SV_SERVER_STOP` from a callback on the server thread after the world becomes ready.
The method checks the current owner and the `server_control` capability at each call.
It refuses calls during simulation decisions or shutdown with `SV_STATE`.
A missing capability gives `SV_DENIED`. An unknown request gives `SV_INVALID`.

`SV_OK` means that the host accepted the request. It does not mean that the operation completed.
The host keeps one pending request across all plugins. A second request gives `SV_STATE`.
The main loop consumes the request once at the start of a later tick, before callbacks and simulation.
The host cancels an unconsumed request when its owner stops. A completed file save remains persistent.

`SV_SERVER_SAVE` writes the account file at that safe point.
Success posts the existing account-save notice with reason `SV_ACCOUNT_SAVE_DISK` and account zero.
The console records `Account save completed.` or `Account save failed.`.
Failure leaves the previous file in place. The request has no separate completion callback.

The writer uses a new temporary file in `user/`, checks each complete write, flushes the file and closes it.
It then replaces `user/server.srk` with `MoveFileExA` and the replace/write-through flags.
The writer removes its temporary file after a failed save.
The writer refuses an invalid account count, a reparse directory or a reparse target.
The file has the existing version 3 format. Integer fields use the Windows x86 little-endian representation.

| File part | Size | Fields in order |
| --- | --- | --- |
| Header | 12 bytes | Version `3`, account key, account count. Each field has 32 bits. |
| Each account | 60 bytes | Two 32-bit IDs, the full 32-byte name, money, play time and three look selectors. Each numeric field has 32 bits. |

The count ranges from 0 through 65,535. The total file size is `12 + 60 * count` bytes.
This format does not save `ban_time` or other account fields absent from version 3.
The save includes the account values already held by the core. It does not add a player-balance synchronization pass.

`SV_SERVER_STOP` asks the main loop to leave through the existing controlled shutdown path.
The plugin callback returns before shutdown starts. A stop request does not add an account save.
The normal exit convention remains unchanged.

`SV_SERVER_RESTART` asks for the same controlled shutdown as `SV_SERVER_STOP`. The server then exits with code 75.
The launcher starts the server again when it sees code 75 and the user did not press Stop. A service wrapper may start it again on the same code.
A normal stop exits with code 0, and a failed plugin exits with code 2. A restart request does not add an account save.
`tests/restart_check.ps1 -DataFolder DIR` runs the real server with a plugin that asks for each request and checks the codes 75 and 0. The server needs the game data from `DIR\data`, so ctest does not run the check.

### Console commands

`execute(owner, line, reply, capacity, length)` runs one registered plugin command as console actor zero.
The caller needs `console`. The command owner runs with its own capability set.
Console authority satisfies public, administrator and local command permission levels. No player or living body is required.
The host restores the calling owner after the command returns.

The method accepts a line of 1 through 300 bytes, with an optional leading slash.
The command name must fit its existing buffer. Arguments contain at most 255 bytes.
An empty line, an excessive length, a newline, a carriage return or an invalid output argument gives `SV_INVALID`.
Only registered plugin commands run. An unknown command, a game-owned command or a host management command gives `SV_DENIED`.
Nested console calls and calls during simulation decisions or shutdown give `SV_STATE`.

The reply buffer capacity includes its final zero byte and ranges from 1 through 4,096 bytes.
The reply contains at most 4,095 text bytes. `length` excludes the final zero byte.
The host captures the command owner's `SvHost.log` output and `message_to` replies to actor zero.
It joins captured messages with newline bytes while capacity remains.
Replies to real players keep their existing destination.

The method returns the command result when the reply fits.
`SV_LIMIT` means that the command ran and the reply was truncated. The host terminates the reply even after truncation.
Callers must not repeat a command solely because its reply was truncated.
This operation does not subscribe to arbitrary server console lines.

### Existing host timers and random state

`SvHost.after(owner, ticks, id)` schedules one timer. `every` schedules a repeating timer.
Both methods require a timer subscription and the current callback owner on the server thread.
The delay ranges from 1 through `UINT32_MAX` ticks. Zero gives `SV_INVALID`.
Each plugin can hold 32 timers. Reusing an ID replaces that owner's timer.
The existing full-table result remains `SV_LIMIT` for ABI compatibility.

Timers run before the tick event at a safe point. A one-shot timer runs once.
Round resets keep timers. Removing the timer subscription or stopping the plugin removes its timers.
Callbacks can schedule another timer with a delay of at least one tick.
Wrong owner, wrong phase or a missing timer subscription gives `SV_STATE`.

`SvHost.random(owner, value)` uses state private to that plugin.
The host combines the game seed with the plugin ID. Activation restarts that stream for the same seed and ID.
The method never consumes `os_rand()` or the game's Mersenne Twister state.
A null output gives `SV_INVALID`. A missing seed, wrong owner or unsafe phase gives `SV_STATE`.

### Connection privacy

The existing `connection` read requires `read` and a valid player reference.
A bot or a player without a connection gives `SV_NOT_FOUND`.
An invalid output buffer gives `SV_INVALID`. An invalid connection count gives `SV_STATE`.

The `network` capability remains bit 12 with danger level `high`.
Without it, `ip` contains four zero bytes and `SV_CONNECTION_MASKED` is set.
With it, `ip` contains the real address and the flag is clear.
Both callers receive the port, idle ticks, spectate value, `address_id` and `network_id`.

The host derives the 32-bit identifiers with HMAC-SHA256 and a secret 16-byte salt in `user/connection-salt.bin`.
The address identifier uses all four bytes. The network identifier uses the first three bytes with a separate domain.
The profile reuses its salt across starts. A malformed salt refuses startup without replacement.
No API, log or plugin listing returns the salt. Native code remains outside this privacy boundary.
Identifiers can collide and are not authentication credentials.

The address bytes use normal IPv4 order. The port is a host-order number.
The focused test sends a real UDP join from `127.0.0.1` through the server receive path.
It compares the stored address and API bytes with that address, and compares the port with `ntohs`.
The sender uses unequal port bytes so a missing conversion is observable.
The original client was not tested.

