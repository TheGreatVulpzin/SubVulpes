# Plugin events

An event tells a plugin that something happened in the game. A plugin picks the events that it wants with `subscribe`. Events come in three kinds, from safe to risky.

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
| Chat text | 2 | Player, text | Allow, deny or replace. A deny drops the text. A plugin may write a replacement of 1 to 63 printable ASCII bytes. The plugin commands already run at this point |
| Join | 2 | Player, account | Allow or deny. A deny ends the connection. |
| Leave | 1 | Player, account | None |
| Spawn | 1 | Human, player | None. A plugin that wants another position moves the human at the same safe point, so the human shows the old place for one tick |
| Death | 1 | Human, player, the killer when the game knows it | None |
| Round (`SV_EVENT_ROUND`, 4096) | 1 | The new state of the round in `value` (1 waiting, 2 playing, 3 ended) and the number of the round in `extra` | None. The host looks for a change at the start of each tick, so the event comes one tick after the change. The first look only records the state |
| Vehicle seat (`SV_EVENT_VEHICLE`, 8192) | 1 | Human, player, vehicle, seat in `value`, 1 in `extra` for a human that got in and 0 for one that got out | None. Posted for the use button, the exit button, `vehicle_exit` and `vehicle_enter`. A seat that the game sets by itself (a bot in a traffic car) gives no event |
| Shot (`SV_EVENT_SHOT`, 16384) | 1 | Human and player of the shooter, the gun in `item`, its type in `value`, the rounds left in the gun (the chamber plus its magazine) in `extra` | None. Posted by the game when a gun fires. A shot that a plugin shows with `bullet` is not a shot |
| Vehicle gone (`SV_EVENT_OBJECT_GONE`, 32768) | 1 | The vehicle (a reference that no longer resolves), its type in `value` and its color in `extra` | None. Posted when the game removes a vehicle at the end of its life |
| Item in a slot (`SV_EVENT_ITEM`, 65536) | 1 | Human, player, item, slot in `value`, 1 in `extra` when the item went in and 0 when it left | None. A pick up is an item that goes in, a drop is an item that leaves. A move between two slots gives both. The creation of a body with items also gives them |
| Damage | 3 | Source kind (bullet, fist, blade, vehicle), attacker, target (human, object or wheel), amount | Allow, deny, or a new amount |
| Phone digit and call | 2 | A phone digit or a call. See [the shop and phone API](plugin-shop.md) | Claim or pass |

## The safe route for damage

Most damage rules do not need kind 3. The core can read a number that the plugin wrote before.
The property table has these rows: `human.damage_taken_scale`, `human.damage_dealt_scale` and `object.damage_taken_scale`.
Each is a decimal number, 1 by default. The core multiplies the amount by it. A value of 0 cancels the damage.
This route covers the safe spawn area, the car that nothing destroys and the spawn protection fix. The wheels of a car follow the object scale. The plugin changes the number at a safe point, so the effect can come one tick late.
It cannot express a rule that depends on one hit. That rule needs kind 3.

## Danger of kind 3

Kind 3 exists for a rule that must see each hit. The owner of the plugin takes the responsibility for it. The host does not make it safe.

What the host does:

- The plugin must ask for kind 3 in `plugin.ini` (`unsafe_events=1`). The approval of the plugin shows this to the administrator.
- Inside the callback the write API and the property table `set` and `add` return `SV_STATE`. A plugin cannot change the game in the middle of a calculation. The reads work.
- The host checks the answer. A new amount must be inside the range of the damage, or the host ignores the answer and allows the damage.
- The host measures the time of each decision. A plugin that is slower than 200 microseconds is counted. More than 10 slow answers in one minute disable the plugin.
- A plugin that reports a failure is disabled, as for any other event.

What the host does not do, and the plugin author must handle:

- Speed. The server waits for the answer. A slow answer slows every player.
- A crash. A native plugin is code in the process of the server, so its crash is a crash of the server. This is true for every native plugin, and kind 3 raises the chance, because it runs in the middle of the game code.
- A half-made world. In the middle of a calculation some positions and timers are already new and some are old. A rule that reads two values can see an impossible pair.
- A loop. An answer must not call anything that starts the same event again.
- Many plugins. The plugins answer in the order that they loaded. The first deny wins. New amounts go from one plugin to the next.
- Network cost. A decision for each bullet of an automatic gun can run many times in one tick.

Use kind 3 only when the safe route cannot say the rule. Keep the answer short: read a few numbers, compare, return.

## Core command and login hooks

The core posts `SV_EVENT_CORE_COMMAND` after plugin commands and before its inherited login and commands.
Subscribe with `SV_EVENT_EXT_BIT(2)` and read `SvCoreCommandEvent` after checking its size.
The record holds the player, a lowercase command name, arguments and the inherited command name in `builtin`.
Names and arguments hold at most 63 bytes. The builtin holds at most 15 bytes.
The builtin keeps the inherited case-sensitive prefix tests. For example, `/army` has builtin `arm`, while `/ARMY` has no builtin.
The list commands use builtin `list`. The account ban commands use builtin `ban`.

Only `decision` is writable. Each subscriber receives the original input and `SV_COMMAND_DEFAULT`.
Default keeps the admin flag rule. Allow runs the inherited commands for this text without that flag.
Deny stops the commands and login and keeps the text out of chat.
The host calls at most 32 plugins in load order. Deny wins over allow, and allow wins over default.
A callback failure contributes no decision and disables that plugin.
Allow never grants a password login. Login still checks the password and attempt limit.
The command arguments are empty for `admin`, so no plugin receives its password.

`SV_EVENT_ADMIN_LOGIN` uses `SvAdminLoginEvent` and subscription bit `SV_EVENT_EXT_BIT(3)`.
It runs before the attempt counter and password check. An `SV_DENIED` answer skips both.
`SV_EVENT_ADMIN_SUCCESS` uses the same record and subscription bit `SV_EVENT_EXT_BIT(4)`.
The core queues this notice after it sets both admin flags. The host delivers it before a later tick callback.
Both records contain only the player. They contain no attempt text or password.
Without subscribers, the inherited command and login rules stay the same.
