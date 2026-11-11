# Bot API additions

## Aim posture (N58)

`aim_mode` accepts 0 or 1 for a bot with a consistent body.
After the bot update, 1 sets input mask 2 and player `move_mode` 2.
Zero clears that input mask and sets `move_mode` 1.
The player simulation copies the mode to the human on the same tick.
Release and plugin stop restore mode 1 and clear the aim input.
The host binds cleanup to the player reference and the world reset sequence.
A reused slot never receives that cleanup.

Mode 2 sets base walking speed to 0.025 units per tick, or 1.5 units per second at 60 ticks per second.
Mode 1 sets 0.05 units per tick, or 3 units per second.
Input mask 8 multiplies either speed by 0.625.
These are speed targets in the existing walk code. This change does not change that code.
The original client was not tested. Client effect unknown.

## Driver chase (N59)

`chase_mode(owner, bot, enabled)` appends a method to `SvGameAi`, capability 3, version 1.
It accepts 0 or 1 during a safe callback on the server thread, with a ready world.
A non-bot returns `SV_DENIED`. A stale or wrong-kind reference returns `SV_NOT_FOUND`.
A bad value returns `SV_INVALID`. A missing car driver returns `SV_STATE`.
The body and car must be active. The car must have health above zero, type 0 to 9, and 1 to 7 seats.
Seat zero and the car driver link must identify this bot.
An unsafe callback or a wrong thread returns `SV_STATE`.

The last plugin writer owns cleanup. The host saves the first value, reference and reset sequence.
Stop restores that first value when the same bot still holds the last written value.
A chase that the game sets stays active. A request to clear that chase returns `SV_DENIED`.
The method allocates no memory and needs no restart.
It changes the existing driver behavior without a new packet.
The original client was not tested. Client effect unknown.

## Chase effects (N65)

`chase_effects(owner, bot, parts)` appends a method to `SvGameAi`, capability 3, version 1.
It switches the four effects of a chase one by one: `SV_CHASE_SPEED` (a higher target speed and a shorter gap to the car ahead),
`SV_CHASE_LATERAL` (a lateral limit of 32 in place of 16), `SV_CHASE_LANE` (a lane change when a car is ahead) and
`SV_CHASE_LIGHTS` (no stop at a red light). `SV_CHASE_ALL` is the same as `chase_mode` 1. Effects 0 switches the chase off.
A bit outside `SV_CHASE_ALL` returns `SV_INVALID`. Every other rule, result and cleanup is the rule of `chase_mode`.
In the game, `Player.ai_chase` is 0, 1 (all four, as the scenario writes it) or a mix of the bits in `src/game/ai_car.h`.
The value 1 and the value 0 behave as before. The test checks the stored value of each mix and the full chase,
and it does not measure the driving of each part alone.

## Street list (N67)

`street_list(owner, position, radius, filter, out, capacity, count)` appends a read to `SvGameAi`, capability 3, version 1.
It is `street_near` with more than one answer. It gives up to `capacity` candidates (1 to 8), the nearest first, at most one for each lane.
The filter may be NULL. Then it is the filter of `street_near`: both road directions, a human gap of 4 units and an object gap of 16 units.
`kinds` is a mix of `SV_STREET_ALONG_X` and `SV_STREET_ALONG_Z`. `human_gap` and `object_gap` are the least distance from a candidate to any human and to any object, from 0 to 64 units.
A gap of 0 turns that check off. The other bounds, the sample rule and the failure results are the rules of `street_near`.
`*count` gets the number of candidates. With none, the call returns `SV_NOT_FOUND` and `*count` is 0. A kind of 0, a bit outside `SV_STREET_ANY`, a gap outside 0 to 64,
a capacity outside 1 to 8 or an `out` entry with a short size returns `SV_INVALID`. `out` stays unchanged on every error.
`street_near` now calls the same scan with a capacity of 1, and the `n-roads` test checks that its answers did not change.

## Input lock of a player (N71, N74)

`input_lock(owner, player, parts)` appends a method to `SvGameWrite`, capability 2, version 1.
It holds the input of a real player. `parts` is a mix of `SV_INPUT_LOCK_BODY` and `SV_INPUT_LOCK_VIEW`, or 0 to end the lock.
The body part keeps the four movement axes and the buttons at 0, so the body does not walk, jump or shoot.
The view part keeps the yaw, the pitch and the two arm angles at the values that the player has when the lock starts.
The main loop calls `sv_input_lock_apply` after `serverrecv`, so the held values replace the input that the client sent in the same tick.
Item actions and menu choices in the packet still run, because the server reads them before the lock. A plugin cannot cancel them with this call.
A bot returns `SV_DENIED`: use `override_input`. A player that another plugin locks returns `SV_CONFLICT`. A stale reference returns `SV_NOT_FOUND`.
`parts` outside 0 to 3 returns `SV_INVALID`. A call outside a safe callback returns `SV_STATE`.
The lock ends when the plugin stops, when the player leaves, or when the round resets.
Client effect unknown: the client predicts its own movement and may fight the server, and nobody has seen it with the original client.
The view part also covers the camera need of N74 for a player. For a bot on foot, `aim_at` and `override_input_partial` with the view part turn the body,
because the game copies the view to the body on each tick. A one-time write of the body angle would be undone on the next tick, so N74 has no separate property.

## Voice routing (N48)

`voice_route(owner, speaker, listener, mode)` appends a method to `SvGameWrite`, capability 2, version 1.
It sets who hears whom. `mode` is `SV_VOICE_DENY`, `SV_VOICE_FORCE` or `SV_VOICE_DEFAULT`.
- `SV_VOICE_DENY`: the listener never hears the speaker.
- `SV_VOICE_FORCE`: the listener hears the speaker at any distance, in one of its 8 voice slots.
- `SV_VOICE_DEFAULT`: the call ends the rule.

The listener must be a real player. A bot listener returns `SV_DENIED`. A rule of another plugin for the same pair returns `SV_CONFLICT`.
256 rules at most exist, and the next one returns `SV_LIMIT`. A stale reference returns `SV_NOT_FOUND`.
The server asks `sv_voice_mode` inside `calculateplayervoice`, so the 8 slots and the radio logic do not change. A radio is not changed by a rule.
A rule ends when the plugin stops, when one of the two players leaves, or when the round resets.
Client effect unknown: the client may fade a voice by the distance that it knows.
No automatic test drives `server.c`. A test in `n-bots` checks the results of the call only. Check the sound in the game.

## Train speed (N49, in part)

`train_speed(owner, locomotive, throttle)` appends a method to `SvGameWrite`, capability 2, version 1.
It sets the target speed of a locomotive (type 12). `throttle` is any finite number from 0 up, with no upper bound. The game uses 0.4.
A high value is not tested: the train is a box held by bonds, and a high speed may break a bond or leave the track.
A target of 0 brakes the train anywhere. The door face of the locomotive follows the real speed, so a train that stops opens its door.
A value of -1 gives the control back to the game. A value that is not a finite number, or is below 0 (other than -1), returns `SV_INVALID`.
A bogie (type 13) and every other object return `SV_DENIED`. A locomotive that another plugin controls returns `SV_CONFLICT`.
8 locomotives at most can have a setting, and the next one returns `SV_LIMIT`.
The code of the locomotive in `objects.c` calls `sv_train_throttle` once for each tick.
The setting ends when the plugin stops, when the locomotive is gone, or when the round resets.
Client effect unknown. The test checks the results of the call only, not the movement of the train.
The property `train.stop_ticks` holds a train at a station or sends it on. See the [property table](../plugin-game-api.md).

## Driver speed (N64)

`driver_speed(owner, bot, speed)` appends a method to `SvGameAi`, capability 3, version 1.
It sets the base target speed of a bot that drives a car. The unit is the distance in one tick.
The default is 0.33, and a chase car uses 0.5 or 0.67. The game scales the base by the turn of the road, so the car slows in curves.
`speed` is any finite number from 0 up, with no upper bound. A high value is not tested. -1 gives the control back to the game.
The brake before a crossing, the red light and the cars ahead still lower the speed. The stuck logic does not change.
The call works with chase mode: the value replaces the base of both modes, and the chase effects for lanes and lights stay.
A bot that does not drive a car returns `SV_STATE`. A stale reference returns `SV_NOT_FOUND`. A bad value returns `SV_INVALID`.
The setting is part of the bot control lease. Release, plugin stop, slot reuse and world reset clear it.
The server drives the car, so no client effect is expected.

## Aim noise and sight range (N68)

`accuracy(owner, bot, scale)` and `sight_range(owner, bot, range)` append two methods to `SvGameAi`, capability 3, version 1.
They work for a bot on foot. The game does not aim at the true position of an enemy.
It keeps a noisy estimate. The noise shrinks while the bot sees the enemy for longer, and it grows with the distance.
`scale` multiplies the noise. 1 is the game rule, 0 is a perfect estimate, and 4 is four times as noisy.
`range` is the distance at which the bot looks for enemies. The game uses 128.
Both values are any finite number from 0 up, with no upper bound. -1 ends the setting.
The bot still needs a clear line to the enemy, and the enemy must be in front of the bot.
The reaction speed, the shot chance and the fire cone do not change. Use `shot_chance` for the hesitation to fire.
A human returns `SV_DENIED`, a bot with no body returns `SV_STATE`, a stale reference returns `SV_NOT_FOUND`,
and a bad value returns `SV_INVALID`. Both settings are part of the bot control lease.
Release, plugin stop, slot reuse and world reset clear them.

## Open street route (N64)

`street_route_open(owner, bot, points, count, end_action)` appends a method to `SvGameAi`, capability 3, version 1.
It is `street_route` for a route with a start and an end. It has 2 to 64 points, and each point joins the next one. The last point does not join the first.
At the end of the last road the driver does the end action. `SV_ROUTE_STOP` brakes and holds the car until the plugin gives a new route or calls `release`.
`SV_ROUTE_RELEASE` gives the car back to the default AI. An end action that is neither returns `SV_INVALID`.
Every other rule, bound and result is the rule of `street_route`, and the cleanup when the plugin stops is the same.
The test checks the points that the game reads at each crossing, the end of the route for both actions, and the brake of the stop. It does not drive a car along the route.

## Aim at a target (N68, N61)

`aim_at(owner, bot, target, ticks)` appends a method to `SvGameAi`, capability 3, version 1.
The target is a point (`SV_AIM_POINT`) or an entity (`SV_AIM_ENTITY`): a human, or a vehicle or another object.
For `ticks` updates (1 to 3600), after the default AI has run, the call turns the yaw and the pitch of the bot to the target.
The angles are the angles that the AI uses for a threat in `player_ai.c`: `atan2(dx, -dz)` for the yaw and `atan2(-dy, sqrt(dx*dx + dz*dz))` for the pitch,
measured from the head bone of the bot, with 0.5 units taken from the height. For a human target the point is the torso, or the head when the perception is 1.
The aim ends when the entity is gone, after `ticks` updates, or with `release`. It does not fire. A plugin fires with `override_input_partial`.
It works for a bot on foot and for a passenger in seat 1 to 3. A driver, a bot with no body, or a seat that is not a passenger seat returns `SV_STATE`.
An entity that is gone returns `SV_NOT_FOUND`. A bad kind, a point outside the map, a short target and `ticks` outside 1 to 3600 return `SV_INVALID`.
`override_input_partial` takes the buttons and the view of a passenger in seat 1 to 3, and no axes: the axes of the driver drive the car.
The test checks the angles of a point, of an entity and of a passenger against the formula, and that the aim ends. It does not show that the game moves the arm of a passenger the same way.

## Partial input override (N76)

`override_input_partial(owner, bot, input, parts, button_mask, ticks)` appends a method to `SvGameAi`, capability 3, version 1.
It is `override_input` for some parts of the input. `parts` is a mix of `SV_AI_PART_AXES`, `SV_AI_PART_BUTTONS` and `SV_AI_PART_VIEW` (yaw and pitch).
For the buttons, `button_mask` says which bits to replace. The other bits stay with the default AI.
The default AI runs first, and the chosen parts go over its result for each of the `ticks` updates.
All parts with all button bits (`SV_AI_BUTTONS_ALL`) are the same as `override_input`, which skips the default AI.
The aim policy of `aim_mode` keeps the last word on button bit 2. A bot in a vehicle returns `SV_CONFLICT`, as for `override_input`.
`parts` 0, a bit outside `SV_AI_PART_ALL`, a button bit outside `SV_AI_BUTTONS_ALL`, a null input and `ticks` outside 1 to 3600 return `SV_INVALID`.
The test checks that the default AI runs, that the chosen axes, button bits and yaw go over it, and that the effect ends after the given updates.

## Death counter (N60)

`human.despawn` appends an integer property with get and set, capability 10, version 1.
The range is 0 to 3600 ticks. The slot is zero.
Set requires a safe callback on the server thread and a ready world.
A human with health above -4 returns `SV_STATE`.
A stale, inactive or wrong-kind human reference returns `SV_NOT_FOUND`.
A bad range or slot returns `SV_INVALID`.
The host saves the first timer and binds cleanup to the human reference and world reset sequence.
The last plugin writer owns cleanup. Stop restores the timer if that same human still exists.
Cleanup never revives a body, recreates a body, or reverses a death tax.

Below 2700, the next dead-body tick releases the player through the normal game code.
The release clears both ownership links and applies the normal death rules.
At zero, normal simulation can remove the body. Rounds mode keeps a dead body for at least four ticks.
Health -4 is accepted, but normal simulation resets its timer to 3600 while it has a player.
A write before the first death tick can bypass the timer-based death notice.
This operation allocates no memory and needs no restart.
The original client was not tested. Client effect unknown.

## Street query (N27)

`street_near(owner, position, radius, out)` appends a read to capability 3, version 1.
It requires a host callback on the server thread and a ready world.
Each position axis must be finite and within -32768 to 32768.
The radius must be finite, above zero and at most 512 units.
The output size must cover `SvAiStreetPosition`.
The result copies one street point, position, direction vector and next node index.
It returns no pointer into the shared road graph and allocates no memory.

The search checks at most 512 roads, two directions and two lanes per direction.
Each lane has 17 samples, at eight-unit steps within 64 units of the projection.
Samples stay eight units beyond each intersection extent and follow the six-tile city ramp.
The result is a car position, 1.5 units above the street.
This bounded search can miss a free position between samples or farther along the same road.
A candidate needs 16 units from every active object center and four units from every active human center.
This is a bounded center-distance check. It does not reserve space or prove clearance for every vehicle shape.
No candidate returns `SV_NOT_FOUND`. Bad arguments return `SV_INVALID`.
Bad graph capacities, endpoints, membership, geometry or lane counts return `SV_FAILED`.
An invalid context returns `SV_STATE`.
Errors leave the output unchanged.
The query changes no game state, sends no packet and needs no restart.

## Possession (N45)

`possess(owner, player, bot)` appends a method to `SvGameAi`, capability 3, version 1.
It gives a real player the body of a bot. It needs a safe callback on the server thread and a ready world.
A player that is a bot returns `SV_DENIED`. A stale or wrong-kind reference returns `SV_NOT_FOUND`.
Both bodies must be consistent and alive, on foot, with the player account and the teams in range. Any other state returns `SV_STATE`.
A second possession for the same player while one is active returns `SV_CONFLICT`.

The call swaps the body links of the two players and sets the team of the new body to the team of the player.
It rolls back before it publishes if a link does not hold, and it returns `SV_FAILED`.
The old body of the player stays in the world without an owner. The bot has no body after the call.
The host sends the update events for both players and both bodies.
The possession ends when the plugin stops or the world resets. The host then gives the bodies back if they still match.
`unpossess(owner, player)` ends a possession before that. It appends a method to `SvGameAi`, capability 3, version 1.
It needs a safe callback on the server thread. A stale or wrong-kind reference, or a player with no possession, returns `SV_NOT_FOUND`.
A possession that another plugin made returns `SV_DENIED`. A possession from before a round reset is gone, so it returns `SV_NOT_FOUND`.
The call uses the same release as the stop of the plugin. The bot gets its body back when the body has no owner.
The player gets the old body back when that body is alive and has no owner. The body the player left stays without an owner otherwise.
The host sends the update events for both players and both bodies.
The original client was not tested. The camera of the client follows the human of the update event. Client effect unknown.

## Street map reads (N63)

`road(owner, index, out)` and `intersection(owner, index, out)` append two reads to `SvGameAi`, capability 3, version 1.
They require a host callback on the server thread and a ready world.
The output size must cover `SvAiRoad` or `SvAiIntersection`. A null or short output returns `SV_INVALID`.
Graph counts outside 0 to 512 roads or 0 to 1024 intersections return `SV_FAILED`.
An index from the count up returns `SV_NOT_FOUND`. An invalid context returns `SV_STATE`.
Errors leave the output unchanged.

A road copies its count, the two end intersections, the kind (0 along x, 1 along z), the lanes on each side,
the unit direction from the first end to the second and the unit side vector.
A street point with direction 0 runs from `ends[0]` to `ends[1]`.
An intersection copies its count, its position, the road at each of the four sides (-1 for none),
the extent of each road and the light of each side.
The light is the value that `logicsimulation` writes for the cars that arrive on that side: 0 green, 1 yellow, 2 red.
A car stops at 1 and 2 unless it is in chase mode. The light changes each tick, so the copy holds for the current tick.
The reads copy values only. They return no pointer into `sr_data_city`, change no game state and allocate no memory.
A plugin can walk the graph and build a closed `street_route` from A to B and back.

## Trigger count (N75)

`item.trigger_ticks` appends an integer property with get and set, capability 10, version 1.
The range is 0 to 3600 ticks. The slot is zero. `add` returns `SV_DENIED`.
Set requires a safe callback on the server thread and a ready world.
A stale, inactive or wrong-kind item reference returns `SV_NOT_FOUND`. A bad range or slot returns `SV_INVALID`.

`itemweaponsimulation` counts the ticks that the use bit of a held gun is down and fires when the count equals 2
(6 for the Magnum), with a round and no cooldown.
An automatic gun stops the count at that value and fires at each cooldown while the trigger stays down.
The 9 mm and the Magnum count on, so a held trigger fires once.
A write of 0 lets a held pistol fire again after 2 ticks (6 for the Magnum).
A write of 0 on each tick holds the fire of an automatic gun. A value above the need also holds it, because the count
then never equals the need.
When the use bit is up, the game sets the count to 0 on each tick. So the effect of a write ends at the next release
of the trigger, and stop has no value to restore. The host keeps no cleanup record for this row.
The original client was not tested for the gun of a real player. Client effect unknown.

## Aim snap (N77)

`aim_snap(owner, bot, enabled)` appends a policy method to `SvGameAi`, capability 3, version 1.
It accepts 0 or 1 for a bot with a consistent body, during a safe callback on the server thread, with a ready world.
A non-bot returns `SV_DENIED`. A stale or wrong-kind reference returns `SV_NOT_FOUND`.
A bad value returns `SV_INVALID`. A missing body or missing game hooks return `SV_STATE`.

With 1, `playerai` sets the yaw and pitch of the bot to its aim at the current threat in the same update,
on foot and as an armed passenger. Without the policy the game turns by 1/16 of the gap on each update,
and a bot fires only within 0.125 rad. The game still decides each shot: the threat level, the distance,
the friend and car body checks and the shot chance do not change.
The policy is part of the bot control lease. Release, plugin stop, slot reuse and world reset clear it.
The original client was not tested. Client effect unknown.

## Known limits

These limits come from the design, not from a defect. Each one has a row in the need list of [legacy modules](../legacy-modules.md).

- A driver route can be open (`street_route_open`), and `driver_speed` sets the base speed. A stop at a given point is still open (N64, the rest).
- Chase mode switches four effects together: a higher target speed, a larger lateral limit, a lane change and no stop at red lights (N65).
- The shot chance sets the hesitation to fire. `accuracy` sets the aim noise, and `sight_range` sets the range. Perception has no field of view or team filter (N68, the rest).
- A possession has no reverse call (N66).
- Passenger aim takes a player as the target, in seats 1 to 3 (N61).
- Most writes return to the first value when the plugin stops. Things that a plugin creates, such as a possession, follow their own rule. Read each section.

## Owner decisions

- Is half-speed walking acceptable while a bot aims? The existing game uses that speed for mode 2.
- Does the requested death counter mean `despawn_timer`? The requirement remains probable until its author confirms it.
- For possession, should the old body stay ownerless or transfer to the bot? This brief selects the ownerless rule.
