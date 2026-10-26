# Plugin system of the SubVulpes server (plan)

Status: design proposal. S3 adds the native foundation and official plugins. S4 adds Lua, WASM and the external SDK.
No plugin API or host is implemented yet. See `plano.md`.

Goal: a deep and integrated plugin system. A plugin can change almost any part of the server, not only react to a few events.

## Terms

- Plugin: code that runs in the server. It has three kinds: native (C), Lua and WASM. It works with the original client.
- Mod: assets for the client (models, textures, sounds). The server offers them and the OverRosa client downloads them. They need a client with support for custom assets (a later milestone). A mod has no code, so it has less risk than a plugin.
- Package: a folder with a `manifest` (name, version, hash) and two parts: `plugin/` and `assets/`. A package can hold a plugin, a mod or both. The manager shows which parts it has.
- A plugin can need a mod. An original client ignores the mods, so the plugin needs a fallback (for example, an item of the original game in place of a new item).
- The hash warning below is for native plugins only. A mod has no code.
- No "addon" term. If the client gets scripts later, the term can come back for client code.

## Principles

- Open everything by default. Close only what is dangerous.
- The server owns the state. A plugin asks the server to act.
- The API uses numbers, text and IDs. It never gives a pointer to a plugin. Reason: Lua and WASM cannot use a pointer, and a stale pointer crashes the server.
- One API in C is the source of truth. The other hosts sit on top of it.
- The server exposes players, humans, objects, items, buildings and economy as tables. The field names come from `analysis/structs/names.csv`.
- Plugins do not change what the client calculates alone (local physics, collision with unknown object types). A change of state is safe. A change of physics can desync the client.

## Core and official plugins

The core owns simulation, game storage, protocol parsing and unconditional memory and representation checks.
It also owns the native loader, the versioned API and the checks on operations requested through that API.
Core startup diagnostics must work before plugins load.

Administration commands, permission services, ban policy and persistence, RCON, log sinks and configurable policies belong in official plugins.
The core emits events and offers validated operations. Plugins implement the service or policy behind those interfaces.
Basic diagnostic output remains available even when no optional log sink loads.
Port inherited administration behavior in tested steps rather than replacing it all at once.

Official means shipped and maintained with SubVulpes. It does not mean compiled into the server executable.
Use separate module files and version metadata so a compatible plugin can be updated independently.
Official and third-party plugins use the same public API, without private memory patches or an internal-layout dependency.

Activation classes:

- Required: always active for the selected server profile. Missing, incompatible or disabled required modules prevent normal startup.
- Default enabled: active on a fresh configuration, but the administrator can disable the module if no required module depends on it.
- Optional: enabled when the administrator selects the feature and its dependencies are available.

The exact module catalog and required set remain design decisions. Required does not mean that every network endpoint is enabled by default.
Administration interfaces must not accept commands without their required authorization service.

## Independent module updates

A plugin package declares its version, public API requirements and module dependencies.
The manager checks those requirements before installation and activation.
A compatible plugin update replaces that module without rebuilding the core or unrelated plugins.
A plugin that needs new core capabilities must declare the newer API requirement.

The initial update path can stop the server, install the checked module, restart and run a focused check.
Preserve private configuration and persistent plugin data. Keep the prior module available for rollback.
Live unload or replacement of native code during a running match is not an initial requirement.
The current executable-only launcher updater does not yet implement plugin installation or updates.

Shimmie2 is an architecture reference: its extensions react to events and administration has a separate extension directory.
See its [extension documentation](https://github.com/shish/shimmie2/wiki/Extension-Development)
and [admin extension](https://github.com/shish/shimmie2/tree/main/ext/admin).
The activation and update contract above is the SubVulpes design, not a claim about an identical Shimmie2 implementation.

## Three kinds of plugin

| Kind | Who writes it | Speed | Safety |
| --- | --- | --- | --- |
| Native (C, DLL or `.so`) | Trusted authors | Maximum | None. Level 1 protection only (see below) |
| Lua | Anyone | About 10 to 50 times slower than C | Sandbox |
| WASM | Authors in any language that compiles to WASM (Rust, Go, Zig, C#, C) | About 1.2 to 2 times slower than C | Isolated memory and time limit |

Ease of writing, from easy to hard:

1. Lua. One text file. The author installs nothing.
2. WASM. The author chooses the language and gives one `.wasm` file. The same file runs on Windows and Linux. The author needs a compiler and the SDK.
3. Native. The server is x86 32 bit and MSVC. A native plugin needs a compatible 32-bit DLL and public API ABI.
   Internal `sr_types.h` layouts are not the public plugin contract. Each new platform needs its own native build.

Native plugins are for the core and for trusted authors. WASM is the main way to write fast plugins for third parties. Without an SDK (a template folder, the types of the API and one build command), WASM is not easier than native. Plan the SDK with the WASM host.

Design:

1. The C API holds the events (`on_chat`, `on_death`, `on_connect`, `on_disconnect`, `on_tick`) and the calls (`player_add_money`, `player_teleport`, `player_message`, `bot_set_target`...).
2. A native plugin calls the C API directly.
3. The Lua host is a native plugin. It embeds Lua, connects each event to a Lua function and gives the API as Lua functions. No `os` or `io` in the sandbox.
4. The WASM host is a native plugin. It embeds a runtime (for example wasm3 or wasmtime). The module imports the functions of the API.

Order of work:

1. S3: minimal C API, events, loader and official native plugins with separate package/update support.
2. S4: Lua host on the tested API.
3. S4: WASM host and SDK after Lua, with the same versioned API contract.

Cost: each new function of the API is work in three places. Keep the API small and stable. Plugins need an order (priority) and a way to cancel an event (for example `on_chat` returns "block").

## Cost of events

- An event for each bot or each player at each tick is expensive in Lua.
- Better: the script sets a goal now and then (every 0.5 s), and the C code does the steps.
- Use few tick events, and give them a filter.

## Protection of native plugins

Level 1 only:

- The server calls the plugin inside a structured exception handler (SEH on Windows). A memory fault in the plugin removes the plugin. A fault can corrupt shared state. SEH does not make native plugins safe.
- A watchdog checks the time of each event. A plugin that runs over the limit (for example 5 ms) is marked bad.
- Three failures remove the plugin from the list.
- Limit: this does not find a plugin that writes wrong data without a crash. The damage shows later, in other code.
- Linux needs another method (`SIGSEGV` with `sigsetjmp` and `siglongjmp`). It is less safe.

Level 2 (a separate process) and level 3 (a native plugin compiled to WASM) are the options for authors that nobody trusts. Level 2 adds a delay to each call, so it fits rare events only.

## Hash check for native plugins

Native code has no sandbox, so a warning is the protection.

1. The manager holds a list of SHA-256 hashes of native plugins that someone reviewed.
2. The user chooses a native plugin in the manager.
3. The manager computes the hash of the file.
4. Hash in the list: load with no warning.
5. Hash not in the list: show a warning ("This plugin runs native code with no protection. Load it only if you trust the source"). The user confirms or cancels.

Lua and WASM plugins need no warning.

Points of care:

- A hash proves identity, not safety. The person who builds the list decides what is safe.
- Each new build changes the hash. Option: the author signs the plugin, and the list holds the public key of the author. An update then needs no change of the list.
- The list must be trusted. Put it inside the manager, or sign it.
- The server must check too, not only the manager. If someone starts the server without the manager, the loader of the server must check the hash and refuse a plugin that is not in the list. An explicit option (for example `allow_unsigned_native=1`) can turn this off.
- Compute the hash of the bytes that the server loads. Copy the file to a server folder, compute the hash there and load from that copy. Then the file cannot change between the check and the load.

## Game modes

The server can make a mode that the client does not know. The client accepts all that is valid for the original protocol.

Possible without a change of the client:

- Position: teleport players, move objects, create and remove items and cars.
- Chat and messages: text, notices, a score in messages.
- Rules: money, health, team, who can kill whom, round time.
- World: scenario, buildings and missions come from the server.
- Bots: behavior, spawn and goals.

Examples: an arena with rounds, hide and seek, a race with checkpoints, a tutor that teleports a new player and explains each step in the chat.

Not possible without a change of the client:

- New menus, HUD, sounds, textures and models.
- Rules that the client calculates alone.
- Objects or items of a type that the client does not know.
- More players, objects or items than the fixed limits of the protocol.

Care:

- A hard teleport can show a jump, or the client can correct the position. Test with lag.
- Chat text has the size limits of the protocol. Split long messages.
- Prefer a change of state to a change of physics.

High level calls that modes need: `player_teleport`, `player_message`, `world_spawn_item`, `round_start`. A mode is a plugin with its own state (round, score, teams). It uses `on_tick`, `on_death` and `on_connect`.

## Bots

The code of the bots is in `playerai`, `ai_assessthreats`, `ai_playercar` and `ai_car_nextwaypoint`. A bot is an input of the server, like a player, so a change is safe for the client.

Places for a plugin:

- Decision: an event `on_bot_think(bot)` sets the target, the item or the destination.
- Threats: a filter in `ai_assessthreats` ignores or prefers some players.
- Cars: change the path or the speed.
- Creation: `bot.spawn(...)` with name, team and item.

Care: a plugin that changes the number of `os_rand()` calls changes the simulation. This changes the simulation and needs explicit tests.

## Tests

Test API events, invalid handles, cancellation, plugin failures and compatibility.
Use synthetic inputs and a bot load test to measure host cost per tick.

## Server manager

The server manager of a friend hooks the server by memory patches. Each hook shows a point where people wanted a change. When the new version of the manager is here:

1. List each hook (RVA, what it changes, which function).
2. Find the rewritten function for each hook.
3. Make one event or call of the API for each point.
4. Never copy `config.txt` or passwords into docs.
