# Admin Commands

Working ID: `sv.admcmd`. Version: `0.4.0`. This native plugin starts enabled.
The manifest holds the ID, display name and version. The host uses that ID for its storage namespace.
The plugin needs read, write, properties, configuration and plugin services.
It uses only the public API. It needs no game assets or internal record layouts.

## Permissions and targets

Each command checks `admcmd.NAME`. Aliases use the node of their main command.
For example, `/imortal` needs `admcmd.god`, and `/car` needs `admcmd.vehicle`.
With Permission System active, the node decides. Without it, only a player with the admin flag can use a command.
A malformed or incompatible permission service refuses access. It does not select the fallback.
The local console can use commands with an explicit target.
`/give admin` needs the permission service and its assignment rules too.

A target accepts `id:N` (the slot of the connection, which changes at each join), a plain number, an exact name, a name prefix or contained text, in that order.
A plain number is the account number of a player who is online now. It stays the same across sessions. It is the number that the game shows in `/players` and takes in `/give` and `/ban`. It is not the account ID of the client, which stays private.
A number never falls back to a slot. If no online player has that account, the command answers `Account N is not online.` and changes nothing.
A bot has no account. Reach a bot with `id:N` or by name.
The commands of the game (`/kick`, `/ban1`, `/give N`) keep their own slot rules. This plugin does not change them.
This replaces the rule of version 0.1.0, where a plain number was a slot.
Names ignore case. Quotes permit spaces. An ambiguous name returns the matching names.
For commands with a value, put the target after that value: `/arm 2 Alex`, `/team 1 Alex`.
Without an optional target, the command affects its caller.
Bots can receive body, inventory and team changes. They cannot receive money or an account group.
A node permits the command for its supported targets. Give that node only to trusted groups.
The ban plugin has separate rules for the level of a target.

## Commands

| Command | Effect |
| --- | --- |
| /god [PLAYER], /imortal [PLAYER] | Toggle health 100 each tick, with no god mode flag |
| /heal [PLAYER\|all] | Set health 100, bleeding 0 and stun 0. An unreleased body can recover |
| /kill [PLAYER] | Set health -5 and bleeding 1 |
| /arm [N\|NAME] [PLAYER] | Give a weapon in a free hand, or else in a pocket, with one loaded magazine and three spare magazines. N 1 to 6, default 2 (M-16) |
| /item N\|NAME [PLAYER] | Give item type 0 to 25 in a free hand, or else in a pocket |
| /vehicle NAME [color=C] [enter] [dist=N] [yaw=N] [PLAYER], /car ... | Make vehicle type 0 to 11 in front of the target's head. `color=` is 0 to 5, `enter` seats the target in the driver seat, `dist=` is 2 to 30 units (default 4) and `yaw=` turns the car from -180 to 180 degrees (default 0). Put the modifiers in any order. The one word that is left is the player |
| /team N\|NAME [PLAYER] | Set team 0 to 5 |
| /heavy [PLAYER], /heavydude [PLAYER] | Multiply the 15 bone masses by 100000, once per body |
| /tp | Move the caller to the aim hit, at most 2000 units away and one unit above it |
| /tp B, /goto B | Move the caller beside player B |
| /tp A B | Move player A beside player B. The thing that moves comes first, as in Minecraft. Version 0.1.0 took the destination first |
| /tp X Y Z | Move the caller to the place |
| /tp A X Y Z | Move player A to the place |
| /bring PLAYER [DEST] | Move the player beside DEST, or beside the caller when DEST is absent |
| /tpall [DEST] | Move other players beside DEST or the caller. Report skipped bodies |
| /freeze PLAYER, /unfreeze PLAYER | Hold or release the body's root position each tick |
| /npc [PLAYER] | Queue one bot three units in front of the target's head |
| /sound N\|NAME [PLAYER] | Play sound 0 to 46 at the target's aim hit, with volume 1 and pitch 0.15 |
| /give money PLAYER N | Add 1 to 100000000 to the player's account and active balance |
| /give admin PLAYER | Queue the account group from the admin_group setting |
| /turn ACCOUNT [PLAYER] | Copy the saved name and three look selectors. Keep other player data |
| /herobrine [PLAYER] | Set look selector 2 to 3 |
| /players [all] | List real players as `acc N  id:S  name [group]`. A bot has no account. `all` adds the bots. The last line counts the hidden bots |
| /who [all] | The same list without the groups |
| /id PLAYER | Show the account number, the slot and the group of a matching player |
| /where [PLAYER] | Show the body's coordinates in the form that `/tp` takes, for example `Alex: 120.5 6.0 -48.2` |
| /names KIND | List the numbers and names that a command accepts. KIND is `weapon`, `item`, `vehicle`, `team` or `sound` |
| /say TEXT | Send a server message to everybody |
| /round reset | Queue the round reset |
| /clear [PLAYER] | Send 20 blank private lines to the target or caller |
| /delh [PLAYER] | End the body of the player, or of the human on the aim line within 100 units. A bot goes away with its body. Never the caller |
| /help, /bmhelp | With [Patchouli](plugin-patchouli.md) active, it lists these commands by permission. Without it, the plugin shows a short list itself |

All commands except heal need a living body when their action uses one.
A place of `/tp` is X, Y and Z. A value can be `~` (the current value) or `~N` (the current value plus N). Each value must be from -32768 to 32768.
The game does not check walls at the place, so the place can be inside a wall.
A teleport takes a seated target out of its vehicle first, as freeze does.
Inventory commands never remove or replace an item that the target holds.
`/arm` and `/item` use a free hand. If both hands are full, the item goes to a pocket. If no slot takes it, nothing is given.
`/arm` gives the parts that fit. The spare magazines go to the pockets even when the weapon is in a pocket. A full pocket skips only its magazines, and the answer says how many it skipped.
A failed weapon removes the items that the command made.
The completed items, vehicles and bots stay after the plugin stops.

## Weapons

| N | Weapon | Game item type |
| ---: | --- | ---: |
| 1 | AK-47 | 0 |
| 2 | M-16 | 2 |
| 3 | Magnum | 4 |
| 4 | MP5 | 6 |
| 5 | Uzi | 8 |
| 6 | 9mm | 10 |

Arm loads one magazine into the weapon and puts up to three spare magazines in the pockets.
`/arm` with no number gives the M-16, as the base command does. Use `/item` for a bandage.
With this plugin active, its `/arm` replaces the inherited core command.
Item numbers are game types. Type 24 is a key in this server, despite the knife name in the legacy catalog.
The original client supplies the existing models and sounds.

## Names

Every number of `/arm`, `/item`, `/car`, `/team` and `/sound` also takes a name. The number still works.
A name ignores case. The command tries the whole name first, then a prefix, then a part inside the name.
Two different values that match give no change. The answer lists up to eight names, for example `"m" matches several: m16, magnum, mp5. Be more exact.`
Two names for the same value, such as `ak47` and `ak`, are not a clash.
`/names KIND` lists them, in pages when [Patchouli](plugin-patchouli.md) is active.

| Command | Names |
| --- | --- |
| /arm | `ak47` (`ak-47`, `ak`), `m16` (`m-16`), `magnum`, `mp5`, `uzi`, `9mm` |
| /car | `towncar`, `towncar2`, `metro`, `limo`, `turbo`, `turbos`, `beamer`, `van`, `minivan`, `truck`, `trailer`, `heli` (`helicopter`) |
| /team | `goldmen`, `monsota`, `oxs`, `nexaco`, `pentacom`. Team 5 has no name in the game data |
| /item | The names of the 26 item types, in lowercase with hyphens, for example `bandage` and `black-disk` |
| /sound | The names of the sounds that make a noise, for example `engine`, `ping`, `thud` and `ring` |

The six colors of a car (`color=0` to `color=5`) have no name. The game data and the code do not say how they look.
`/car` cannot set the health of a car or its paint pattern. The public API has no call for them.

## Answers and notices

A command that works says what it did, with names and values: `Healed Alex.`, `Teleported Alex to Bob.`, `Made a turbo (color 2) in front of Alex. Alex is in the driver seat.`
A command that fails says why and what to do, in plain words, and then shows its form: `Use: /tp [PLAYER] PLAYER|X Y Z`.
Raw result names stay out of the chat. All the texts are in one table near the end of `admcmd.c`.

When another person uses a command on you, you get a short notice, for example `Admin healed you.`
The setting `notify_target` controls this and defaults to 1. A bot and the caller never get a notice. The console sends none.

## The log

If the [Logging System](plugin-logging.md) is active, this plugin records every failure there. It also records these commands when they work: `/heavy`, `/give`, `/round`, `/kill`, `/tpall` and `/delh`.
The line has a fixed form and holds the caller, the target and the result with its number.
Without that plugin, nothing is recorded and the commands work the same.

## Settings and lifetime

The setting `admin_group` defaults to admin. It applies live.
The setting `notify_target` defaults to 1. It applies live.
The permission service checks `permissions.assign`, target authority, owner rules and the account before it accepts the change.
It saves the assignment on its next tick. A failed save gives a private message.
With that service absent, `/give admin` reports that it cannot find the service.

Immortality and freeze end on body change, departure, round reset or plugin stop.
Freeze restores the root position and sets all 15 bone velocities to zero at each plugin tick.
It does not stop the simulation between those ticks. Damage and client movement prediction still run.
A death ends freeze. Heavy changes stay for that body's life, including after plugin stop.
Heavy accepts starting masses from 0.5 to 500. A second request for the same body fails.
The public mass range now reaches 50000000, so every accepted heavy result fits.

## Unsupported commands and checks

The public API does not expose company base positions, so `/gobase` remains open.
The train system owns vehicle types 12 and 13, so vehicle creation stops at 11.
`/deactivate` has no supported game effect.
These commands are absent from this plugin.

The SDK scenarios check every command, all six weapons, the loaded and spare magazines, partial gifts and optional targets.
They also check bots, partial names, bad targets, missing nodes, numeric bounds and permission fallback.
Native contract tests check API sizes, incompatible services, command text bounds, the heavy input range, every name table and the modifiers of `/car`.
See `plugin-official-progress.md` for test results and removal proofs.
No original client playtest or manual launcher session ran.
