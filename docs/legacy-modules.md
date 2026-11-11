# Legacy module catalog

Status: requirements list. [Admin Commands](plugin-admcmd.md) implements the supported administration commands.
Other modules remain planned.
It guides the plugin API work in [system coverage](plugin-api-map.md) and the production plugins in [the milestones](plano.md).

## Source and use

A friend of the group made a script manager for the original dedicated server.
It patches the memory of the running original server and injects native DLLs. That method does not work in SubVulpes,
because SubVulpes is a native server with no original image. The tool and its files stay outside this repository.

This list keeps the behavior that players and administrators know. It does not copy code, addresses or text from the tool.
Each plugin here is a new official plugin on the public API.
The first list used version 0.7.1. Version 0.8.0 (with its documentation) is now compared and merged. See [Changes in 0.8.0](#changes-in-080).

A command in the chat goes to the plugins first, after the host check of the permission level.
The inherited commands in `src/net/commands.c` stay until a plugin replaces them and passes its checks.

## Chat commands

All commands need the admin flag, a living player and, where noted, a body on foot.
"Now" means the current read and write API is enough. The needs (N1 to N55) are in the next section.

| Command | Behavior to keep | Needs |
| --- | --- | --- |
| `/imortal` | Toggle. Hold the health at 100 each tick without the god mode flag, so hits and pushes still show. Ends on body change, disconnect, reset or plugin stop. | Now (tick, the property `human.health`), N18 |
| `/heal` | Set health 100, `bleeding` 0 and `stun_timer` 0. The tool calls the stun timer "drunk level" and the bleeding flag "death mark". The source of 0.8.0 confirms these three writes and no others. | Now (the properties `human.health`, `human.bleeding`, `human.stun`) |
| `/kill` | Kill the own body: health -5 and `bleeding` 1. | Now (the properties `human.health` and `human.bleeding`) |
| `/item N` | Make item type 0 to 26 and put it in a free hand. A knife (24) starts in attack state. Fail without cost when there is no free hand or item. | N6 |
| `/vehicle N` | Make vehicle type 0 to 13 four units in front of the head, color 0. Do not seat the player. | N7 |
| `/arm N [PLAYER]` | Select one of six weapons and give it in a free hand. | N6 |
| `/kit N [PLAYER]` | Give the selected weapon, four magazines and two bandages. Removed from `sv.admcmd` 0.3.0 by the owner: `/arm` gives the weapon with its magazines, and `/item` gives a bandage. | N6 |
| `/heavydude` | Multiply the 15 physical values of the body by 100000, once. A joke command. | N19 |
| `/delh` | Delete the nearest human on the aim line, up to 100 units, never the caller. | N10, N8 |
| `/team N` | Set team 0 to 5 and send the update. | N3 |
| `/deactivate ID` | Toggle the active record of the human of player ID (0 to 255). Another admin can toggle it back. Version 0.8.0 gives the target rule only, not the effect on the world. | Refusal likely (see below) |
| `/give admin ID` | An admin gives the admin flag to a connected player. Reject empty slots and bots. Lasts until disconnect. The inherited `/give` is a different command (cash). | N2, N1 |
| `/give money ID N` | An admin adds N (1 to 100000000) to the money of the player and of the saved account. It takes nothing from the caller. Refuse a bot, a missing account and a sum that overflows. A numeric `/give ID N` stays with the inherited command. | N21 (economy), N1 |
| `/npc` | Make a pedestrian bot 3 meters in front, with no car. Queue the request to a safe point and refuse a second request while one waits. | N8 |
| `/bmhelp` | Show the help to the caller only. | N1 |
| `/turn N` | Copy name, eyes, skin and hair from saved account N to the caller. Copy no money, identity, team or rights. | N4 (read exists) |
| `/herobrine` | Set the eyes to type 3. | N4 |
| `/tp` | Teleport to the surface on the aim line, up to 2000 units, 1 unit above it. Fail if the aim hits nothing. | N10, `teleport` now |
| `/gobase N` | Teleport to base N (0 to 4: Goldmen, Monsota, OXS, Nexaco, Pentacom). The tool uses fixed positions of the original map, height 25. Read them from the level data when it has them. Do not change the team. | Level read, `teleport` now |
| `/sound N` | Play sound 0 to 47 at the aim point, volume 1, pitch 0.15. | N10, N11 |

The owner changed the arm requirement on 2026-10-31. Weapon numbers are 1 to 6.
Commands that affect a player accept an optional target after their values.
See [Admin Commands](plugin-admcmd.md) for the supported forms.

The tool lists `/nick` in a table but never runs it. Do not build it.

## Modules

| Module | Behavior to keep | Needs |
| --- | --- | --- |
| Black market | Two phones that sell a knife (type 24) and a bandage (type 12) for set prices and waits. A buy needs a living player near the phone. Take the money only after the item exists. | N6, N15, N16 |
| Knife attack | Damage 25, range 1.5, wait 20 ticks, in the active hand, with a push on the hit body. | N10, N12, the property `human.health` now |
| Chat in console | Log the name and text, with the id as an option. Limit the text to 63 bytes and strip control bytes. | N16 |
| Maximum players | Set the active player limit from 1 to 32. Do not kick anyone. | N14 |
| NPC count | Set the number of bots that the start makes. Do not add or remove now. | N14 |
| Starting money | Set the money of a new account. Do not change existing balances. | N14 |
| Faster truck | Multiply speed range and engine force of trucks (model 9) by a factor of 1 to 4. Apply to existing and new trucks. | N17 |
| Truck in the dealership | Keep one truck in each dealership catalog for a set price. Replace the last offer when full. Make no truck until bought. | N15 |
| Death messages | One message of at most 63 bytes for each death. Check the message index. | N16 |
| Instant death | Run the death processing at once. The game keeps the penalties and the body split. | N16 |
| Eject from vehicle | When a crash damages the vehicle, eject the occupants and zero the speed of the body parts. Model 11 is exempt. | N9, N16 |
| Punch | Damage from the relative speed of the fist, a head bonus and a push. Defaults: scale 100, head 5, push 8.5, threshold 3, wait 70. | N12, `set_health` now, N16 |
| Spawn protection fix | Cancel the spawn protection when the body moves faster than a set speed. | N13 |
| Safe spawn area | A zone where spawn protection applies. | N13, N16 |
| Spawn at the dealership | Spawn at a set position. | N16 |
| Aggressive bots | Bots look at the torso (body part 1) and not at the head. Aim mode turns on when a bot adds a target and off when it drops one. The random 25% hesitation before a shot goes away. No change to damage, range or fire rate. | N20 |
| Clunky ragdolls | Skip the step that resets the orientation of a body part, so a ragdoll reacts with less correction. A core physics switch, not a rule. | N17 |
| Critical health | Below 10 health: the body bleeds and leaves its vehicle. The tool also moves nine health checks in the core to the value 9, so the death and drop steps start earlier. | N9, N5 now, N17 for the checks |
| C4 and detonator | Phone 4 sells a kit (price 500, wait 400 ticks). The player drops the red disc, waits about one second, and dials 1 on the kit phone. The blast hurts and pushes all bodies in a radius. Defaults: damage 200 (0 to 500), radius 8 (1 to 30), push 3 (0 to 5). A price of 0 gives free kits. Disabling the module removes the live kits. It needs the phones of the black market. | N15, N6, N12, N16 |
| Crazy NPC pair | `/npclouco` (admin, living, on foot) makes a driver (no weapon) and a shooter in the passenger seat. The car appears on a free street near the caller, found with the road graph. At most four pairs. `/npclouco limpar` removes the pairs and their cars. Weapon 0 to 5 (M16, AK-47, MP5, Uzi, 9 mm, Magnum), car color 0 to 5, car model 0 to 13, health values up to 1000000, names up to 20 bytes. The shooter fires 30 rounds, then waits 120 ticks to reload. If the driver dies or the car is destroyed, the shooter leaves the car and fights on foot. The module announces its own deaths with names and the killer. | N20, N27, N28, N29, N24, N8, N16 |
| Team rounds (PvP) | Only in the Rounds gametype (3). Count the living humans of three teams at the round start (bots excluded). A death, a leave or a team change removes the player until the next round and sends a line with the survivors. The round ends on time (default 300 s) or, with the early option, when one team is left (two teams must have started). A tie is not a win. After the end the score freezes for a pause (default 5 s), then the round resets and the result is announced after the reset, because the reset clears the event queue. The module must not run with the death message modules. | N26, N16, N1 |

## Rule of generality

Every operation in the next list is general. It has no name, price, number or item of one module. A module gives the meaning. The core gives access, bounds and events.
When a need looks like one module, split it: the general operation goes in the core table and the rest stays in the plugin.
A plugin keeps its own state in plugin storage (N18) and not in unused fields of the game records.
The official plugins use only these operations and the events. They have no other door into the core.
A plugin that needs a thing that the API does not give waits for a new general operation.

## Needs

| Id | Operation or event |
| --- | --- |
| N1 | Send a message to one player. Done: `message_to`. The server filters the event for each connection, so the original client gets a normal message. |
| N2 | Set the admin flag of a player and of its connection, as the password login does. Done: the property `player.admin`. |
| N3 | Set the team of a player and send the update. Done: the property `player.team`. |
| N4 | Write the look fields and colors of a player or human and send the update. Done for the five look selectors: `set_looks`. The colors have no support: `colors[4]` of a player and of a human never leave the server, and no server code reads them after the creation. A write would change nothing that a client or the game shows. The close of S3 lists this as "no support". |
| N5 | Write the `bleeding` flag and the `stun_timer` of a human. Both fields exist and the read API already shows them. Done: the properties `human.bleeding` and `human.stun`. |

About the "drunk" look. This server has no drunk field. A body staggers in two cases, and both come from the code:

- A hit sets `stun_timer` to 3 or 6 times the damage (`bullet_sim.c`). It counts down by 1 each tick and weakens the muscles of the torso while it runs.
- Below 25 health, `body_strength` drops and wobbles (`human_strength` in `humans.c`). The game recalculates it every tick from the health.

So a heal needs health 100 and `stun_timer` 0. `body_strength` follows by itself. This is read from the code, not tested.
The same two effects give a way to make a body stagger on purpose: set a high `stun_timer`. That needs a write for the timer, with an upper bound.
The "death mark" of the tool is probably not a field. It is likely a state that existing game systems already use. In the code, death works like this (`humans.c`):

- Health below 1: the body drops the first item of slots 0 to 6.
- Health below -4: the body is dead. `despawn_timer` counts down from 3600. Below 2700 the game releases the player (`human_releaseplayer`): the death tax, the loss of team managers in gametype 4, `human_id` of the player set to -1 and `player_id` of the body set to -1.
- While health is -4 or more and a player holds the body, the game sets `despawn_timer` back to 3600 on each tick.
- In a building of type 6 (a hospital) a body with a player gains 1 health each 32 ticks and stops bleeding.

So health 100 revives a body that is dead for less than 900 ticks (15 seconds), with no new write. After the release the body has no player, and a heal does not give it back. The player must respawn.
This reading is from the code and not tested.
Version 0.8.0 of the tool settles the open question. Its `/heal` writes health 100, the `bleeding` flag 0 and the `stun_timer` 0, and nothing else.
Its `/kill` writes health -5 and `bleeding` 1. So the "drunk level" is `stun_timer` and the "death mark" is `bleeding`. Both fields have write methods already.
| N6 | Make, remove, link and unlink items, with the checks that keep both sides of a link whole. Done: `item_create`, `item_remove`, `item_equip`, `item_attach` and `item_detach`. |
| N7 | Make and remove vehicles and other objects. Done: `object_create` and `object_remove`. Train pieces are refused. |
| N8 | Make and delete a human, and make a bot player with a human at a safe point. The pool guards and rollback exist. Done for bots: `npc_create` and `npc_remove`. Not done: delete the body of a real player. Kill it with `set_health` instead. Decision of 2026-11-08, read from the code and not built: a safe delete exists. `deletehuman` (`human_body.c`) unlinks the player, takes the items out of the slots, stops the bonds, ends the record and posts the event. It does not take the body out of a vehicle and it does not send an update for the player. So the method must call `vehicle_exit` first, then `deletehuman`, then send the player update. It must refuse a bot (use `npc_remove`) and a stale reference. It applies no death tax, which is right for an administrator delete. The test must compare the pool counts before and after, and check that the player has no body and can respawn. Done on 2026-11-08: `SvGameWrite.human_remove`, with `/delh` of `sv.admcmd` as its first user. See [plugin game API](plugin-game-api.md). The delete by aim line is not tested. |
| N9 | Eject a human from a vehicle. Done: `vehicle_exit`, with the option to stop the bones. The exit code of the game is now one shared function, `human_leavevehicle`. |
| N10 | Aim and line queries against the level, objects and humans. They copy the result out of the shared scratch record. Done: `line` and `aim` in the read table, and the sphere query `within` (N39). Not done: a query of a surface material or a block (N39, N40). |
| N11 | Play a known sound at a position, with a volume and a pitch. The index is from 0 to the size of the sound list. Done: `sound`. This server has 47 sounds (0 to 46). The legacy tool accepts 47 too, and this server refuses it. |
| N12 | Add a velocity change (an impulse) to one bone of a human or to an object, with a bound on its size. Done: the properties `human.bone_velocity` and `object.velocity` with `add`. |
| N13 | Write the spawn protection timer. Done: the property `human.protect`. |
| N14 | Named settings of the core in one table. Each has a type and a range, a read, a write with a range check and a change event. The first entries are the player limit, the money of a new account and the bot count. Another setting adds a row, not a new method. Built and merged (typed table `SvGameSettings`, capability 6). The server reads the money of a new account and the two traffic counts from it. See [core seams](plugin-seams.md). |
| N15 | Phones, in a general way. The core knows no shop and no price. A cell phone item keeps its dialed digits in `phone_number` and its state in `call_state`, and the game calls another phone at 4 digits (code read, not run). A plugin gets an event for each digit (phone item, number so far, the human that holds it) and one when the game is about to call. It can claim the number, so the game does not call, or let the game go on. A write puts a phone back to idle. The plugin gives the number its meaning with the methods that exist: a sale (`add_money`, `item_create`, `item_equip`), a command or a quiz. A short code such as 24 needs a rule for its end, for example a wait of some ticks. The plugin owns that rule. A second, separate part: read and replace the rows of the menu shops of a building (type and price), for the dealership truck. Built and merged (capability 9, the phone rows `item.phone_state` and `item.phone_number`, catalog reads and writes). The phone digit and call decisions wait one tick for the plugins, and the two purchase functions check their slots first. See [core seams](plugin-seams.md). |
| N16 | One event mechanism. Each event has a phase (observe or decide), a typed payload and a limit on what a decision can change. The first events: chat text (observe), human death (observe, and early processing), spawn (decide the position), damage (N24), menu purchase (decide), connect (decide) and phone (N15). Another event adds a row, not a new mechanism. Done: the record, the queue, the notices, the decisions at a safe point and the decisions inside the simulation. See [plugin events](plugin-events.md). |
| N17 | Named engine parameters in one table: per object type (for example the speed range and the engine force) and global physics switches (for example the reset of the orientation of a ragdoll part, or the health value where the death steps start). Each has a type, a range and a default. The host puts the default back when the plugin stops. Built and merged (capability 8). The owner cleanup and the consumers of the two global rows are installed. See [core seams](plugin-seams.md). |
| N18 | Plugin storage with a size bound, in three scopes: session (one player, cleared when the player leaves or the plugin stops), account (kept on disk, tied to an account) and server (kept on disk). See the notes below. Built and merged (capability 7). The host opens the storage of a plugin before its enable callback, and the session values leave with the player. See [core seams](plugin-seams.md). |

Plugin storage (N18) is built (see the row above). A native plugin can still write its own files, but the host does not bound or clean them. The rules below are the design that the storage follows.
The design rules:

- Do not add fields to the account record. Its file format is fixed (0x4c bytes for each record), and old account files must keep working. The player record is gone when the player leaves.
- Keep the data in a private plugin folder that the installer already keeps during updates. The data stays after a disable and goes only when the administrator asks.
- The key of the account scope is an opaque reference. The host maps it to the real account id. The plugin never sees the id, as in the read API.
- Each plugin has its own keys, a limit for each value and a limit for the total. The host saves at a safe point and in one step, so a crash leaves the old file whole.
- Persistent bans, permission groups and ranks depend on the account scope.
| N19 | Write the body part masses. Done: the property `human.bone_mass` (15 slots, 0.5 to 500). The client effect is not known. |
| N20 | AI control: make a bot walk to a point, follow waypoints, aim at a player and hold a street route. Done: the table `SvGameAi`. See [AI control](plugin-ai.md) and [the map](plugin-api-map.md#planned-operations-for-ai-economy-and-missions). |
| N21 | Economy: read the companies, change a price or a share count, add or remove stocks. Done: the table `SvGameEconomy`. See [the map](plugin-api-map.md#planned-operations-for-ai-economy-and-missions). |
| N22 | Missions: read the slots of a team, start or cancel a mission of an existing kind. Done: the table `SvGameMission`. See [the map](plugin-api-map.md#planned-operations-for-ai-economy-and-missions). |
| N23 | Plugin configuration set by the administrator, in the launcher or with a command. Done: see [plugin settings](plugin-config.md). The notes below are the design. |
| N24 | Damage decision. Before a bullet, a fist, a blade or a vehicle applies damage to a human, an object or a wheel, a plugin can allow it, scale it or cancel it. The event says the source, the target and the amount. Done: the properties `human.damage_taken_scale`, `human.damage_dealt_scale` and `object.damage_taken_scale`, and the decision inside the simulation. |
| N25 | Closed. The read table already gives the buttons, the velocity of each bone and `reload_ticks`. The punch module of the tool keeps a counter in a free field of the human record. A plugin keeps such a counter in its own storage (N18), and not in the game record. |
| N26 | World flow. The world read already shows the mode, the round, the state and the timers. Add writes for the timers and the state, and a request to reset the round. No operation names a gametype. Done: the properties `world.state`, `world.timer`, `world.round_time`, `world.wait_time` and `world.end_time`, and the round event (N33). Done: `world_reset`, a request that the main loop runs at the start of the next tick. |
| N27 | Road graph query: find a free street position and direction near a point, with the next node. The crazy NPC pair uses it to place the car. It copies out of the shared graph and returns a bounded result. Done: `street_near` in `SvGameAi`. It gives one free street position, the direction and the next node, within 512 units. See [bot API additions](game/bot-controls.md). |
| N28 | Seat a human in a vehicle seat (driver 0, passenger 1) and take it out. The seat rule is in the core: `vehicle_id` and `vehicle_seat` of the human. Includes the eject of N9 as the second half. Done: `vehicle_enter`, with `vehicle_exit` as the second half. |
| N29 | Object health and protection: set the health of an object, and mark an object or one of its wheels as protected from damage (use with N24). A health range wider than 100 for a human, if the core accepts it. Done for objects: the property `object.health`, and `object.damage_taken_scale` 0 protects the object and its wheels. Not done: a health range wider than 100 for a human. The packet writer clamps the sent health to -10 to 117. Decision of 2026-11-08, read from the code and not built: a health above 100 is safe for the game code. The only rules that use 100 are the hospital heal (it stops at 100) and the first value at spawn. Damage and bleeding have no cap. The packet writer sends at most 117, so a client shows a full bar above that, and the client effect is unknown. Widen the range of `human.health` to -100 to 1000. The tests that add past 100 in `plugin_game_fixture.c` must change with it. Done on 2026-11-08: the range of `human.health` is -100 to 1000. |
| N30 | Teleport an object and an item. A human has `teleport`. An object derives its frame from its particles, so the move shifts every particle and bond point by one offset and keeps the speeds. An item moves its position and, for a held item, refuses or lets go first. Done: `teleport` takes a human, a vehicle or a free item. A held item gives `SV_STATE`. |
| N31 | Property rows for items and objects: `item.ammo`, `item.time_to_live`, `object.time_to_live`. Each has a range. The health of an object is N29. Done. |
| N32 | A property row for the ban time of an account (`Account.ban_time`). Keeping a ban on disk after a restart needs N18. Done: the property `account.ban_time`. The disk part waits for N18. |
| N33 | More notice events: round start and end, a human enters or leaves a vehicle, an item is picked up, dropped or used, a shot is fired, an object is destroyed, an account is saved. Each one is a row in the event mechanism (N16), with its own bit. Done: the events `SV_EVENT_ROUND`, `SV_EVENT_VEHICLE`, `SV_EVENT_SHOT`, `SV_EVENT_OBJECT_GONE` and `SV_EVENT_ITEM`, and the account save event `SV_EVENT_ACCOUNT_SAVE`, which has a reason code (a save to disk, or the update at disconnect that stays in memory). |
| N34 | Chat text replacement. Today a chat decision only allows or denies the text. Let it give a new text (at most 63 bytes, printable ASCII) for rank prefixes and filters. Done: the field `replacement` of the chat event. |
| N35 | A plugin offers its own API to other plugins, with a version, and a plugin declares the dependency. Serves the C4 module that finds its sales through the black market. Done: `provide` and `find` of the table `SvPlugins` (capability 12), with a version that must be equal, and the dependency rule. |
| N36 | The `conflicts=` key in `plugin.ini`, and a clean result "refused to start" with a reason text for the log and the launcher. Done: `conflicts=` (the plugin that starts first wins, a live enable that conflicts is refused unsaved) and `refuse` of the table `SvPlugins`. |
| N37 | Cleanup of what a plugin owns when it stops: the phones, kits and bots that it made. The host keeps a list for each plugin and removes the entries at the stop. Done: the host puts back the first value of each slot that the plugin changed, and it ends the items, vehicles and bots that the plugin made. The write method `disown` keeps one. Phones are not a separate thing: a phone is an item. |
| N38 | Library plugins: a plugin that has no game behavior and offers a shared API to others (N35), with load order and a version rule. Done: `role=library` (no events, no commands, no game tables), load order from `dependencies=`, and the lowest version in `sv.lib@1.2`. See [plugin libraries](plugin-libraries.md). |
| N39 | Queries that are left from N10: a sphere (who is within a radius of a point), the surface material at a hit, and a block cell. A sphere makes an explosion cheap to build from existing parts. Done for the sphere: `within` in the read table. Done: `surface` (the material at a level hit) and `block` (one cell). See [level API](game/level-api.md). |
| N40 | Block edits of the world: set, clear and read a block. The server may change its world state, but the original client may not show it. Mark "client effect unknown". Done: `block_set` (clear or solid), with a restore when the plugin stops. Client effect unknown. See [level API](game/level-api.md). The test room must check it in the game (N69). |
| N41 | A timer owned by the host (call me after N ticks) and a random generator that belongs to the plugin, so plugin randomness does not change the sequence of the engine. Done: the host timers and the plugin random stream exist. The infrastructure work checked and documented them. |
| N42 | Visual events that the original client already reads: `bullet` (the event of a shot with type, position, speed and weapon item, with no bullet in the server) and `bullet_hit` (the impact effect). They give a pseudo-weapon a tracer and a spark. The client may also act on them. Done: `bullet` and `bullet_hit`. Mark "client effect unknown". |
| N43 | Property rows for the 27 item types (price, mass, fire delay, bullet type, bullet speed, spread, recoil, magazine, one hand) and the 4 bullet types. They change every item of the type. N17 keeps object types and physics, so the two do not overlap. Done: the rows `item_type.*` and `bullet_type.*`, with a restore at the stop of the plugin. Mark "client effect unknown". |
| N44 | Rename a player or a bot. The player record has its own name (31 characters). The update event of the player carries the name, so the clients can show a new one. The account name stays, because the account file keeps it. Done: `set_name`, and `npc_create` takes the name, so the first update event has it. Mark "client effect unknown" until the original client shows a changed name. |
| N45 | Possess a bot: give a player the human of another player (a bot), with the links on both sides kept whole (`human_id` of the player, `player_id` and `account` of the human, the team) and the update events. The old body of the player needs a rule: it stays as a body with no owner, or it goes to the bot. This overlaps N20, where a plugin overrides the input of a bot. Mark "client effect unknown": the camera of the client follows the human of the update event. Done: `possess` in `SvGameAi`. The old body stays without an owner. No method gives the body back (N66). |
| N46 | Speech above a head for any human, bots included. The chat of a player already sends it as a message event of kind 1 with the human as source. The range is 0 (short), 1 (normal) or 2 (long). The original client shows it, because it is the normal chat bubble. Done: `speak`. |
| N47 | Voice frames. The server relays the voice of a player as encoded frames, 6 in each input packet, kept in a ring for each player. A plugin writes frames into the ring of a speaker (a real player or a bot), and the clients near that speaker hear them. The host takes the bytes of the frames. Encoding, reading a file or loading a stream from the web stay in the plugin. |
| N48 | Voice routing. Today a listener hears a speaker within 256 units or through a linked phone pair, in 8 slots. A plugin decides who hears whom: everybody, those near a point, or one listener. This is a hook in `calculateplayervoice`, and the 8 slots stay the limit. Done on 2026-11-11: `voice_route` (deny or force a speaker for a listener). Radios do not change. Client effect unknown. |
| N49 | Trains. A plugin reads and controls the trains: speed, stop and go, the station stop time, spawn more, delete one. Today every write to a train piece gives `SV_DENIED`, because the track system owns the pieces. The data: `createvehicle(0, pos, rot)` in `object_create.c` makes a locomotive (type 12) and two bogies (type 13) tied to it with bonds. `track_target` is the track piece that the locomotive drives to, `track_last` is the piece under it, and `stop_ticks` counts the stop at the station (the train leaves at 0x4b0). `game_setup.c` makes two trains, at `g_track[74]` and `g_track[2]`. Needs: (1) reads of a train (the three pieces, speed, `track_target`, `stop_ticks`); (2) writes for speed, `track_target` and `stop_ticks`; (3) `train_create(track_piece, direction)`; (4) `train_remove` that ends the locomotive and both bogies together with their bonds, and never one piece alone; (5) a limit on the number of trains. The denial of the single pieces stays. The train is a box with window holes and one door face (`door_face`). The code of the locomotive (`objects.c`) moves the door face with the speed, so a plugin that sets the speed moves the door too. The train has no seats and no interior, so "control" means speed and stop, not driving. Driving or riding needs new seats and is not part of this row. Risk: the removal must leave no bond that points to a dead particle. Mark "client effect unknown": the client reads the object events, and a train that it has never seen may not draw. Serves the sound test 64, which wants a quiet world.  **In part on 2026-11-11:** the property `train.stop_ticks` (only the locomotive) holds a train at a station or sends it on. The write `train_speed` sets the target speed of a locomotive. The properties `train.track_target` and `train.track_last` read the track pieces. Speed reads already exist as `object.velocity`. Not done: a write for the target track piece, `train_create` and `train_remove`. |
| N50 | Connection read and the privacy of the address. A plugin reads the address, the port, the idle ticks and the spectate flag of a player. Without the capability `network` (bit 12, high danger) it gets the address with all four bytes at zero, and two numbers made with a secret salt: `address_id` for the same address and `network_id` for the same /24 network. With the capability it gets the real address. Done: the read `connection` (R1), commit `6b5355c`, with the byte order still to prove against the real game data. Done: the capability `network` and the masked record, with salted `address_id` and `network_id`. The capability does not isolate native code, which can read the memory of the server. |
| N51 | Done: `SvGameWrite.kicked_count`, `kicked_remove` and `kicked_clear` require `network`. No method lists addresses. Counts stay within `SR_MAX_KICKEDIP`. |
| N52 | Level reads: buildings and bases. The entity kinds have no building. A plugin needs the position and the type of each building (bases, shops, hospitals, dealerships), for example to teleport to a base. The catalog of the menu shops already uses the index of a building (N15). Done: `building_count`, `building`, `base` and `area` in the read table. See [level API](game/level-api.md). |
| N53 | Server control: force a save of the accounts, ask for a controlled stop and ask for a restart. Each is a request that the main loop runs at a safe point, as for the reset of the round (N26). A persistence plugin needs the save, and an administration plugin needs the stop. Not started. Done in part: a request to save the accounts and a request for a controlled stop (capability `server_control`). Not done: the restart, which needs a launcher rule. The save request needs no sync of balances. In gametypes 3 and 4 the player simulation copies the money of each player into its account at every tick (`player.c`), with the stocks at their value. `player_calcassets` is not a sync: it sells the stocks and adds the price of owned vehicles, and the game runs it only at the end of a round. A plugin must not call it for a save. |
| N54 | A console actor and console lines. (1) A plugin runs a command line as the console, with no living player, and captures the replies as text. The replies of a command go to the chat of the caller today, so the host needs a reply channel for a console caller. This lets a remote console plugin (RCON) work like the one of Minecraft: a client connects with a password, sends any command of a plugin and reads the text answer. The remote console speaks the public Source RCON protocol, so ready programs work. The commands that the original game owns still need a living player and stay out. (2) A plugin may receive the lines that the server writes to its console, for an optional log output plugin. A plugin can already write its own log of events, so part (2) may not be needed. Done: part 1, the call that runs a plugin command as the console (capability `console`). Part 2 is skipped. |
| N55 | A secret for a plugin setting, for example the password of a remote administration plugin. Done: the setting type `secret` (up to 63 characters). The host hides it in every listing and log. See [plugin settings](plugin-config.md). |
| N56 | Done: `SV_EVENT_CORE_COMMAND` decides inherited commands. Default keeps the admin rule. Allow permits that command. Deny stops commands and login. |
| N57 | Done: `SV_EVENT_ADMIN_LOGIN` precedes the password check and attempt counter. `SV_EVENT_ADMIN_SUCCESS` follows both flags. Neither event carries a password. |
| N58 | Bot aim posture. `aim_mode` sets only bit 2 of `input_buttons` (`plugin_ai.c`). The chest reads that bit (`human_arms.c:302`). The arm torque reads only `move_mode` (`human_arms.c:491`), so a bot with `move_mode` 1 still swings its gun to the side. A bot never gets `move_mode` 2: the server reads it from a client packet (`server.c:487`), and the game setup writes 1 (`game_setup.c:211`). The player simulation copies it to the human each tick (`player.c:100`), so the write goes to the player record. Needs: a write of `move_mode` for a bot (0 to 2), or an `aim_mode` that sets it, and a restore when the plugin stops (N37). Until then `aim_mode` gives no true aim. Read from the code, not run. Reported by a plugin author. Done: `aim_mode` sets `move_mode` 2. A bot that aims walks at half speed, as a player does. The owner must accept this. |
| N59 | Chase mode of a car driver. `Player.ai_chase` has no write in the API. Only `scenario_createchase` sets it (`scenario.c:261`). When it is on, the car code (`ai_car.c`) uses a target speed of 30 or 40 in place of 20, a lateral limit of 32 in place of 16, changes lane when an object is ahead, and ignores red lights. Needs: a property for a bot driver (0 or 1), with `SV_DENIED` for a player that is not a bot, as `aim_mode` does, and a restore when the plugin stops (N37). Read from the code, not run. Reported by the author of the crazy NPC pair. Done: `chase_mode` in `SvGameAi`. It switches all four chase effects together (N65). |
| N60 | Death counter. `despawn_timer` of a human is readable (`despawn_counter` of `SvHumanState`) and has no write. The instant death mode of the legacy tool moves it forward after health -5. The game sets it back to 3600 on each tick while a player holds a body with health -4 or more (`humans.c`). Below 2700 the game releases the player (`human_releaseplayer`). So a write needs a range (0 to 3600), a test of the release, and a check that it does not end a living body. The author calls this the "death counter". The code fits, but the author must confirm. Done: the property `human.despawn`. The plugin author has not confirmed that this is the field that the need means. |
| N61 | Passenger aim. `seated_target` already gives an armed passenger in seat 1, 2 or 3 a living body to aim at (N20, `plugin-ai.md`). The author lists "control the aim of a passenger". The need beyond `seated_target` is not known (for example a point or a direction in place of a body). Ask the author before any code. Covered on 2026-11-11 by `aim_at` and by `override_input_partial` for a passenger. The author has not confirmed that this is what the need means. |
| N62 | Account number. A plugin needs the number that the game commands use for an account (`/players`, `/give`, `/ban`), to target a player by account across sessions. The reference of an account is opaque, and the account ID of the client is a login that a plugin must never see. Done: `SvGameRead.account_number`, the position of the account in the account list. |
| N63 | Road graph reads. A plugin cannot see which roads connect, so it cannot build a route from one place to another. Done: `road(index)` (ends, lanes, kind, direction) and `intersection(index)` (position, the road and extent at each side, the light of each side) in `SvGameAi`, with the counts. They copy out of `sr_data_city` and bound every index. See [bot API additions](game/bot-controls.md). |
| N64 | Driving control. A route (`street_route`) is a closed loop of 2 to 64 points. A plugin cannot send a driver from A to B and stop there. Add an open route with an end action (stop or hold), a stop at a point, and a bound for the target speed of a driver. Today only chase mode changes the speed. Done on 2026-11-11: the open route with an end action (`street_route_open`). `driver_speed` sets the base speed of a driver, with no upper bound. The stop at a point is not done. |
| N65 | Chase parts. `chase_mode` switches four effects together: a higher target speed, a larger lateral limit, a lane change, and no stop at red lights. Add one flag for each part, and keep `chase_mode` as the sum of the four. Check that each part works alone in `ai_car.c`. Done on 2026-11-10: `chase_effects`. The test checks the API results and the stored value of each mix, not the driving of each part alone. |
| N66 | Give the body back. `possess` (N45) has no reverse. The possession ends only when the plugin stops. Add `unpossess(player)` with the same link checks as `possess` and the same rollback. Decide the rule for the body that the player left. Mark "client effect unknown". Done on 2026-11-10: `unpossess`, with a test in `tests/n_bots`. |
| N67 | Street query options. `street_near` gives one position. Add a list of candidates (at most 8), a filter for the distance to humans and to objects, and a filter for the kind of road. Keep the same bounds. Done on 2026-11-11: `street_list`, with the filter for the kind of road and the two gaps. |
| N68 | Aim helpers. A plugin that wants a bot to aim at a point must compute yaw and pitch itself and send them with `override_input`. Add `aim_at` for a point and for an entity, an accuracy value for each bot (the spread of the shot), and limits for perception (range, field of view and team filter). `shot_chance` only controls the hesitation to fire, and it does not control accuracy. Passenger aim (N61) covers only a player target in seats 1 to 3. Done on 2026-11-11: `aim_at` (a point or an entity, for a bot on foot or a passenger). The accuracy value and the perception limits are not done. |
| N69 | Test room cases for the new work. Add cases that a person checks in the game for N40 (the block appears, blocks the way, and goes away when the plugin stops), and for N58, N59, N60, N27, N45, N52 and N39. The owner decided on 2026-11-08 that N40 must be tested in the game. Each case follows the style of the existing cases in [test room](plugin-testroom.md). Done on 2026-11-10 in the Workshop test room 0.3.0: N40 (`block_set`), N52, N39, N45, N59, N62 and N63 (account number and street map), `human_remove` and `aim_snap`. Not started: N58 and N60. None of them ran in the original client yet. |
| N70 | A shared command layer, as a library plugin (`role=library`, N38). A command gets one raw string today, so each plugin writes its own parser, player finder, aliases, help and pager (see [plugin coverage](plugin-coverage.md)). The library offers: a target resolver (`id:N`, account number, name), a usage and help registry for each command, name tables as aliases for numbers, and a pager with `/page` and `/last`. `sv.permissions`, `sv.admcmd` and later plugins call it through the public plugin service, and send plain lines when it is absent. One owner for `/page` and `/last`. Not started. |
| N71 | Input lock for a player, in two separate parts. (1) The body: hold the movement axes and the buttons of a human, so the body does not walk, jump or act. (2) The camera: hold the view yaw and pitch, so the player cannot turn. Each part has its own switch and can run alone. A lock ends when the plugin stops, the body dies or the round resets. It replaces the freeze of `sv.admcmd`, which teleports the body back at each tick. Today `override_input` takes bots only. Mark "client effect unknown": the client predicts its own movement and may fight the server. Done on 2026-11-11: `input_lock` (the body part and the view part). The item and menu actions of the packet still run. Client effect unknown. |
| N72 | Time of day, read and write. `SvWorldState.world_clock_raw` is read only and has no unit in the docs. Find the unit and the range in the code, add a documented read (for example hours and minutes), and a write row `world.clock` with a range check. Check what else follows the clock (the light, the traffic, the shop hours) and state it. Mark "client effect unknown" until the original client shows the new light. Done on 2026-11-11: the property `world.clock` (get and set, multiples of 10), with a test in `tests/plugin_game_fixture.c`. Client effect unknown. |
| N73 | Vehicle writes. The object rows cover health, velocity, time to live and damage scale. Add the color, the lock and the key (`key_away`), and what the fields of the object allow for the engine. Read the fields of `Object` in `sr_types.h` first, and add only what the game already uses. Each write has a range. Mark "client effect unknown" for a color change after the object exists. Done on 2026-11-11: the properties `object.color` and `object.key_away`. The object has no lock field, so there is no lock to write. Client effect of a color change is unknown. |
| N74 | Body direction. `teleport` keeps the orientation, and no write turns a body. Add a write for the facing and the view of a human (yaw and pitch), for a player and for a bot. For a player this is the camera, so it works with the camera part of N71. Bound the angles. Mark "client effect unknown". Covered on 2026-11-11 by the view part of `input_lock` for a player, and by `aim_at` and `override_input_partial` for a bot. No separate write: the game copies the view to the body on each tick. |
| N75 | Weapon fire rhythm. `SvItemState.trigger_ticks` counts the ticks that the trigger is down, and the weapon fires when it reaches 2 (6 for the Magnum). The AI of a bot rewrites the buttons at each tick, so a plugin cannot press or release the trigger. Done: the write row `item.trigger_ticks` (0 to 3600). A released trigger sets the count to 0 on each tick, so a write has no effect after the next release. No cooldown row: the game sets the cooldown from `fire_delay` at each shot, and `item_type.fire_delay` already changes it for a type. Client effect unknown for a player weapon. See [bot API additions](game/bot-controls.md). |
| N76 | Partial input override for a bot. `override_input` replaces all four axes, the buttons, the yaw and the pitch for 1 to 3600 updates. A plugin that wants only one part, such as the trigger or the reload, loses the default movement and aim of the bot. Add a mask for the parts to replace (axes, buttons, view), and a mask for the buttons, so the rest stays with the default AI. Keep the old call as the mask of all parts. Done on 2026-11-11: `override_input_partial`. |
| N77 | Aim speed of a bot. The AI turns the aim by 1/16 of the gap on each update and fires only within 0.125 rad, so a bot in a moving car or a bot with a near moving target waits long before it fires. Done: the policy `aim_snap` in `SvGameAi`, on foot and for an armed passenger. The game still decides each shot. N68 asks for more aim helpers. See [bot API additions](game/bot-controls.md). |

Groups of the new rows, in the order of work: A is N28 to N32 (small rows in the same mold), B is N26, N33 and N34 (events and decisions), C is N35 to N38 (the plugin system, done), D is N19, N27, N40 and N41 (large or low priority), E is N39, N42 and N43 (weapons and effects), F is N44 to N48 (names, possession, speech above a head and voice), G is N49 (trains), H is N50 to N55 (gaps found by the audit of 2026-10-30). Group E has a client test: change the name of an item type in the server table and check whether the original client shows it. The name is not in any packet, so the client most likely has its own list.

The game has no explosion, fire or smoke event. The C4 module of the tool uses a damage in a radius, a push and a sound. A visual explosion needs OverRosa.

Plugin configuration (N23) does not exist yet. N14 is different: it gives plugins the settings of the core, such as the player limit.
N23 is the settings of one plugin, such as the price of the knife.

The difference from storage (N18):

| | Configuration (N23) | Storage (N18) |
| --- | --- | --- |
| Who writes | The administrator | The plugin |
| Shape | Keys that the plugin declares, with a type and a range | Any bytes, within a size limit |
| Check | The host checks each value before the plugin sees it | The plugin checks its own data |
| Shown to the user | Yes, in the launcher and by command | No |
| On update | Kept for the keys that remain | Kept whole |

The design rules:

- **Declaration.** The plugin lists its settings in `plugin.ini`: key, type, default, minimum, maximum, label and help text.
  The host must read them without loading the DLL, because the DLL stays unloaded until the administrator approves it.
- **Types.** A flag, an integer, a float, a bounded text, a choice from a list, and a secret. The old script manager had the same idea: each parameter had a label, a type, a default and a range.
- **Setting a value.** The local operator uses `plugins config <id>` to list and `plugins config <id> <key> <value>` to set, as with `plugins enable`.
  The launcher builds a form on its Plugins page from the declaration. It sends the command to a running server on standard input, or writes the file when the server is stopped.
  A player who is an admin may set a key only when the plugin marks it as editable by admins.
- **Check and save.** The host refuses a value outside the declaration and keeps the old value. It saves to a private file in the folder that updates keep.
- **Applying.** A key is live or needs a restart. A live key gives the plugin a change event with the old and the new value.
- **Updates.** A key that stays keeps its value. A new key takes its default. A removed key is dropped. A key that changes type goes back to its default.
- **Secrets.** The host never writes a secret to a log or shows it again after it is saved. The launcher already hides passwords in its log.
- **Tests.** Every type, every error, a restart that keeps the values, and an update that adds, removes and retypes a key.
  Removal proofs for the range check and for the hiding of secrets.

## Changes in 0.8.0

Version 0.8.0 of the tool has 20 modules and a long documentation. This section keeps only what changes this project.

**New modules.** C4 and detonator, the crazy NPC pair and the team rounds. See the module table. Three modules gain a real description:
aggressive bots, clunky ragdolls and critical health. The tool still has no grab module. It keeps only an old setting for it.

**New command.** `/give money ID N`. The chat commands now share one admin check and one rule for target ids:
`/give admin`, `/give money` and `/deactivate` take the public id of the player (the id in the lists of the original server),
and reject an id that matches two records. `/team` takes a team, `/turn` an index in the saved accounts, `/item` an item type,
`/vehicle` a model and `/sound` a sound index. Do not turn these arguments into one kind of id.
A reply to the caller goes to the current living body, and only to the console when there is none.

**Parameters.** Every module now has declared keys with a type, a default and a range. These are the targets of N23:

| Module | Key: default (range) |
| --- | --- |
| Black market | `knife_price`: 125 (0 to 1000000); `bandage_price`: 70; `knife_wait`: 400 ticks (0 to 36000); `bandage_wait`: 200 ticks; `knife_damage`: 25 (0 to 100; 0 turns the attack off); `knife_range`: 1.5 (0.1 to 3); two phone positions (6 floats); `show_id` (flag); `commands_enabled` (flag) |
| C4 | `c4_price`: 500; `c4_wait`: 400; `c4_damage`: 200 (0 to 500); `c4_radius`: 8 (1 to 30); `c4_impulse`: 3 (0 to 5) |
| Crazy NPC pair | `weapon`: 0 (0 to 5); `car_color`: 0 (0 to 5); `car_model`: 5 (0 to 13); `shooter_health`, `driver_health`, `vehicle_health`: 10000 (1 to 1000000); `vehicle_infinite` and `wheels` (flags, default 1); `shooter_name`, `driver_name` (text, 20 bytes) |
| Team rounds | `round_seconds`: 300 (10 to 86400); `pause_seconds`: 5 (1 to 60); `early_finish` (flag, default 1) |
| Punch | `damage`: 100 (0 to 10000); `head_multiplier`: 5 (0 to 100); `knockback`: 8.5 (0 to 50); `threshold`: 3 (0 to 100); `cooldown`: 70 ticks (0 to 10000) |
| Safe spawn area | Four limits of a rectangle on the ground plane, default 608 to 784 and 1428 to 2048. The tool names the axes in a confusing way. Use clear names and the real axes of this server. |
| Spawn protection fix | `velocity_squared`: 0.02 (0 to 100). The unit is the squared internal speed of the vehicle. |
| Faster truck | `speed` and `power`: 2 (1 to 4) |
| Truck in the dealership | `price`: 12000 (0 to 1000000) |
| Spawn at the dealership | Three floats, default 485, 26 and 655, in the order of the tool. Check the order against the vectors of this server before use. |
| Starting money | `money`: 2500 (0 to 100000000) |
| Maximum players | `value`: 32 (1 to 32). Lowering it does not kick anyone. |
| NPC count | `value`: 32 (0 to 128). It does not add or remove bots that exist. |

**Rules between modules.**

- A module can declare a conflict with other modules (the team rounds conflict with the two death message modules). The loader has no such key yet. Add `conflicts=` to `plugin.ini` with N23.
- The C4 module finds its sales through the phones of the black market. This is a contract between two plugins. It needs a plugin-owned API (see the decisions in [the plan](plano.md)) and a declared dependency.
- A module can refuse to start when a condition fails. The rounds module returns an error when the gametype is not Rounds. The host needs a clean "refused to start" result with a reason.
- Disabling a module that made phones, kits or pairs removes what it made. This is the rule for plugin-owned resources.

**Facts about the death modules.** The instant death mode starts at health -5 or less and moves the despawn timer forward. The general messages say "an NPC" for a bot killer.
A bot with its own name can do better. Check what the read API gives for a bot before work.
The messages of the crazy NPC pair and the general death messages repeat each other in the tool.
The official plugin must have one source of death messages.

**About `/deactivate`.** The tool toggles the active record of a human. It gives the rule for the target and no effect on the world.
A human record that is switched off while its bones, bonds, items and clients still know it can leave the world in a state that the game does not expect.
No general operation serves it, so the plan is to refuse it and to list it as "no support" at the close of S3. The user decides.

**Not in the catalog.** `/nick` has no code in the tool. The settings file keeps a grab module that is not installed. A cheat table in the package shows the older hand pose and punch code.
The tool stays outside this repository, and none of its binaries or code is used here.

## The original client and OverRosa

The rule stays: each operation must work with the original client. The server may change its own protocol, but the original client does not read new events.
When a change cannot reach that client, the operation does one of two things:

1. It gives the same result by other means that the client understands. Example: to change the color of a vehicle, remove it and make a new one with the new color.
2. It goes in the list below. It waits for OverRosa, the future modified client.

**List for OverRosa.** These need a new event or a new field, and the original client cannot show them:

- Change the color of a vehicle in place. The color goes to the clients only in the creation event.
- Change the type of a vehicle or of an item in place. The type goes only in the creation event.
- A new model, a new sound or a new menu.
- The name of an item type, most likely. The server sends only the type number, so the client has its own names. A test with the original client settles it (see N43).
- A visual explosion, fire or smoke. The game has no such event.

**Unknown until a test with the original client.** The game values that the client may simulate with its own tables, such as engine force or speed range.
The server changes them, but the client may then differ from the server. Mark such a parameter "client effect unknown" in its documentation.

**Plan for the later work.** The server marks each connection as OverRosa or original, and sends a new event only to the first kind.
The packet writer already sends a different packet to each connection (the private message). The protocol may not have a field to tell the two clients apart. Check this first.

## Order

1. N1 to N4, N6 to N8, N10 and N11 first. They serve the chat commands, which one official administration plugin can own.
2. N16 and N12 next. They serve the death, punch, knife and eject modules.
3. N9 and N24 with them. The eject, safe spawn, spawn protection and punch modules need them.
4. N14, N15 and N17 last. They need changes to core data that the API does not reach yet.
5. N26 to N29 with the crazy NPC pair and the team rounds, after N20.

Each operation follows the rules in [system coverage](plugin-api-map.md#required-catalog-before-implementation):
a documented phase, bounds, failure result and a focused test that fails when the check is removed.
The original client limits stay explicit. A new model, sound or menu needs OverRosa.

## Bans

What the server has today (from the code):

- **IP list.** `serverrecv` compares the sender address of every packet with `sr_kickedip` (up to 41,368 addresses, one scan for each packet). A match drops the packet with no reply. The list is in memory only. `/clearkick` empties it and a restart loses it. The `kick` method adds to it.
- **Account ban.** `Account.ban_time` above 0 drops the player at the join and during play. The count falls by 1 each 36,000 ticks of the world clock.
- **No persistence.** `saveaccountsserver` writes the id, name, money, play time and three look values. It does not write `ban_time`. A restart ends every ban. (Read from the code, not run.)

Ideas for an official ban plugin. They need plugin storage (N18) and a decision event at the join (N16):

1. Bans kept on disk, with reason, author, date and end date.
2. Ranges of addresses (a base address and a mask) instead of single addresses. The core list scans one by one, so the plugin keeps its own sorted table and the core only gets a yes or no.
3. A strike system with longer bans each time, and a list of exceptions.
4. A limit of join attempts for each address and each minute, with an automatic short ban.
5. Link an address with an account to find a banned player on a new account. This stores an address history, which is personal data.
6. A shadow mute: the muted player sees their own chat and nobody else does. The per-connection filter and the own-text rule of the packet writer already allow this.

Limits:

- The addresses are IPv4 only (32 bits).
- Players behind one shared address (NAT) can be banned together. A VPN avoids an address ban.
- The protocol has no hardware id. A client token would need OverRosa.
- Stored addresses are personal data. Keep them in the private plugin folder and show them masked in admin output. Never put them in logs that leave the machine or in the repository.
