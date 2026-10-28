# Plugin API: coverage, needs and missing features

Date: 2026-10-28. This page counts what the server offers to plugins today, lists the open needs of the catalog, and lists features that the catalog does not name.
It follows the system table of [system coverage](plugin-api-map.md). The proof for each item is in the test logs and in the page of its area. Nothing here was played with a client except where a line says so.

## What exists

| Part | Count | Where |
| --- | --- | --- |
| Capabilities | 12 | 1 read, 2 write, 3 AI, 4 economy, 5 missions, 6 settings, 7 storage, 8 engine, 9 shop, 10 property table, 11 plugin settings, 12 plugin services |
| Read methods | 13 | `entities`, `player`, `human`, `item`, `object`, `account`, `bone`, `inventory`, `world`, `line`, `aim`, `headline`, `within` |
| Write methods | 24 | messages, kick, money, teleport, looks, 5 item, 2 object, 2 bot, vehicle exit and enter, sound, headline, name, speech, shot and hit visuals, `disown` |
| Property rows | 34 | players, humans, items, objects, accounts, 27 item types, 4 bullet types, the world, 2 phone rows |
| Area tables | 7 | AI 11 methods, economy 4, missions 3, settings 3, storage 3, engine 3, shop 4 |
| Events | 18 bits | tick, stop, lost, spawn, death, leave, chat, join, phone digit, phone call, damage, config, round, vehicle, shot, object gone, item, setting |
| Host rules | 5 | conflicts, refusal reason, plugin APIs, library plugins, and the cleanup of what a plugin made |

## Coverage by system

"Yes" means that a method or a row exists and a test checks it. "Part" means that some of the system is open.

| System | State | What is open |
| --- | --- | --- |
| Accounts | Part | Read: name, money, play time, looks. Write: money, ban time. Open: a save event, rename of an account. |
| Players and teams | Part | Read, team, admin, god mode, kick, join and leave decisions. Open: the connection (IP, port, idle time, spectate), player input for a human, the roster and limits of a team. |
| Economy | Yes | Companies, prices, shares, stocks. Open: a decision before a purchase, taxes. |
| Missions and rounds | Part | Slots, start, cancel, completion guard, world state and timers. Open: mission events, reset of the round, change of the game mode while the server runs. |
| NPC decisions | Yes | Threats, destination, waypoints, target, input, perception, aim, shot chance, street route, seated target. Open: a query of the road graph (N27). |
| Humans | Part | Health, bleeding, stun, protection, bones, teleport, looks, name, speech, collapse threshold. Open: a decision on spawn and respawn, bone masses (N19), delete the body of a real player (N8). |
| Items and inventory | Yes | Create, remove, equip, attach, detach, time to live, ammunition, item types. Open: decisions for pick up, drop and use. |
| Weapons and bullets | Part | Type rows, shot event, tracer and hit visuals. Open: one weapon with its own values (the rows are for the whole type). No explosion, fire or smoke event exists in the game. |
| Objects and vehicles | Part | Create, remove, health, teleport, impulse, seat, engine power, damage scale. Open: seat list, wheel state, helicopter and train control, throttle and steering for a plugin driver. |
| Physics | No | Bone velocity and object velocity only. Open: masses, gravity, particles, bonds. |
| Collision | Part | Line, aim and sphere (`within`). Open: material at a hit, block cell (N39). |
| Block areas | No | Nothing yet (N40). |
| Buildings and city | Part | Shop catalog read and replace, newspaper. Open: floorplans, safe zones, elevators, doors, a read of the building list. |
| Level construction | No | Nothing at the reset boundary yet. |
| Network and communication | Part | Chat with replace, commands, messages, sounds, join. Open: voice (N47, N48), hiding an entity from some players, a list of the connections. |
| System services | Part | Settings, storage, logs, metrics in the list. Open: timers (N41), a way to run work on a worker thread, a way to hand its result to the server thread. |
| Math | No | Random state of a plugin (N41). |

## Open needs of the catalog

| Id | Need | Notes |
| --- | --- | --- |
| N8 | Delete the body of a real player | Kill with `human.health` meanwhile. |
| N10, N39 | Material at a hit, block cell | Same query family as `line`. |
| N19 | Bone masses | A row `human.bone_mass`. The bonds read the mass of each bone live, so a write works at once. |
| N26 | Request to reset the round | The rows for state and timers exist. |
| N27 | Road graph query | The data is `g_city.intersections` and `g_city.roads`. |
| N29 | Human health above 100 | The packet clamps the sent health to 117. |
| N33 | Account save event | The other events of N33 exist. |
| N40 | Block edits | Mark "client effect unknown". |
| N41 | Timers and a random generator for each plugin | See the first item of the list of missing features. |
| N45 | Possess a bot | Links on both sides. |
| N47, N48 | Voice frames and voice routing | The ring of voice frames and the 8 slots exist in the core. |
| N4 | Colors | Decided as "no support": the colors never leave the server. |
| `/deactivate` | No support | Both wait for the acceptance of the owner to close S3. |

Seams from the second editor (see [core seams](plugin-seams.md)): all hooks are wired except the notice and decision events of missions. The settings and the engine keep their typed tables, and they have no rows in the property table. The hooks run in a real server for 30 seconds only. Nobody played them.

## Features that the catalog does not name

Ranked by how much a plugin author loses without it.

### Platform

1. **A bridge from a worker thread to the server thread.** Every host call from another thread returns `SV_STATE`. A plugin that talks to the web (an online map, a remote admin port, a chat bridge) needs a queue: the worker posts a request, and the host runs it at the next safe point with the full API. Without it, each plugin builds its own unsafe version.
2. **An SDK package.** The server package carries no public header, no sample project and no build file for a plugin. A third party must read the source tree. Ship `sv_*.h`, a template with a build script, and the pages of the API.
3. **A command layer.** A command gets one raw string and one of three levels. A plugin needs help text, an argument parser, a player finder by name or number, aliases and named ranks. Each plugin now writes its own.
4. **Decisions for more actions.** Vehicle enter and exit, item pick up, drop and use, spawn and respawn, team change and a menu purchase are notices or nothing. A decision lets a plugin lock a car, protect a base or set the spawn point. The kind-2 mechanism already exists.
5. **Data tags on an entity.** Done: the storage scope `SV_STORAGE_ENTITY` keeps a value for a human, an item or a vehicle in memory, and the value goes when the thing goes. A tag that must survive a round has no place yet, because references do not survive a round.
6. **A declared permission list in the manifest.** A native plugin has the full process. The launcher cannot say "this plugin writes money" before approval. A list of the capabilities that the plugin uses would let the approval screen show it, and the host could refuse the rest.
7. **Hot replacement of a live plugin.** A change of a DLL needs a restart now.
8. **A publish and subscribe channel between plugins.** The plugin API gives calls, not events. A moderation plugin cannot tell a chat plugin that it muted a player.
9. **Per-plugin log level and file, and a metrics call.** The cost of callbacks shows only in the list.
10. **The Lua and WASM hosts (S4).** They are the answer for third-party code that needs containment.

### Reads

1. The connection of a player: address, port, idle ticks, spectate, bandwidth. An admin plugin needs the address to ban.
2. The seats and the occupants of a vehicle, the wheels, the rotor.
3. The building list: type, bounds, shop, safe zone. The shop and the NPC plugins need it.
4. The kill and death count of a round. The core keeps none, so each plugin keeps its own from the events.

### Writes and decisions

1. Round control: reset, force the end, select the next level seed, change the game mode.
2. Vehicle control for a plugin driver: throttle, steering, gear, brake.
3. Doors, elevators and safe zones.
4. The input of a human for a cutscene or a test bot.
5. Hide an entity from some players. It can also fight wall hacks. Test it with the original client first.
6. A kill feed and a score line that differ by player. The clients read the usual events, so this needs no client change.

## Freeze of the contract

The rules of the ABI are in [plugin ABI rules](plugin-abi.md). A request for a table version takes a range, and the event mask has 64 bits. The rest of the freeze work (a snapshot test of the sizes, a header audit, one gate for actions that wait for a plugin, and a measure of the cost of a tick) is in [the freeze brief](api-freeze-brief.md).

## Quality gaps

- No test ran the new seams with a client. The server started and ran 30 seconds. The groups A, E and F, the chat replace, the notice events and tests 19 to 29 of the test room were not played.
- No soak test. A run of an hour with bots and a plugin that uses every capability would find slow leaks and pool exhaustion.
- No stress test of the event queue under 100 players and 256 bots.
- The behavior of the original client is unknown for renamed names, bullet speed rows, fire delay rows and the collapse threshold above 0.
- The pages in `docs/plugin-api-map.md` and `docs/plano.md` repeat counts. Update them with each new method.
