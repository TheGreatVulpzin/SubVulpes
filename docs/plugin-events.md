# Plugin events (N16)

Status: design of 2026-10-27. Step 1 of the order of work is built: the record, the queue and the delivery of kind 1 events. The core does not post any event yet (step 2). It serves the need N16 and the damage decision N24 in the [legacy module catalog](legacy-modules.md).
The plugin API today has two events, `SV_EVENT_TICK` and `SV_EVENT_STOP`. Both run between two ticks, so a plugin never meets the simulation.

## Three kinds of event

| Kind | When the plugin runs | What the plugin can do | Risk |
| --- | --- | --- | --- |
| 1. Notice | At the next safe point, before the tick event | Everything that the API allows | None for the simulation |
| 2. Decision at a safe point | At once, in the network code of the server, outside the simulation | Everything that the API allows. It answers allow or deny | Low. A slow plugin slows the network code |
| 3. Decision inside the simulation | At once, in the middle of a calculation of the game | Read only. It answers allow, deny or a new amount | High. See "Danger of kind 3" |

A kind 1 event is a small record. The core writes it into a queue of 1,024 records when the event happens. The host gives the records to the plugins in order, before it gives the tick event.
When the queue is full the core drops the new record and counts it. The next delivery starts with a "lost events" record that has the count.
A plugin gets only the kinds that it asked for with `subscribe`. The record starts with the `SvEvent` of ABI version 1 (`size`, `kind`, `tick`), so an older plugin that reads only those 12 bytes keeps working. The fields of each event follow.

## The events

| Event | Kind | Fields | Answer |
| --- | --- | --- | --- |
| Chat text | 2 | Player, text | Allow or deny. A deny drops the text. The plugin commands already run at this point |
| Join | 2 | Player, account | Allow or deny. A deny ends the connection. The hook point is not found yet |
| Leave | 1 | Player, account | None |
| Spawn | 1 | Human, player | None. A plugin that wants another position moves the human at the same safe point, so the human shows the old place for one tick |
| Death | 1 | Human, player, the killer when the game knows it | None |
| Damage | 3 | Source kind (bullet, fist, blade, vehicle), attacker, target (human, object or wheel), amount | Allow, deny, or a new amount |
| Phone digit and call | 2 | Defined with the phone interface (N15) | Claim or pass |

## The safe route for damage

Most damage rules do not need kind 3. The core can read a number that the plugin wrote before.
The property table gets these rows when this design is built: `human.damage_taken_scale`, `human.damage_dealt_scale` and `object.damage_taken_scale`.
Each is a decimal number, 1 by default. The core multiplies the amount by it. A value of 0 cancels the damage.
This route covers the safe spawn area, the car that nothing destroys and the spawn protection fix. The plugin changes the number at a safe point, so the effect can come one tick late.
It cannot express a rule that depends on one hit. That rule needs kind 3.

## Danger of kind 3

Kind 3 exists for a rule that must see each hit. The owner of the plugin takes the responsibility for it. The host does not make it safe.

What the host does:

- The plugin must ask for kind 3 in `plugin.ini` (`unsafe_events=1`). The approval of the plugin shows this to the administrator.
- Inside the callback the write API and the property table `set` and `add` return `SV_STATE`. A plugin cannot change the game in the middle of a calculation. The reads work.
- The host checks the answer. A new amount must be inside the range of the damage, or the host ignores the answer and allows the damage.
- The host measures the time of each decision. A plugin that is slower than 200 microseconds is counted. More than 10 slow answers in one minute disable the plugin.
- A plugin that reports a failure is disabled, as for any other event.

What the host does not do, and the owner must handle:

- Speed. The server waits for the answer. A slow answer slows every player.
- A crash. A native plugin is code in the process of the server, so its crash is a crash of the server. This is true for every native plugin, and kind 3 raises the chance, because it runs in the middle of the game code.
- A half-made world. In the middle of a calculation some positions and timers are already new and some are old. A rule that reads two values can see an impossible pair.
- A loop. An answer must not call anything that starts the same event again.
- Many plugins. The plugins answer in the order that they loaded. The first deny wins. New amounts go from one plugin to the next.
- Network cost. A decision for each bullet of an automatic gun can run many times in one tick.

Use kind 3 only when the safe route cannot say the rule. Keep the answer short: read a few numbers, compare, return.

## Order of work

1. Done. The event mechanism: `SvGameEvent` (`sv_game_events.h`), the queue (`plugin_events.c`) and the delivery before the tick event. The core calls `sv_events_post(kind, tick, player, human, actor)`.
   The post does nothing when no active plugin asked for the kind, when the kind is not one that the core posts, or when the call is not on the server thread. The queue holds 1,024 records.
   Tests: the order, a human that is gone at delivery, a flood of 1,100 posts (1,024 kept, a lost record with 76 that comes first), a bad mask, and no record after the stop.
   Removing the queue limit, the kind filter, the ring index and the mask check made `game-api` fail each time (`build/logs/mutation16.log`).
2. The first notices: spawn, death and leave. Find the hook points in the core.
3. The decision at a safe point for chat and join.
4. The safe route for damage: the three rows and the small change in the damage code. The result equals today while the scale is 1.
5. Kind 3: the restricted context, the time budget, the approval text and the tests of each limit.

Each step has tests, a removal proof for each check that guards memory or a limit, and a note here. The original client sees no change: the core sends the same packets.
