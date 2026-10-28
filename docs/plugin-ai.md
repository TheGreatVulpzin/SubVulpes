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
| `target` | Select a live human through a reference. The bot cannot select its own body. The host seam restores this target after each perception scan. |
| `override_input` | Replace four movement axes, buttons, yaw and pitch for 1 to 3600 bot updates. |
| `release` | Clear the target and input lease. The next update runs the default AI. Repeated release succeeds. |
| `perception` | Select target body part 1 (torso) or 2 (head). Other indices return `SV_INVALID`. |
| `aim_mode` | Set input bit 2 after the AI update. Accept only 0 or 1. |
| `shot_chance` | Set the shot chance from 0 to 100 percent. Zero prevents shots. One hundred removes random hesitation. |
| `street_route` | Set a closed street route of 2 to 64 road points for a seated driver. |
| `seated_target` | Select the current living body of a player for an armed passenger in seat 1, 2 or 3. |

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
The new policy methods require a consistent body and installed core seams.
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
The target seam supplies the selected body part to the existing passenger aim code.
These methods change no seat link, item parent or vehicle counter.

## Required core integration

Production integration remains pending outside this area's file ownership.
The fixture installs and exercises the seams directly.
The production host does not install them.
Until installation, valid control and policy calls return `SV_STATE` without a lease.
Threat reads and foot routes work without these seams.

The core owner must make these changes together:

1. Call `sv_ai_install()` when the game host starts.
2. In the bot loop, call `playerai(i)` only when `sv_ai_before_player(i)` returns zero.
3. After each `ai_assessthreats` call inside `playerai`, call `sv_ai_after_threats(player)`.
4. Apply `sv_ai_after_player(i)` after default AI or an input override.
5. Use `sv_ai_perception_part(player)` for the target body part when the result is 1 or 2.
6. At the existing shot decision, use `sv_ai_shot(player, passenger)` with passenger 0 or 1.
7. Return from `ai_car_nextwaypoint` when `sv_ai_route_next(player)` returns nonzero.
8. Call `sv_ai_release_owner(owner)` before the plugin manager clears an owner's resources.

Install only after all seams exist. A missing cleanup seam can leave a disabled plugin's target active.
The target seam supplies a single high-level threat and a foot route when applicable.
Default AI still controls aim, combat and friendly fire.
The original client still uses its existing human input and state formats.
The boss example and client behavior remain untested.

## Source hazards

The implementation read `player_ai.c`, `ai_car.c`, `player.c` and the main loop before adding methods.

- `playerai` clears input after plugin tick callbacks. A write during the callback alone cannot override movement.
- `ai_assessthreats` selects the target again. A single write to `ai_target` cannot keep a chosen target.
- Driving uses road indices and the next waypoint. Foot positions do not contain valid road topology.
- Negative or excessive threat counts can access memory outside the 16-entry arrays.
- More than 64 route points can overwrite the destination and other player fields.
- Internal bot fields use pool indices. A reused slot can refer to another human or player.
- A body link can be absent or inconsistent. The API checks it before route or control changes.
- The API creates no entity and changes no parent link or shared counter.
- Passenger combat reads the first gun and magazine without its own validity checks.
  The seated-target API refuses missing or invalid equipment before enabling this path.
- Passenger aim has a defined base angle only for seats 1 to 3. The API refuses the other seats.
- The default road builder can use a missing road after 512 failed choices.
  A controlled street route uses the bounded route helper instead.
- A driver reads the current and next points, endpoint records and a previous traffic-light index.
  The API checks those indices before the update.

## Evidence

The plugin fixture calls every method inside a real tick callback.
It checks each implemented result code, wrong-kind references and wrong-thread calls.
The host uses real player and human constructors.
It checks the route, target, input transfer, expiry, explicit release, owner cleanup, reset and slot reuse.
The expanded fixture checks the torso/head choice, aim bit, shot chance and a complete cycle of road transitions.
It calls the real driver function once away from a crossing and checks the throttle range.
It checks the selected passenger threat and the unchanged seat links.
It does not run the complete seated combat path, live driving through crossings or a live client.
The shot test proves that 100 percent consumes no random value. It does not measure statistical fairness.
Build and removal results appear below.
The fixture proves the proposed seams, not production integration.
The original client was not tested.

## Vehicle exit request

The user owns vehicle exit and seat changes in the other worktree.
This area implements no vehicle exit, seat write or new ejection path.
Expose a validated exit through the write capability and preserve both sides of the seat relation.
After that operation succeeds, a plugin can call `target` or `waypoints` for foot combat.
Until that integration exists, the automatic transition from a seated shooter to foot combat remains unsupported.

## Work report

This area remains incomplete in production because the core does not call the new seams.
This commit finishes the AI work that its file ownership permits.
N21, N22, settings, storage, engine and shop retain their existing stubs at this checkpoint.
The next ordered area is economy. Core integration remains a separate request to the core owner.
This report stays in the area's documentation because the later instruction permits only the five AI files.
It supersedes the earlier request to write a separate `docs/codex-report.md` file for this work.
No change touches `humans.c`, `collide.c`, a write-table file or another area's files.
No merge, push or tag ran.

### Files changed

| File | Change |
| --- | --- |
| `src/plugins/sv_game_ai.h` | Eleven methods after size and version, copied ABI records, bounds and host seam declarations. |
| `src/plugins/plugin_ai.c` | Validation, threat copies, foot routes, reference-bound control, AI policies, street routes and passenger targets. |
| `tests/systems/ai_plugin.c` | Public calls inside the tick, negative cases, wrong-thread calls and commands for host checks. |
| `tests/systems/ai_host.c` | Real constructors, malformed-state cases, post-tick state checks and seam checks. |
| `docs/plugin-ai.md` | Contract, hazards, evidence, limits and this report. |

### Command results

Inspection used `Get-Content`, `rg`, `Get-Item`, `Get-ChildItem` and `Get-Command`.
The reads covered the required instructions and these implementation files:
`plugin_write.c`, `plugin_game.c`, `plugin_host.c`, `plugin_entities.c`, the public plugin headers,
`player_ai.c`, `ai_car.c`, `player.c`, `logic.h`, `logic_sim.c`, `economy.c`, `missions.c`, `scenario.c`,
`city.c`, `city.h`, `items.c`, `items.h`, `object_create.c`, `human_arms.c`, `sr_types.h`, `os_calls.c`,
the systems fixtures, CMake files, build script and native/repository checks.
Those reads succeeded.
Three initial paths did not exist: `docs/plugin-ai.md`, `tests/test_systems_api.py` and `build/server`.
The real test driver is `tests/test_systems.py`. The build created `build/server`.

| Command | Result |
| --- | --- |
| `git status --short`, `git branch --show-current` | The sandbox reported unsafe ownership. No repository state changed. |
| Git with temporary safe-directory settings inside the sandbox | The same ownership error remained. |
| `git -c safe.directory=<worktree> status --short` outside the sandbox | Passed. The starting worktree was clean. |
| `git -c safe.directory=<worktree> branch --show-current` | Confirmed `codex/systems-api`. |
| `git -c safe.directory=<worktree> show main:docs/legacy-modules.md` | Read 0.8.0, Aggressive bots and Crazy NPC pair without a merge. |
| `git -c safe.directory=<worktree> show 4c6e667:docs/plugin-api-map.md` | Read the expanded N20 row without a merge. |
| `git -c safe.directory=<worktree> show main:docs/legacy-modules.md` and `show main:docs/plugin-api-map.md` | Read the full replacement rules, generality, N rows and current N20 row. |
| `git -c safe.directory=<worktree> show 1b0c645:docs/legacy-modules.md` | Read the original-client rule. No protocol file changed. |
| `cmd /d /c tools\build_hook.bat` | Initial normal build passed. It built the server, launcher and test targets. |
| `call <vcvars32.bat>` then `cmake --build build\server --target sv-systems-test sv-systems-fixture` | Passed after each final test revision. Builds ran one at a time. |
| `ctest --test-dir build/server -R systems-api --output-on-failure` | Passed after the base API, expanded API and corrected host-command checks. |
| `python -I build/prove_ai.py` | Initial context removal did not compile because `owner` became unused. This attempt proves nothing. |
| Corrected `python -I build/prove_ai.py` | The first run exposed an ineffective host-command assertion. The test passed after a road-bound removal. This attempt proves nothing for that guard. |
| Repeated `python -I build/prove_ai.py` after the command-result fix | Fifteen guards compiled and failed the test. The gun-index case still passed because another parent check refused it. |
| `python -I build/prove_ai.py gun-index gun-type magazine-index` | All three compiled and failed the test after the malformed gun case also preserved the parent relation. |
| Restored build and `systems-api` inside the proof script | Passed. The script restored exact source bytes and recorded a Git diff after each removal. |
| `tools\build_hook.bat` after the final source revision | Passed. The normal server, launcher and all test targets built. |
| `ctest --test-dir build/server --output-on-failure` inside the sandbox | Seven tests passed. Package contents failed because Git metadata was outside the sandbox. |
| The same full CTest command outside the sandbox with a temporary `safe.directory` environment | All eight tests passed in 57.60 seconds. No global Git setting changed. |
| `python -I tools/re/gen_storage.py --check` | Passed. Checked 138 storage objects. |
| `python -I tools/ci/check_native.py` | Passed. Checked 82 native server sources. |
| `python -I tools/ci/check_repo.py` and `git diff --check` | Passed. Checked 307 files and found no private files or whitespace errors. |
| `python <asd-ste100>/scripts/ste-lint.py --fail-over 2.5 build/ai-doc-draft.md` | Passed for the contract draft. The final report receives a separate lint run. |

The final 18 removal cases cover callback context, bot permission, snapshot size, threat count,
foot route count, both body-bound paths, input size, street route count, road/intersection capacities,
road index, road endpoint, vehicle index, passenger seat range, gun index, gun type and magazine index.
Each final case compiled before its test failed.
The proof scripts and logs stay in ignored `build/` paths.
They contain no copied game assets or third-party code.

### Limits and decisions

The original client was not tested.
The test does not prove live combat, full driving through intersections or the complete Crazy NPC pair module.
It does not test a second plugin taking a lease, automatic seat exit or the perception change inside a live core scan.
There is no new privacy or persistent-data operation in this area.
The new integer arithmetic uses bounded counts and indices. No integer balance or reward arithmetic exists here.
The 100 percent shot test proves no random hesitation and no random-state consumption at the helper boundary.
The production fire path still needs its core call.

The core owner must decide when to install the eight integration steps above.
The user owns the vehicle exit and seat-write API.
After those changes, repeat the systems test with the real core call sites and test the original client.

## For OverRosa

- No implemented N20 method requires a new protocol event, field, model, sound or menu.
  Control uses existing player inputs and the game's existing human, weapon and vehicle updates.
  The original client was not tested, so this is a source-based compatibility assessment.
- Automatic seat exit and the transition to foot combat remain unbuilt in this branch.
  The shared `human_leavevehicle` function and `vehicle_exit` API exist only on `main`.
  The branch cannot merge `main` or edit their owned files.
  This is a core integration dependency, not a known original-client limitation.
- Production wiring for the AI controls remains unbuilt in the four core-owned files listed above.
  The fixture calls the seams explicitly. A plugin cannot substitute for that integration.
  This also needs core work, not a new client protocol.

The API adds no module name, price, phone number or module-specific item.
Its static control records hold host leases and routes. They are not plugin feature counters.
A plugin must keep its feature counters in N18 storage.
