# Bot control API

Capability 3 uses version 1 and the `SvGameAi` table in `sv_game_ai.h`.
All methods require the current owner callback, the server thread and a ready world.
They check context first. Invalid context returns `SV_STATE`.
Wrong, retired or wrong-kind bot references return `SV_NOT_FOUND`.
A player who is not a bot returns `SV_DENIED`.
Errors leave game records and output buffers unchanged.

## Methods

| Method | Contract |
| --- | --- |
| `threats` | Copy at most 16 threats. The host sets `item_size` to the size of one threat. Set the output size first. Null or short output returns `SV_INVALID`. A malformed count returns `SV_FAILED`. An absent human produces reference zero. |
| `destination` | Replace the foot route with one position. |
| `waypoints` | Replace the foot route with zero to 64 positions. Zero clears the route and permits a null array. |
| `target` | Select a live human through a reference. The bot cannot select its own body. The host restores this target after each perception scan. |
| `override_input` | Replace four movement axes, buttons, yaw and pitch for 1 to 3600 bot updates. |
| `release` | Clear the target and input lease. The next update runs the default AI. Repeated release succeeds. |
| `perception` | Select target body part 1 (torso) or 2 (head). Other indices return `SV_INVALID`. |
| `aim_mode` | Accept 0 or 1. After the AI update, 1 sets input mask 2 and player `move_mode` 2. Zero clears the mask and sets mode 1. Release and stop restore mode 1. Mode 2 halves base walking speed. |
| `shot_chance` | Set the shot chance from 0 to 100 percent. Zero prevents shots. One hundred removes random hesitation. |
| `street_route` | Set a closed street route of 2 to 64 road points for a seated driver. |
| `seated_target` | Select the current living body of a player for an armed passenger in seat 1, 2 or 3. |
| `chase_mode` | Set 0 or 1 for a bot driver. Restore the first value on stop. Refuse a request to clear a chase that the game sets. |
| `street_near` | Copy one free street position, direction, street point and next node within a radius above 0 and at most 512 units. |
| `road` | Copy road `index` of the street map: the count, the end intersections, the kind, the lanes and the direction. An index from the count up returns `SV_NOT_FOUND`. |
| `intersection` | Copy intersection `index`: the count, the position, the road and the extent at each side and the light of each side. An index from the count up returns `SV_NOT_FOUND`. |
| `aim_snap` | Accept 0 or 1. With 1 the bot turns to its aim at once, on foot and as a passenger. Release and stop clear it. |

Route coordinates must be finite and within plus or minus 32768 on each axis.
Invalid coordinates, a null nonempty array or more than 64 points return `SV_INVALID`.
A route starts at waypoint zero. The API initializes the unused road fields to zero.
A missing or inconsistent bot body returns `SV_STATE` for route, target and input methods.
A bot in a vehicle returns `SV_CONFLICT`. These methods support foot control only.
They do not expose driving topology or modify parent, vehicle or inventory links.

The input size must cover `SvAiInput`.
Each axis must be finite and within -1 to 1.
Yaw must be finite and within approximately plus or minus pi.
Pitch must be finite and within approximately plus or minus pi divided by two.
Buttons must fit the existing 19 input bits.
Invalid input or duration returns `SV_INVALID`.
The input reaches the human through the existing `playersimulation` function.
Expiry restores default AI on the next update.

All enabled native plugins share game write authority, as with the write capability.
The last target or input writer owns the lease for cleanup.
Any valid plugin callback can replace or release that lease.
Each lease stores the bot reference and reset sequence.
Slot reuse and world reset invalidate the lease.
The target uses a human reference, so a new occupant never inherits target identity.
The methods allocate no memory and use no random numbers.

The shot helper uses `os_rand()` for chances from 1 to 99.
Chances 0 and 100 use no random value.
Without a policy, the helper keeps the existing foot chance and the existing passenger behavior.
The default perception remains the original scan across body parts.
The default aim behavior also stays with the game.
The new policy methods require a consistent body and the matching game hooks.
They return `SV_STATE` until those conditions hold.

A street point contains a road index, direction 0 or 1, and lane 0 or 1.
The route must connect the destination of each point to the start of the next point.
The last point must connect to the first point. Immediate reversal on the same road is refused.
The API checks road and intersection capacities, endpoint indices and reciprocal road membership.
It derives turns from the intersections and initializes the two waypoints that the driver reads.
The route helper supplies each next road without the default random road selection.
The existing `ai_playercar` function controls steering and throttle.
The API refuses a trailer, helicopter or train. It accepts vehicle types 0 to 9.
It checks the active vehicle, health, seat count and driver link.
Invalid points return `SV_INVALID`. A missing driver seat returns `SV_STATE`.
The guard before each driver update stops invalid routes with neutral axes and the brake bit.

A seated target uses a player reference and stores that player's current human reference.
It does not follow a new body after respawn.
It refuses the bot itself with `SV_INVALID` and a wrong target reference with `SV_NOT_FOUND`.
It requires a live car, a valid passenger seat and a linked gun with one valid magazine.
Missing equipment or inconsistent seat state returns `SV_STATE`.
The target setting supplies the selected body part to the existing passenger aim code.
These methods change no seat link, item parent or vehicle counter.

The bounds, errors, cleanup and client limits of N58, N59, N60 and N27 are in [bot API additions](game/bot-controls.md).
