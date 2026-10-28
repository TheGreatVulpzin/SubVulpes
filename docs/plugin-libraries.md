# Plugin libraries, conflicts and owned things

This page describes four host rules: the API that a plugin offers to other plugins (N35), the `conflicts=` key and the reason of a refusal (N36), the cleanup of what a plugin made (N37), and library plugins (N38).
The host tests prove all four with real DLLs. No game test shows them in a client, because they do not change the game.
See [the plugin SDK](plugin-sdk.md) for the manifest and [the game API](plugin-game-api.md) for the write table.

## Manifest keys

| Key | Value | Rule |
| --- | --- | --- |
| `dependencies` | IDs, separated by commas | Each ID can end with `@VERSION`, such as `sv.lib@1.2`. At most 8. |
| `conflicts` | IDs, separated by commas | No version. At most 8. No repeat, no own ID. |
| `role` | `plugin` (the default) or `library` | A library has no game behavior. |

A plugin cannot depend on a plugin that it conflicts with. The host refuses such a manifest. The installer refuses it too.

## The version rule

The version of a package is the `version` key of its manifest. The host reads the groups of digits at the start: `1.2.0` has the groups 1, 2 and 0. A missing group is 0. Text after the groups, such as `-beta`, does not count.
`dependencies=sv.lib@1.2` means that the package version of `sv.lib` must be 1.2 or higher. The versions 1.2.0, 1.2.5 and 2.0 pass. The version 1.1.9 fails.
If the version is too low, the plugin does not start. The reason is `A dependency has a lower version than the plugin needs.`
The version of the package is not the version of an API. An API has its own number, and the number must be equal. See below.

## Conflicts

Two plugins that name each other, from either side, cannot be active at the same time.

- At the start, the plugin that comes first in `plugins.ini` wins. The other one fails with the reason `Conflicts with active plugin ID.`
- A live enable that would conflict returns `SV_CONFLICT`. The host does not save the state, so a restart does not change the winner.
- The offline command `--plugin-enable` refuses a conflict with a plugin that the profile selects.
- The offline list shows the later plugin as `failed`, with the reason `Conflicts with plugin ID, which starts first.`

A plugin that you want in place of the other one needs two steps: disable the active plugin, then enable the new one.

## A refusal with a reason

A plugin can say why it does not start. Get the table `SvPlugins` (`SV_API_PLUGINS`, header `sv_plugins_api.h`), call `refuse` inside the enable callback, then return a result other than `SV_OK`.

```c
const void *table;
host->get_api(owner, SV_API_PLUGINS, SV_PLUGINS_VERSION, &table);
((const SvPlugins *)table)->refuse(owner, "needs a newer server");
return SV_FAILED;
```

The log, the plugin list and the launcher show `Refused to start: needs a newer server`. The text has 1 to 95 printable ASCII characters. It cannot hold a tab or a line break, because the launcher reads a list separated by tabs.
Without a reason, the host keeps its general text `Enable callback reported failure. Registrations removed.`

## An API between plugins

A plugin offers an API with `provide`, inside its enable callback. A plugin that depends on the first one finds it with `find`.

| Call | Rule |
| --- | --- |
| `provide(owner, name, version, table)` | The name is the plugin ID, or the ID, a dot and a suffix. A name of another plugin returns `SV_DENIED`. The table starts with two `uint32_t` fields, `size` and `version`, and the version must equal the version in the call. At most 8 names for a plugin and 64 in the host. |
| `find(owner, name, version, &table)` | The caller must list the offering plugin in `dependencies`, or `SV_DEPENDENCY`. An unknown name or a plugin that is not active returns `SV_NOT_FOUND`. A different version returns `SV_INCOMPATIBLE`. |

The host never reads the table. It passes the pointer. A dependency makes the offering plugin load first and stop last, so the pointer stays valid while the caller runs.
When a plugin stops, the host removes its names. A plugin that depends on it stops first.

A function of the table runs on the stack of the caller, with the owner of the caller as the current owner. The offering plugin must take `SvOwner caller` as the first argument of every function that needs the host, and must use only that owner in host calls. A call with the owner of the library itself returns `SV_STATE`.

The version of an API is a number for the contract. Change it when you change the layout of the table or the meaning of a function. A caller that asks for an old number gets `SV_INCOMPATIBLE`, and it can show a clear message. Keep the old table in the library if old callers must still work.

## Library plugins

A manifest with `role=library` marks a plugin that only offers an API. The host enforces this:

- `subscribe` with a mask other than 0 returns `SV_DENIED`.
- `command` returns `SV_DENIED`.
- `get_api` returns `SV_DENIED` for every capability except the settings (`SV_API_CONFIG`) and the plugin services (`SV_API_PLUGINS`).

A library can still read its own settings and log. Its functions reach the game through the tables of the plugin that calls them, so the actions and the permissions belong to that plugin. A library cannot do more than its callers.
A library is a normal plugin in every other way: it needs approval of its DLL, it can be `required` or `enabled`, and a plugin that needs it must list it in `dependencies`. The plugin that needs a library does not start if the library is missing, disabled or too old.

## Things that a plugin made

The host remembers the items, the vehicles and the bots that a plugin made with `item_create`, `object_create` and `npc_create`. When the plugin stops, the host ends the ones that still exist, as `item_remove`, `object_remove` and `npc_remove` do. The game removes them in its next lifetime passes.

- A plugin that wants a thing to stay calls `disown(owner, entity)` with the reference of the item, the vehicle or the player of the bot. The call returns `SV_NOT_FOUND` for a thing that the plugin did not make or that is gone.
- The table holds 2,048 entries for all plugins. When it is full, entries of things that are gone leave first. If it is still full, the three create calls return `SV_LIMIT` and make nothing.
- The cleanup runs when the plugin stops for any reason: the operator disables it, it fails, or the server stops. A reset of the game makes old references invalid, so the host ends nothing after a reset.
- A piece of the train and a player who is not a bot are never ended.
- The host does not end a thing that a plugin moved or changed but did not make.
- The rows that the plugin changed in the property table come back to their first values, as before. See [the game API](plugin-game-api.md).

## Limits

No test with a real client exists for this page. The rules do not change what a client sees.
The host stops the callers before the library, and all calls run on the server thread. A library function that blocks blocks the server, as any plugin code does.
