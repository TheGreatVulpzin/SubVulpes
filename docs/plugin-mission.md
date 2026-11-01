# Mission API

Capability 5 uses version 1 and `SvGameMission`.
The table appends three methods after size and version.
All methods require the current owner callback, the server thread and a ready world.
They check context before arguments and return `SV_STATE` for invalid context.
Errors leave the game, the scenario counter and output buffers unchanged.

## Methods

| Method | Contract and errors |
| --- | --- |
| `slots(owner, team, out)` | Copy all 16 slots of team 0 to 4. The host sets `item_size` to the size of one slot. Set the output size first. Invalid team, null output or short output returns `SV_INVALID`. |
| `start(owner, team, input, out)` | Fill one free slot with an existing kind and an existing objective item. Invalid input returns `SV_INVALID`. A wrong item reference returns `SV_NOT_FOUND`. An expired item, unavailable core integration or scenario-id collision returns `SV_STATE`. A full set of eight client slots or an exhausted scenario counter returns `SV_LIMIT`. An item already used by a mission returns `SV_CONFLICT`. |
| `cancel(owner, reference)` | Cancel every slot linked to that scenario through the existing game function. Invalid reference size, team or slot returns `SV_INVALID`. An inactive, stale or wrong reference returns `SV_NOT_FOUND`. A linked native stake or an unrepresentable linked slot returns `SV_STATE`. |

The snapshots contain active state, kind, partner, reward, expiry, site, stake and the item reference.
They contain no internal pointer or account identity.
An inactive slot has no reference. An active slot can contain item reference zero if its item is gone.
A mission reference contains its team, slot, scenario, reset sequence and item reference.
The host checks all of them again for cancellation.
It refuses a slot that now holds another scenario or another item.

The start record must cover `SvMissionStart`.
Kinds are 0 to 6. Partners are teams 0 to 4.
Rewards are 0 to 8388607, within the existing packed reward representation.
Expiry must exceed the current world clock and must not exceed `SV_MISSION_EXPIRE_MAX`.
It uses the raw world clock, not milliseconds or simulation ticks.
The existing packet carries expiry divided by 60.
The site must fit 0 to 32766.
Kind 0 also requires a site below the current building count.
Kind 1 uses the native site sentinel 255. Kinds 2 to 6 use site zero.
The objective must be a live item of an existing mission type, 17 to 22, with positive lifetime.

The caller creates the objective with the item API first.
The start method creates no bot, vehicle, building or new item type.
It uses the existing kinds for display. A plugin gives that display its objective and meaning.
It creates no native stake or price-growth rule.
It keeps the objective alive and reserves one scenario id only after every check passes.
The scenario counter must be nonnegative and below `INT32_MAX`.
The method refuses a counter value that already belongs to an active scenario.
The backing array has 16 slots. Creation uses only slots 0 to 7, which event 12 can identify without overlap.
It handles `scenario_getnextmission` returning 8 to 16 before it indexes either slot table.

Cancellation reuses `scenario_setmissioninactive`.
It refuses a scenario with an active slot from 8 to 15, because its update would alias another team.
It removes the scenario's objectives through the existing lifetime path and sends the existing mission updates.
It refuses linked stake changes.
Any valid native plugin callback can cancel a valid mission reference, as with the other game write capabilities.
An API-created mission also records its owner for cleanup when that plugin stops.
Those records are host resource metadata, not plugin feature counters or unused game fields.

## Original client and limits

The API reuses mission event 12 and the existing item lifetime path.
It adds no protocol event, field, model, sound or menu.
The original client shows the existing mission kind and numeric data.
It cannot receive arbitrary mission text through event 12.
A plugin can announce custom text with the existing message API instead.
This was checked in the code, not in a client.

Event 12 encodes `slot * 2 + active + team * 16` in the same low byte.
With equal payloads, team 0 slot 8 and team 1 slot 0 produce the same event.
For kind 5 and item type 18, both produce parameter `0x00120511`.
The remaining three event integers can also be equal.
No client can distinguish those two updates from the packet alone.
This API therefore reads all 16 server slots but starts missions only in the first eight.
It also refuses cancellation or guarded completion when a linked active slot is 8 or higher.

- A full team returns slot 16. The client also aliases slots 8 to 15 with other teams.
  The API applies the stricter limit of eight before any write or counter change.
- Native builders can create several entities before a later constructor fails.
  The API uses a caller-created objective and does not expose those unchecked builders directly.
- Native mission code stores item indices. The API also stores item references and rejects stale identities.
- A zero or reused reference can otherwise identify the wrong objective.
  Cancellation compares the current resolved item with the slot, and lifecycle cleanup protects a reused item.
- Scenario ids can overflow or collide with active missions. The API refuses both cases.
- Rewards share one packed integer with other fields. The API bounds the reward before sending the existing event.
- Native completion adds money without a range check. The proposed core guard checks the full linked payout first.
- Native completion can split unrelated companies after it changes prices.
  The guard refuses unsafe prices before this path runs for an API-owned mission.
- Stake cancellation changes prices and shared stakes. The public cancel method refuses that path instead of guessing its policy.
- Core cleanup needs a nonzero owner. The host guard refuses zero before it examines unowned bindings.
