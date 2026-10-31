# Permission System

Working ID: `sv.permissions`. Version: `0.1.0`. This native plugin starts enabled.
The manifest holds its ID, display name and version. CMake supplies those values to its entry point.
The host uses that ID for its private storage folder. The public service name is `SV_PERMISSIONS_API` in `sv_permissions.h`.
The plugin needs read, write, properties, storage, configuration and plugin services. It does not request network access.
It has commands and events, so its manifest does not use `role=library`.

## Groups and nodes

| Group | Level | Parent | Main nodes |
| --- | ---: | --- | --- |
| guest | 0 | none | permissions.view, chatadd.msg, admcmd.help |
| member | 10 | guest | Inherits guest |
| moderator | 50 | member | core.kick, core.list, core.message, bans.ban, bans.kick, bans.view, chatadd.mute and player inspection commands |
| admin | 90 | moderator | core.*, admcmd.*, bans.*, chatadd.*, permissions.*, auditlogs.view |
| owner | 100 | admin | All nodes |

A node uses lowercase dotted words. `admcmd.*` covers the words below that prefix.
A negative node, such as `-core.give`, removes an inherited grant.
The nearest group with a matching rule decides. Within that group, a negative rule wins.
A group can inherit one existing parent. The loader rejects missing parents and cycles.
A group has a name, level, optional prefix and at most 20 local nodes.
A new group takes its parent's level. Its administrator can then set another level within their authority.

An account keeps its group in account storage. It follows the account across sessions and host restarts.
An account with no stored group uses `default_group`. A player with no account uses that default too.
A deleted or unknown group falls back to the default. An unknown default setting falls back to guest.
The plugin sets the player and connection admin flags from `core.admin`.
The default moderator has no admin flag. N56 still permits its three core command nodes.
The core sends that command's admin replies privately to the player as normal message events.

## Commands

Use `/perm` or `/pm`. Use `/group` or `/grp`.
A target accepts a current player slot, an exact name, a prefix or a contained word, in that order.
Names ignore case. An ambiguous target returns a list. Quotes permit a name with spaces.

| Command | Node |
| --- | --- |
| /whoami | Public. Shows the caller's group and main nodes |
| /perm who | permissions.view |
| /perm check PLAYER NODE | permissions.view |
| /perm set PLAYER GROUP | permissions.assign |
| /group list | permissions.view |
| /group show NAME | permissions.view |
| /group create NAME [PARENT] | permissions.group. The default parent is guest |
| /group delete NAME | permissions.group |
| /group allow NAME NODE | permissions.group |
| /group deny NAME NODE | permissions.group |
| /group remove NAME NODE | permissions.group |
| /group prefix NAME TEXT | permissions.group. Empty text clears the prefix |
| /group level NAME N | permissions.group |

Allow and deny replace the opposite local rule for the same node. Remove clears that local rule.
The caller cannot assign a higher group or change a target above their own level.
The caller cannot lower their own group level or grant a node they do not hold.
The local console can manage groups without a player. Bots cannot receive an account group.
No caller can delete a default group. A group with a child cannot be deleted.
Only an owner can change the owner group. The local console can repair it too.
The owner level stays 100. Owners always retain permission management and `core.admin`, so they can repair their rules.
The last stored owner cannot be demoted.

## Password login and recovery

With `password_login=1`, a correct core password selects `login_group`. Its default is admin.
An existing owner keeps owner. The core still checks the password and its attempt limit.
With `password_login=0`, the plugin denies the attempt before the counter and sends `Password login is off.` privately.
Neither hook receives the password.
The login group lasts until departure. With `save_login_group=1`, it goes into account storage instead.

If no account is an owner, a successful password login also permits `/perm set` for that session.
This recovery rule permits the first owner assignment even if the login group lacks permission nodes or has a lower level.
Once an account becomes owner, recovery ends for every session.
The local console can also assign the first owner with `perm set PLAYER owner`.
Keep the core password and console access available for recovery.

## Settings

| Key | Default | Effect |
| --- | --- | --- |
| default_group | guest | Group with no account assignment |
| login_group | admin | Group after a correct password |
| password_login | 1 | Permit or deny the core login attempt |
| gate_core_commands | 1 | Decide core commands from core.NAME |
| save_login_group | 0 | Save the login group to the account |

All five settings apply live. Names that do not exist use the safe fallback stated above.
With the core gate on, a matching node gives allow. A missing node gives deny and a private permission message.
With the gate off, the plugin returns default. The inherited admin flag rule then decides.
The builtin name comes from the core prefix tests. For example, `/army` requires `core.arm`.
The list commands use `core.list`. The inherited ban commands use `core.ban`.

## Public service

Include `plugins/permissions/sv_permissions.h` and declare a dependency on `sv.permissions@0.1.0`.
Find `SV_PERMISSIONS_API` through `SvPlugins.find`, with `SV_PERMISSIONS_VERSION` (1).
Check the table and record sizes before calls.

| Method | Effect |
| --- | --- |
| can(player, node) | Returns 1 for a grant and 0 otherwise |
| group_of(player, out) | Copies the name, prefix and level into SvPermissionGroup |
| groups(out, capacity, item_size, count) | Copies the group metadata with the supplied item stride |
| assign(actor, player, group) | Checks and queues an account assignment for the next provider tick |

The reads use the provider's last callback snapshot. They make no host calls with the provider's owner from another plugin.
The provider refreshes that snapshot at joins, core commands, configuration changes and each host tick.
The assignment service uses the same node, level and owner rules as `/perm set`.
It returns a queue result first. The next provider tick saves the account and reports a failed save privately.
A second pending assignment for the same player returns `SV_CONFLICT`.
The host starts dependencies first and stops their consumers first, as for other plugin services.

## Bounds and storage

Names hold 1 to 23 lowercase letters, digits, underscores or hyphens. The first character is a letter.
A node holds at most 39 bytes, including a stored minus sign. A prefix holds at most 31 printable ASCII bytes.
A level is 0 to 100. At most 24 groups and 256 live session snapshots exist.
Each group and the group index fit in one host storage value. Values use a format number and checked counts.
The host saves persistent values atomically. A failed save leaves the current group unchanged.
The host quota is 128 entries and 64 KiB of values. Five default groups and their index leave 122 account assignments.
With 24 groups, 103 account assignments remain. Unassigned accounts need no entry.
A full store reports a failed save. It does not grant the requested group.
Malformed group data gives a clear log line and a plugin failure. It does not replace private data with defaults.
Defaults are made only when the index is missing.

## Checks and limits

The native unit test covers inheritance, wildcards, negative rules, cycles, formats, names, node sizes and levels.
The SDK scenarios load the real DLL and a separate native consumer.
They cover core decisions, group commands, account storage across restarts, session expiry, password settings and first-owner recovery.
They also cover a bot, bad targets, invalid values, owner protection and the group and node limits.
Removal proof results and the full test run belong in `plugin-official-progress.md`.
No original client or manual launcher check ran for this plugin.
