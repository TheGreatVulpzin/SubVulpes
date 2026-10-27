# Legacy module catalog

Status: requirements list. No module here is implemented yet.
It guides the plugin API work in [system coverage](plugin-api-map.md) and the production plugins in [the milestones](plano.md).

## Source and use

A friend of the group made a script manager for the original dedicated server, version 0.7.1.
It patches the memory of the running original server and injects native DLLs. That method does not work in SubVulpes,
because SubVulpes is a native server with no original image. The tool and its files stay outside this repository.

This list keeps the behavior that players and administrators know. It does not copy code, addresses or text from the tool.
Each plugin here is a new official plugin on the public API. The list uses version 0.7.1.
A newer version exists. Compare this list with it when that version is available, and add the changes.

A command in the chat goes to the plugins first, after the host check of the permission level.
The inherited commands in `src/net/commands.c` stay until a plugin replaces them and passes its checks.

## Chat commands

All commands need the admin flag, a living player and, where noted, a body on foot.
"Now" means the current read and write API is enough. The needs (N1 to N19) are in the next section.

| Command | Behavior to keep | Needs |
| --- | --- | --- |
| `/imortal` | Toggle. Hold the health at 100 each tick without the god mode flag, so hits and pushes still show. Ends on body change, disconnect, reset or plugin stop. | Now (tick, `set_health`), N18 |
| `/heal` | Health 100, no bleeding and no stun. The tool says it clears a death mark and a drunk level. This server has no field with those names. The drunk look is most likely the stagger of a hurt or stunned body (see N5). | `set_health` now, N5 |
| `/kill` | Kill the own body. | Now (`set_health`) |
| `/item N` | Make item type 0 to 26 and put it in a free hand. A knife (24) starts in attack state. Fail without cost when there is no free hand or item. | N6 |
| `/vehicle N` | Make vehicle type 0 to 13 four units in front of the head, color 0. Do not seat the player. | N7 |
| `/arm N` | Uniform 1 to 6, clothes variant 4, two bandages in slot 6. The inherited `/arm` stays until then. | N4, N6 |
| `/heavydude` | Multiply the 15 physical values of the body by 100000, once. A joke command. | N19 |
| `/delh` | Delete the nearest human on the aim line, up to 100 units, never the caller. | N10, N8 |
| `/team N` | Set team 0 to 5 and send the update. | N3 |
| `/deactivate ID` | Toggle the active state of a player slot 0 to 255. The meaning needs analysis first. | Unknown |
| `/give admin ID` | An admin gives the admin flag to a connected player. Reject empty slots and bots. Lasts until disconnect. The inherited `/give` is a different command (cash). | N2, N1 |
| `/npc` | Make a pedestrian bot 3 meters in front, with no car. Queue the request to a safe point and refuse a second request while one waits. | N8 |
| `/bmhelp` | Show the help to the caller only. | N1 |
| `/turn N` | Copy name, eyes, skin and hair from saved account N to the caller. Copy no money, identity, team or rights. | N4 (read exists) |
| `/herobrine` | Set the eyes to type 3. | N4 |
| `/tp` | Teleport to the surface on the aim line, up to 2000 units, 1 unit above it. Fail if the aim hits nothing. | N10, `teleport` now |
| `/gobase N` | Teleport to base N (5 bases). Read the positions from the level data, not from fixed numbers. | Level read, `teleport` now |
| `/sound N` | Play sound 0 to 47 at the aim point, volume 1, pitch 0.15. | N10, N11 |

The tool lists `/nick` in a table but never runs it. Do not build it.

## Modules

| Module | Behavior to keep | Needs |
| --- | --- | --- |
| Black market | Two phones that sell a knife (type 24) and a bandage (type 12) for set prices and waits. A buy needs a living player near the phone. Take the money only after the item exists. | N6, N15, N16 |
| Knife attack | Damage 25, range 1.5, wait 20 ticks, in the active hand, with a push on the hit body. | N10, N12, `set_health` now |
| Chat in console | Log the name and text, with the id as an option. Limit the text to 63 bytes and strip control bytes. | N16 |
| Maximum players | Set the active player limit from 1 to 32. Do not kick anyone. | N14 |
| NPC count | Set the number of bots that the start makes. Do not add or remove now. | N14 |
| Starting money | Set the money of a new account. Do not change existing balances. | N14 |
| Faster truck | Multiply speed range and engine force of trucks (model 9) by a factor of 1 to 4. Apply to existing and new trucks. | N17 |
| Truck in the dealership | Keep one truck in each dealership catalog for a set price. Replace the last offer when full. Make no truck until bought. | N15 |
| Death messages | One message of at most 63 bytes for each death. Check the message index. | N16 |
| Instant death | Run the death processing at once. The game keeps the penalties and the body split. | N16 |
| Eject from vehicle | When a crash damages the vehicle, eject the occupants and zero the speed of the body parts. Model 11 is exempt. | N9, N16 |
| Punch | Damage from the relative speed of the fist, a head bonus and a push. Defaults: scale 100, head 5, push 8.5, threshold 3, wait 70. | N12, `set_health` now, N16 |
| Spawn protection fix | Cancel the spawn protection when the body moves faster than a set speed. | N13 |
| Safe spawn area | A zone where spawn protection applies. | N13, N16 |
| Spawn at the dealership | Spawn at a set position. | N16 |
| Aggressive bots, clunky ragdolls, critical health | The tool gives no description. Read the newer version before work. | Unknown |

## Needs

| Id | Operation or event |
| --- | --- |
| N1 | Send a message to one player. Done: `message_to`. The server filters the event for each connection, so the original client gets a normal message. |
| N2 | Set the admin flag of a player and of its connection, as the password login does. Done: `set_admin`. |
| N3 | Set the team of a player and send the update. Done: `set_team`. |
| N4 | Write the look fields and colors of a player or human and send the update. Done for the five look selectors: `set_looks`. The colors are not done. |
| N5 | Write the `bleeding` flag and the `stun_timer` of a human. Both fields exist and the read API already shows them. Done: `set_bleeding` and `set_stun`. |

About the "drunk" look. This server has no drunk field. A body staggers in two cases, and both come from the code:

- A hit sets `stun_timer` to 3 or 6 times the damage (`bullet_sim.c`). It counts down by 1 each tick and weakens the muscles of the torso while it runs.
- Below 25 health, `body_strength` drops and wobbles (`human_strength` in `humans.c`). The game recalculates it every tick from the health.

So a heal needs health 100 and `stun_timer` 0. `body_strength` follows by itself. This is read from the code, not tested.
The same two effects give a way to make a body stagger on purpose: set a high `stun_timer`. That needs a write for the timer, with an upper bound.
The "death mark" of the tool is probably not a field. It is likely a state that existing game systems already use. In the code, death works like this (`humans.c`):

- Health below 1: the body drops the first item of slots 0 to 6.
- Health below -4: the body is dead. `despawn_timer` counts down from 3600. Below 2700 the game releases the player (`human_releaseplayer`): the death tax, the loss of team managers in gametype 4, `human_id` of the player set to -1 and `player_id` of the body set to -1.
- While health is -4 or more and a player holds the body, the game sets `despawn_timer` back to 3600 on each tick.
- In a building of type 6 (a hospital) a body with a player gains 1 health each 32 ticks and stops bleeding.

So health 100 revives a body that is dead for less than 900 ticks (15 seconds), with no new write. After the release the body has no player, and a heal does not give it back. The player must respawn.
This reading is from the code and not tested. The newer version of the tool can confirm what its `/heal` clears.
| N6 | Make, remove, link and unlink items, with the checks that keep both sides of a link whole. Done: `item_create`, `item_remove`, `item_equip`, `item_attach` and `item_detach`. |
| N7 | Make and remove vehicles and other objects. Done: `object_create` and `object_remove`. Train pieces are refused. |
| N8 | Make and delete a human, and make a bot player with a human at a safe point. The pool guards and rollback exist. Done for bots: `npc_create` and `npc_remove`. Not done: delete the body of a real player. Kill it with `set_health` instead. |
| N9 | Eject a human from a vehicle. |
| N10 | Aim and line queries against the level, objects and humans. They copy the result out of the shared scratch record. |
| N11 | Play a known sound at a position. |
| N12 | Add a push or speed to a body part. |
| N13 | Write the spawn protection timer. |
| N14 | Settings that the core owns: player limit, starting money and bot count. |
| N15 | Shop catalog entries and phone purchase events. |
| N16 | Events: chat text (observe), human death (observe and early processing), spawn (decide position), vehicle damage (observe), purchase (decide). |
| N17 | Engine parameters of one object type. |
| N18 | Plugin storage with a size bound, in three scopes: session (one player, cleared when the player leaves or the plugin stops), account (kept on disk, tied to an account) and server (kept on disk). See the notes below. |

Plugin storage (N18) does not exist yet. A native plugin can write its own files, but the host does not bound or clean them.
The design rules:

- Do not add fields to the account record. Its file format is fixed (0x4c bytes for each record), and old account files must keep working. The player record is gone when the player leaves.
- Keep the data in a private plugin folder that the installer already keeps during updates. The data stays after a disable and goes only when the administrator asks.
- The key of the account scope is an opaque reference. The host maps it to the real account id. The plugin never sees the id, as in the read API.
- Each plugin has its own keys, a limit for each value and a limit for the total. The host saves at a safe point and in one step, so a crash leaves the old file whole.
- Persistent bans, permission groups and ranks depend on the account scope.
| N19 | Write the body part masses. Low priority. The effect is not known. |
| N23 | Plugin configuration set by the administrator, in the launcher or with a command. See the notes below. |

Plugin configuration (N23) does not exist yet. N14 is different: it gives plugins the settings of the core, such as the player limit.
N23 is the settings of one plugin, such as the price of the knife.

The difference from storage (N18):

| | Configuration (N23) | Storage (N18) |
| --- | --- | --- |
| Who writes | The administrator | The plugin |
| Shape | Keys that the plugin declares, with a type and a range | Any bytes, within a size limit |
| Check | The host checks each value before the plugin sees it | The plugin checks its own data |
| Shown to the user | Yes, in the launcher and by command | No |
| On update | Kept for the keys that remain | Kept whole |

The design rules:

- **Declaration.** The plugin lists its settings in `plugin.ini`: key, type, default, minimum, maximum, label and help text.
  The host must read them without loading the DLL, because the DLL stays unloaded until the administrator approves it.
- **Types.** A flag, an integer, a float, a bounded text, a choice from a list, and a secret. The old script manager had the same idea: each parameter had a label, a type, a default and a range.
- **Setting a value.** The local operator uses `plugins config <id>` to list and `plugins config <id> <key> <value>` to set, as with `plugins enable`.
  The launcher builds a form on its Plugins page from the declaration. It sends the command to a running server on standard input, or writes the file when the server is stopped.
  A player who is an admin may set a key only when the plugin marks it as editable by admins.
- **Check and save.** The host refuses a value outside the declaration and keeps the old value. It saves to a private file in the folder that updates keep.
- **Applying.** A key is live or needs a restart. A live key gives the plugin a change event with the old and the new value.
- **Updates.** A key that stays keeps its value. A new key takes its default. A removed key is dropped. A key that changes type goes back to its default.
- **Secrets.** The host never writes a secret to a log or shows it again after it is saved. The launcher already hides passwords in its log.
- **Tests.** Every type, every error, a restart that keeps the values, and an update that adds, removes and retypes a key.
  Removal proofs for the range check and for the hiding of secrets.

## Order

1. N1 to N4, N6 to N8, N10 and N11 first. They serve the chat commands, which one official administration plugin can own.
2. N16 and N12 next. They serve the death, punch, knife and eject modules.
3. N14, N15 and N17 last. They need changes to core data that the API does not reach yet.

Each operation follows the rules in [system coverage](plugin-api-map.md#required-catalog-before-implementation):
a documented phase, bounds, failure result and a focused test that fails when the check is removed.
The original client limits stay explicit. A new model, sound or menu needs OverRosa.

## Bans

What the server has today (from the code):

- **IP list.** `serverrecv` compares the sender address of every packet with `sr_kickedip` (up to 41,368 addresses, one scan for each packet). A match drops the packet with no reply. The list is in memory only. `/clearkick` empties it and a restart loses it. The `kick` method adds to it.
- **Account ban.** `Account.ban_time` above 0 drops the player at the join and during play. The count falls by 1 each 36,000 ticks of the world clock.
- **No persistence.** `saveaccountsserver` writes the id, name, money, play time and three look values. It does not write `ban_time`. A restart ends every ban. (Read from the code, not run.)

Ideas for an official ban plugin. They need plugin storage (N18) and a decision event at the join (N16):

1. Bans kept on disk, with reason, author, date and end date.
2. Ranges of addresses (a base address and a mask) instead of single addresses. The core list scans one by one, so the plugin keeps its own sorted table and the core only gets a yes or no.
3. A strike system with longer bans each time, and a list of exceptions.
4. A limit of join attempts for each address and each minute, with an automatic short ban.
5. Link an address with an account to find a banned player on a new account. This stores an address history, which is personal data.
6. A shadow mute: the muted player sees their own chat and nobody else does. The per-connection filter and the own-text rule of the packet writer already allow this.

Limits:

- The addresses are IPv4 only (32 bits).
- Players behind one shared address (NAT) can be banned together. A VPN avoids an address ban.
- The protocol has no hardware id. A client token would need OverRosa.
- Stored addresses are personal data. Keep them in the private plugin folder and show them masked in admin output. Never put them in logs that leave the machine or in the repository.
