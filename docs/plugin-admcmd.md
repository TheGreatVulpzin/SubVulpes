# Admin Commands

Working ID: `sv.admcmd`. Version: `0.2.0`. This native plugin starts enabled.
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
| /arm N [PLAYER] | Give one weapon in a free hand |
| /kit N [PLAYER] | Give the weapon, four magazines and two bandages |
| /item N [PLAYER] | Give item type 0 to 25 in a free hand |
| /vehicle N [PLAYER], /car N [PLAYER] | Make vehicle type 0 to 11 four units in front of the target's head |
| /team N [PLAYER] | Set team 0 to 5 |
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
| /sound N [PLAYER] | Play sound 0 to 46 at the target's aim hit, with volume 1 and pitch 0.15 |
| /give money PLAYER N | Add 1 to 100000000 to the player's account and active balance |
| /give admin PLAYER | Queue the account group from the admin_group setting |
| /turn ACCOUNT [PLAYER] | Copy the saved name and three look selectors. Keep other player data |
| /herobrine [PLAYER] | Set look selector 2 to 3 |
| /players [all] | List real players as `acc N  id:S  name [group]`. A bot has no account. `all` adds the bots. The last line counts the hidden bots |
| /who [all] | The same list without the groups |
| /id PLAYER | Show the account number, the slot and the group of a matching player |
| /where [PLAYER] | Show the body's coordinates in the form that `/tp` takes, for example `Alex: 120.5 6.0 -48.2` |
| /say TEXT | Send a server message to everybody |
| /round reset | Queue the round reset |
| /clear [PLAYER] | Send 20 blank private lines to the target or caller |
| /delh [PLAYER] | End the body of the player, or of the human on the aim line within 100 units. A bot goes away with its body. Never the caller |
| /help, /bmhelp | With [Patchouli](plugin-patchouli.md) active, it lists these commands by permission. Without it, the plugin shows a short list itself |

All commands except heal need a living body when their action uses one.
A place of `/tp` is X, Y and Z. A value can be `~` (the current value) or `~N` (the current value plus N). Each value must be from -32768 to 32768.
The game does not check walls at the place, so the place can be inside a wall.
A teleport takes a seated target out of its vehicle first, as freeze does.
Inventory commands keep existing items. A full hand or pocket refuses the request.
A failed kit removes its new items and keeps the previous inventory.
The completed items, vehicles and bots stay after the plugin stops.

## Weapons and kits

| N | Weapon | Game item type |
| ---: | --- | ---: |
| 1 | AK-47 | 0 |
| 2 | M-16 | 2 |
| 3 | Magnum | 4 |
| 4 | MP5 | 6 |
| 5 | Uzi | 8 |
| 6 | 9mm | 10 |

Arm gives the weapon alone. Kit loads one magazine into it and puts three spare magazines in pockets 3 and 4.
The two bandages use pocket 6. Kit does not replace an occupied hand or pocket.
With this plugin active, its `/arm` replaces the inherited core command.
Item numbers are game types. Type 24 is a key in this server, despite the knife name in the legacy catalog.
The original client supplies the existing models and sounds.

## Settings and lifetime

The setting `admin_group` defaults to admin. It applies live.
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

The SDK scenarios check every command, all six weapons, the kit contents and optional targets.
They also check bots, partial names, bad targets, missing nodes, numeric bounds and permission fallback.
Native contract tests check API sizes, incompatible services, command text bounds and the heavy input range.
See `plugin-official-progress.md` for test results and removal proofs.
No original client playtest or manual launcher session ran.
