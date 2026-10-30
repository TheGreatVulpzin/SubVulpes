# Plugin API: coverage, needs and missing features

Date: 2026-10-30. This page counts what the server offers to plugins today, lists the open needs of the catalog, and lists features that the catalog does not name.
It follows the system table of [system coverage](plugin-api-map.md). The proof for each item is in the test logs and in the page of its area. Nothing here was played with a client except where a line says so.

## What exists

| Part | Count | Where |
| --- | --- | --- |
| Capabilities | 13 | 1 read, 2 write, 3 AI, 4 economy, 5 missions, 6 settings, 7 storage, 8 engine, 9 shop, 10 property table, 11 plugin settings, 12 plugin services; network is a permission at bit 12 |
| Read methods | 14 | `entities`, `player`, `human`, `item`, `object`, `account`, `bone`, `inventory`, `world`, `line`, `aim`, `headline`, `within`, `connection` |
| Write methods | 25 | messages, kick, money, teleport, looks, 5 item, 2 object, 2 bot, vehicle exit and enter, sound, headline, name, speech, shot and hit visuals, `disown`, `world_reset` |
| Property rows | 37 | players, humans, items, objects, accounts, 27 item types, 4 bullet types, the world, 2 phone rows |
| Area tables | 7 | AI 11 methods, economy 4, missions 3, settings 3, storage 3, engine 3, shop 4 |
| Events | 20 bits | tick, stop, lost, spawn, death, leave, chat, join, phone digit, phone call, damage, config, round, vehicle, shot, object gone, item, setting, account save, timer |
| Host methods | 8 | `log`, `status`, `command`, `subscribe`, `get_api`, `after`, `every`, `random` |
| Host rules | 5 | conflicts, refusal reason, plugin APIs, library plugins, and the cleanup of what a plugin made |

## Coverage by system

"Yes" means that a method or a row exists and a test checks it. "Part" means that some of the system is open.

| System | State | What is open |
| --- | --- | --- |
| Accounts | Part | Read: name, money, play time, looks. Write: money, ban time. Save notices cover disk and disconnect memory updates. Open: rename of an account. |
| Players and teams | Part | Read, team, admin, god mode, kick, join and leave decisions, connection snapshots with address masking. Open: player input for a human, the roster and limits of a team. |
| Economy | Yes | Companies, prices, shares, stocks. Open: a decision before a purchase, taxes. |
| Missions and rounds | Part | Slots, start, cancel, completion guard, world state, timers and round reset. Open: mission events, change of the game mode while the server runs. |
| NPC decisions | Yes | Threats, destination, waypoints, target, input, perception, aim, shot chance, street route, seated target. Open: a query of the road graph (N27). |
| Humans | Part | Health, bleeding, stun, protection, bones, bone mass, teleport, looks, name, speech, collapse threshold. Open: a decision on spawn and respawn, delete the body of a real player (N8). |
| Items and inventory | Yes | Create, remove, equip, attach, detach, time to live, ammunition, item types. Open: decisions for pick up, drop and use. |
| Weapons and bullets | Part | Type rows, shot event, tracer and hit visuals. Open: one weapon with its own values (the rows are for the whole type). No explosion, fire or smoke event exists in the game. |
| Objects and vehicles | Part | Create, remove, health, teleport, impulse, seat, engine power, damage scale. Open: seat list, wheel state, helicopter and train control, throttle and steering for a plugin driver. |
| Physics | Part | Bone mass, bone velocity and object velocity. Open: gravity, particles, bonds. |
| Collision | Part | Line, aim and sphere (`within`). Open: material at a hit, block cell (N39). |
| Block areas | No | Nothing yet (N40). |
| Buildings and city | Part | Shop catalog read and replace, newspaper. Open: floorplans, safe zones, elevators, doors, a read of the building list. |
| Level construction | No | Nothing at the reset boundary yet. |
| Network and communication | Part | Chat with replace, commands, messages, sounds, join. Open: voice (N47, N48), hiding an entity from some players, a list of the connections. |
| System services | Part | Settings, storage, logs, metrics in the list, tick timers. Open: a way to run work on a worker thread, a way to hand its result to the server thread. |
| Math | Part | Plugin random has independent state (N41). |

## Open needs of the catalog

| Id | Need | Notes |
| --- | --- | --- |
| N8 | Delete the body of a real player | Kill with `human.health` meanwhile. |
| N10, N39 | Material at a hit, block cell | Same query family as `line`. |
| N19 | Bone masses, done | `human.bone_mass`: 15 float slots, GET, SET and ADD, range 0.5 to 500. |
| N26 | Round reset, done | `world_reset` defers and merges reset requests. State and timer rows also exist. |
| N27 | Road graph query | The data is `g_city.intersections` and `g_city.roads`. |
| N29 | Human health above 100 | The packet clamps the sent health to 117. |
| N33 | Account save event, done | `SV_EVENT_ACCOUNT_SAVE` follows a bulk file save or the disconnect update in memory. |
| N40 | Block edits | Mark "client effect unknown". |
| N41 | Timers and plugin random, done | `SvHost.after`, `every` and `random` use ticks and an independent state. |
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

1. Connection snapshots cover address, port, idle ticks and spectate. Bandwidth remains unsupported.
2. The seats and the occupants of a vehicle, the wheels, the rotor.
3. The building list: type, bounds, shop, safe zone. The shop and the NPC plugins need it.
4. The kill and death count of a round. The core keeps none, so each plugin keeps its own from the events.

### Writes and decisions

1. Round control still lacks a forced end, selection of the next level seed and a game mode change. Reset requests exist.
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

## Bone mass (R4)

`human.bone_mass` exposes 15 float slots with GET, SET and ADD. The range is 0.5 to 500, inclusive.
Writes change the live bone mass. Invalid slots, nonfinite values and results outside the range fail without a write.
The game API fixture checks all slots, both endpoints, addition and failed writes. No client playtest ran.

Removal proofs for the slot bound and both range checks fail the game API test. The source is restored before final checks.

## Connection read (R1)

`SvGameRead.connection` copies an IPv4 address, port, idle ticks and spectate value into `SvConnectionState`.
The IP bytes use dotted address order. The port uses host order. The spectate value keeps the game encoding, including -1.
Bots and players without a connection return `SV_NOT_FOUND`. Invalid connection counts return `SV_STATE`. Failed calls leave the output unchanged.
Plugins must restrict address disclosure to administrators. The fixture checks copies, bounds, bots and missing connections. No client playtest ran.

Removal proofs for the snapshot size and connection count fail the game API test. The source is restored before final checks.

## Account save notice (R6)

`SV_EVENT_ACCOUNT_SAVE` is `SV_EVENT_EXT(0)`. Subscribe with `SV_EVENT_EXT_BIT(0)` and read `SvAccountSaveEvent.account` and its final field, `reason`.
Reason 0 (`SV_ACCOUNT_SAVE_DISK`) follows the bulk disk save after every write and the file close succeed.
The bulk save posts one notice for all accounts, with `account = 0`. Failed saves post no notice. Only reason 0 means the data reached disk.
A plugin that needs a disk write must use reason 0.
Reason 1 (`SV_ACCOUNT_SAVE_DISCONNECT`) follows the existing money update at disconnect in `deleteplayer`. It saves to memory only.
There is no single-account disk save in this core. A disconnect notice does not promise disk persistence.
Notices enter the normal queue and run at a safe point. An account reference can be stale at delivery.
The fixture checks the closed file, failed writes, failed opens and the disconnect update. No client playtest ran.
No short test room case exists because the write API does not request an account save.

An account read at delivery returns the current state, which can include later changes.

The disk notice covers all accounts, including an empty account list. Disconnect notices carry one account reference.
`os_fclose` forwards `fclose` and returns 0 on success. The `!= 0` check rejects a failed close.
The flood test saves 2,000 accounts and checks one disk notice, with an earlier queued event intact and no loss notice.

The R1 network test sends a real UDP join through `serverrecv` and checks the stored endpoint and public connection snapshot.

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
The test room declares `network` and sends the address directly to private chat, without its usual chat log.
The capability does not isolate native code. A native plugin can read server memory, including the salt.
Tests cover both permissions, stable identifiers, distinct addresses, a shared /24, salt reuse and the capability removal proof.

## Round reset request (R5)

`SvGameWrite.world_reset` requests one reset at the start of the next server tick. It requires the normal write context and a ready world.
The main loop consumes the flag before control requests, plugin callbacks and simulation. Two requests before that point cause one call to `resetgame()`.
A normal core reset or host stop also clears the flag. The fixture checks deferred requests, coalescing, context rejection and flag cleanup.
The focused test exercises the reset hook and flag consumption. It does not run map creation or a client playtest.
Test room case 67 requests a reset. The client playtest remains open.

Removal of the write context check fails the game API test. The source is restored before final checks.

## Tick timers and plugin random (R9)

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

Removal proofs cover timer capacity, delay bounds and owner checks. The source is restored before final checks.
