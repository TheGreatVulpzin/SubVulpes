# Plugin API: game system coverage

Status: approved implementation scope, 2026-10-26. Runtime coverage needs separate evidence.
See [the milestone plan](plano.md) and [the plugin contract](plugins.md).
It covers the 13 system pages in `docs/game/`, their index, internal headers, and selected implementation paths.
It does not prove every field or operation safe for plugins or compatible with the original client.

## Scope rule

Expose every feasible server capability through the common API over time.
The original client remains usable wherever its protocol and behavior support the feature.
Use OverRosa when a feature requires client support that the original client lacks.
An absent API operation is work to plan, rather than a permanent restriction on mods.

The inventory below describes target capabilities. It does not claim complete implementation.
The implemented [read capability](plugin-game-api.md) covers copied entity, inventory, bone and world snapshots.
The [write capability](plugin-game-api.md#write-capability-version-1) has twenty-one methods: messages, kick, money, god mode, health, bleeding, stun, teleport, admin flag, team, looks, five item operations, two object operations and two bot operations.
Most other mutations, decisions that plugins can replace and owned game resources remain unimplemented.
Internal C functions establish implementation seams. They are not a safe or stable external contract by themselves.

## Coverage by system

| System and local documentation | Candidate reads and changes | Current implementation seams | Compatibility and contract work |
| --- | --- | --- | --- |
| [Accounts](game/accounts.md) | Account lookup, names, appearance, money, play time, bans, save events | `src/server/accounts.c`, `src/game/account_db.c`, `player.h` | Separate persistent account identity from a player session. Define which values actually persist. Keep private account data out of public snapshots. |
| [Players and teams](game/logic.md#players-and-accounts) | Enumerate players, teams, human links, input, inventory, team managers, join and leave decisions | `player.c`, `logic.c`, `player.h`, `logic.h` | Changes need consistent links and network events. Handle disconnects, resets and slot reuse. Define control ownership before replacing player input. |
| [Economy](game/logic.md#economy) | Money, stocks, prices, shops, taxes, stakes, purchase rules and rewards | `economy.c`, `economy.h`, `logic.c` | Keep account, player and team balances consistent. Define integer overflow checks and when default policy can be replaced. |
| [Missions and rounds](game/logic.md#missions) | Objectives, participants, stages, rewards, expiry, round timers, world rules and reset behavior | `missions.c`, `scenario.c`, `logic_sim.c`, `game_setup.c` | New mission logic can use plugin state and existing entities. Original menus and HUD still impose display limits. Existing mission constructors need capacity checks before exposure. |
| [NPC decisions](game/logic.md#bot-controls) | Different behavior per NPC, targets, threat queries, waypoints, driving, aim and input | `player_ai.c`, `ai_car.c`, `scenario.c`, `logic.h`, `ai_car.h` | A plugin needs a declared way to replace or combine default AI. Writing input alone can lose to a later engine update. Bound perception queries and work per tick. |
| [Humans](game/humans.md) | Spawn, removal, health, bleeding, timers, clothes, movement, bones, seats and held items | `human_body.c`, `humans.c`, `human_pose.c`, `human_arms.c`, `humans.h` | A teleport must update the body consistently. Network health has a fixed representation. New body layouts and appearances require corresponding client support. |
| [Items and inventory](game/items.md) | Spawn, removal, lifetime, containers, slots, cash, keys, phones, doors and use behavior | `items.c`, `human_items.c`, `item_logic.c`, `items.h` | Parent and child links must remain consistent. Use validated operations that emit updates. Do not expose unrestricted writes to inventory arrays or active flags. |
| [Weapons and bullets](game/items.md#bullets) | Fire behavior, delay, ammunition, spread, bullet speed, hits, damage, recoil and custom effects | `item_types.c`, `item_logic.c`, `bullets.c`, `bullet_sim.c` | Reuse a known client representation where possible. Per-instance weapon variants need new host support because current firing reads shared type data. New models, sounds or unknown types need client support. |
| [Objects and vehicles](game/objects.md) | Spawn, removal, health, motion, forces, seats, engine behavior, wheels, helicopters and trains | `object_create.c`, `objects.c`, `vehicles.c`, `wheels.c`, `object_types.h` | Object position and frame derive from particles. A setter must change the underlying state consistently. New server type geometry is not automatically sent to the client. |
| [Physics](game/physics.md) | Particle and bond queries, forces, velocities, mass, gravity, constraints and contact decisions | `particles.c`, `bonds.c`, `solver.c`, `particles.h` | Packed pools move surviving records on deletion. Handles need stable identity plus relocation tracking. Expose controlled operations around solver phases. Default simulation remains in the core. |
| [Collision](game/collision.md) | Rays, spheres, wheel circles, hit results, surface data, zones and collision decisions | `collide.c`, `collide_area.c`, `rigid.c`, `collide.h` | Copy results out of shared scratch state. Specify query scope and cost. Replacing collision responses needs dedicated events, not arbitrary writes during the solver. |
| [Block areas](game/area.md) | Cells, materials, colors, interior data, block placement, destruction and region queries | `area.c`, `area.h`, block and interior builders | The server can change its world state. That alone does not prove original clients receive a matching live map. General geometry replication needs a separate protocol and client check. |
| [Buildings and city objects](game/buildings.md) | Floorplans, placement, special rooms, safe zones, elevators, shops and newspaper text | `buildings.h`, floorplan and city-object implementations | Validate dimensions, names, counts and room links. Some builders reuse global scratch state. Match client data for geometry and existing object types. |
| [Level construction](game/level.md) | Seed, terrain, track, roads, intersections, tiles, building lists and generation decisions | `level.c`, `city.c`, `mesh.c`, `level.h`, `city.h` | Start with load or reset boundaries when needed. A server-only terrain change can disagree with the client. Do not promise arbitrary map streaming from the current protocol. |
| [Network and communication](game/network.md) | Join and leave events, chat, commands, voice routing, messages, known sounds, replication priorities and server queries | `server.c`, `commands.c`, `events.c`, `netpack.c`, `server.h`, `events.h` | Keep packet parsing and unconditional bounds checks in the core. Separate policy hooks from representation checks. Client extensions require explicit capability negotiation. |
| [System services](game/system.md) | Settings, lifecycle, timers, metrics, tasks, logs and persistence services | `main_loop.c`, `options.c`, common services and platform code | The current loop supplies callback seams. The native manager exists. Plugin storage and bounded async work still need implementation. Client graphics settings do not affect the dedicated server. |
| [Math](game/math.md) | Vector, rotation, geometry and random helpers | `vectors.c`, `random.c`, `intersect.c` | Preserve engine numeric behavior. Define random-state ownership so plugin randomness does not silently change the engine sequence. Returned values must not expose shared scratch pointers. |

An online map uses authorized entity snapshots and an external service.
The read API supplies entity positions. A bounded external network service remains future work.
Access to positions does not require OverRosa. A new map display inside the game requires separate client support.

## Source checks that change the API design

1. **Stable handles need relocation support.** `particletimetolive` moves the last particle into a deleted slot and updates internal references.
   Generation checks alone do not preserve the identity of a moved survivor.
   See `src/game/particles.c:95` and the packed-pool description in `docs/game/physics.md`.
2. **Derived values need real operations.** Objects rebuild their frame from particles in `src/game/objects.c:174`.
   An object teleport cannot be a single write to `Object.pos`.
   Inventory changes also need matching parent links and events.
3. **Core codecs enforce wire limits (C3/C4).** Commit `bba5b84` implements these guards in `src/net/netpack.c`.
   Both object packet writers select the first 255 eligible items and preserve the existing order.
   Later item identifiers can receive no updates while 255 earlier identifiers remain eligible.
   Human health plus ten uses a wire value from 0 through 127. Bounds checks precede addition and leave server health unchanged.
   The real [codec tests](../tests/codec_bounds.c) passed for both writers, including integer extremes, count boundaries, selection order and trailing payload.
   See [network behavior and limits](game/network.md#representation-limits) and [S3 evidence](s3.md).
4. **Some internal paths lack capacity checks.** `src/game/item_physics.c:171` appends a contact bond without a local capacity check.
   `scenario_getnextmission` in `src/game/logic.h` returns slot 16 when all 16 slots are active.
   API validation must cover resulting engine work as well as the immediate call.
5. **Type numbers are not type definitions.** Object creation events send an object index, type and color in `src/net/events.c:45`.
   Object state packets send motion and wheel data, not a complete new type definition.
   Spare internal type slots therefore do not prove original-client support for new shapes.

These checks justify an early host-safety review before enabling broad game mutations.
They do not constitute a full crash audit or a live client compatibility test.

## Documentation status

Account and physics pages describe the native implementation. The system page describes controlled shutdown.
The index includes all 13 system pages. Historical upstream harness references remain historical evidence.
They do not identify tools available in this repository.
Runtime acceptance evidence belongs in the milestone report, rather than in this candidate catalog.

## RosaServer comparison

RosaServer documents object fields, methods and explicit network updates in its [type reference](https://github.com/RosaServer/RosaServerCore/blob/master/.meta/template/types.lua).
Its [global reference](https://github.com/RosaServer/RosaServerCore/blob/master/.meta/template/globals.lua) separates collection queries from creation operations.
Its [hook reference](https://github.com/RosaServer/RosaServer/wiki/Hooks) distinguishes replaceable behavior from notification events.
These patterns help organize this API.
Their field names, capacities and supported features are not proof of behavior in our 0.24d base.

## Required catalog before implementation

For each public operation, record:

- Its system, exact meaning, parameters, result, scope and API capability version.
- Whether it reads, changes, creates, removes, observes or replaces behavior.
- Its execution phase, control ownership, network updates and original-client requirements.
- Its handle rules, bounds, resource cost, failure behavior and required permissions.
- Its state lifetime, plugin-disable behavior, restart requirement and focused acceptance test.

Keep unknown fields, unreviewed paths and untested compatibility explicit.
Do not export every internal struct member merely because it has a name.
Do not use that rule to exclude a feasible mod feature permanently.

## Effect on the delivery plan

Follow the [milestone order](plano.md):

1. Complete the existing framework, status proof and framework tests. The status DLL and test fixtures remain framework proof.
2. Complete the common API across every mapped system, with focused tests.
   Cover reads, mutations, decisions that plugins can replace or cancel, and resources owned by plugins.
   Record implemented coverage, unsupported operations and compatibility limits before production plugin work starts.
3. Only after this API proof, build production gameplay and administration plugins.
   Then demonstrate two NPC behaviors, a custom mission and a weapon effect that reuses an existing representation.
   Use an online map snapshot as the read and async-service example.

Each example needs its own acceptance conditions and client checks where applicable.
These examples exercise the API. They do not define its final ceiling or prove coverage of every system.
The C3/C4 guards do not close the remaining safety gaps, the mapped API or S3.

The common contract remains the base for native, Lua and WASM plugins.
Live activation is conditional on module support. Installation, native replacement and world changes can require a restart.
The initial review ran no gameplay test. Later implementation must record its own build and runtime evidence.

## Sending to some clients only

The server writes the event list and the object packet again for each connection. It can write different bytes for each one.
The original client reads the usual formats, so these uses need no client change: a private message, a sound for one player, team chat,
text and names that differ by observer, and fewer updates for far or spectating clients.
A private message exists (`message_to`). The core keeps the packet writer and the memory checks. A plugin gives only the rule: who gets what.

Hiding an entity from some players can leave the client with a world that differs from the server. It can also work as a defense against wallhacks.
Do not call it done until the original client runs it.
A new packet type, menu, HUD element or model still needs OverRosa.

## Planned operations for AI, economy and missions

The [legacy catalog](legacy-modules.md) (N1 to N19) has none of these. They come from the user's ideas, such as a boss bot that hunts the employees of a company.
Status on 2026-10-27: planned, with no code. The facts below come from `docs/game/logic.md`.

How the game works today:

- **Missions.** Each team has 16 slots. A mission has a kind (0 to 6: guards, vehicle chase, three-way exchange, transactions), an expiry on the world clock and a reward.
  `createscenario_world` picks the kind and the numbers. Completion pays the reward, moves stock prices and clears the linked missions.
- **Economy.** Each company has a share count, a price, an opening price and a mission stake. Prices have a floor of 10 and split at 500 or more.
  The death tax takes 5 percent of the money above 10,000.
- **Bots.** `playerai` follows waypoints on foot or drives. Its target comes from a list of 16 threats. A chase bot uses a destination and waypoints.

| Id | Operation or event | What it allows |
| --- | --- | --- |
| N20 | **AI control.** Read the threats of a bot. Set its destination, waypoints and target. Override its input (movement and buttons) for some ticks, with a way to give control back. | A bot that hunts chosen players. Two different NPC behaviors. |
| N21 | **Economy.** Read the companies (shares, price, stake). Change a price or a share count under the split rules. Add or remove stocks of a player. | Rewards and penalties in stocks. A market event. |
| N22 | **Missions.** Read the slots of a team (kind, expiry, reward). Start or cancel a mission with an existing kind. Events for start, completion and expiry, with a decision to replace the reward. A decision before the game picks a scenario (see below). | Change the rules of the existing missions. A mission of your own. |

There is no data pool of missions to add to. `createscenario_world` (`missions.c`) is code. It runs when a team has fewer than one mission
(or fewer than two at some clock values). It picks by random numbers between three builders: a three-way exchange, a transaction and an acquisition (guards or a chase).
So "add a mission to the pool" means a decision event at that point. The plugin may leave the choice to the game, change the odds, forbid a builder,
or start its own scenario instead. A scenario of the plugin fills mission slots with an existing kind for the display, and the plugin decides when it ends.
Each team has 16 slots. `scenario_getnextmission` returns 16 when all are in use, so the API must check that case itself.

About the text of a mission. The mission event (type 12) carries numbers only: the kind, the team, the ids, the expiry and the reward. It has no text.
So the server cannot change the words that the client shows for a mission. The client most likely builds them from the numbers. This is read from the server code, and the client was not checked.
The server does send text in these places, and a plugin can change them:

- Server messages (kind 0, done in `message`) and admin messages (kind 4).
- Headlines (kind 3): two lines of up to 63 bytes for each headline index. The API has no method for them yet.
- Newspapers: the text comes from `readme.txt`, at most 1,013 lines, read at the load. The API has no method for it.
- Player names, at most 31 bytes.

A mission of a plugin can announce itself with a message or a headline, while the slots use an existing kind for the display.

The original client shows a mission only through the existing kinds, the phone menu and the news. A new kind of display needs OverRosa.
A plugin can reuse the display of a kind and change the rules behind it.

What exists now for a boss bot that hunts the employees:

- **Ready:** `npc_create` for the bot, `item_create` and `item_equip` for a heavy gun, `set_godmode` (a bot is a player, so the flag works), `add_money` for the reward, `message` for announcements, the tick event, and the read API for the positions and teams of the players.
- **Missing:** a way to make the bot walk and aim at a chosen player (N20). Without it a plugin can only move the bot by teleport, which skips the physics.
- **Missing for health:** the health range of the API is -100 to 100. A boss with more health needs a higher bound or a damage rule.
