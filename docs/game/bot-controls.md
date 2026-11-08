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
No method ends a possession before that (N66).
The original client was not tested. The camera of the client follows the human of the update event. Client effect unknown.

## Known limits

These limits come from the design, not from a defect. Each one has a row in the need list of [legacy modules](../legacy-modules.md).

- A driver route is a closed loop. No method reads the road graph, so a plugin cannot find a route from A to B (N63, N64).
- Chase mode switches four effects together: a higher target speed, a larger lateral limit, a lane change and no stop at red lights (N65).
- `street_near` gives one position and has no filter (N67).
- A bot has no method to aim at a point or an entity. The shot chance sets the hesitation to fire and does not set accuracy. Perception has no range, field of view or team filter (N68).
- A possession has no reverse call (N66).
- Passenger aim takes a player as the target, in seats 1 to 3 (N61).
- Most writes return to the first value when the plugin stops. Things that a plugin creates, such as a possession, follow their own rule. Read each section.

## Owner decisions

- Is half-speed walking acceptable while a bot aims? The existing game uses that speed for mode 2.
- Does the requested death counter mean `despawn_timer`? The requirement remains probable until its author confirms it.
- For possession, should the old body stay ownerless or transfer to the bot? This brief selects the ownerless rule.
