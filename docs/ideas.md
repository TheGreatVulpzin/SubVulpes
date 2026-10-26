# SubVulpes ideas

These ideas are proposals. See [the milestone plan](plano.md) for the implementation order.

## Server

| Idea | Product | Where | Notes |
| --- | --- | --- | --- |
| Own server list if the Cryptic Sea master does not answer | both | `getmasterserverip` | The master is `crypticsea.com/anewzero/serverinfo.php`. |
| Native plugin API (C, loaded from a DLL or `.so`) | SubVulpes | | Replaces the memory patches of the server manager. Plan with Lua and WASM hosts, hash check and game modes: [plugins.md](plugins.md). |
| Plugin events: chat, death, connect, disconnect, tick | SubVulpes | `serverrecv` (chat), the death code of `humansimulation`, `servercycle` | |
| Admin commands: `/heal`, `/tp`, `/npc`, `/imortal`... | SubVulpes | Official administration plugins through the command/event API | Define commands and permissions from documented needs. |
| Server manager modules as plugins: black market, death messages, faster truck, starting money, punches, bots | SubVulpes | `logic_*`, `item_*`, `humansimulation` | Define each module before implementation. |
| Admin console / RCON, structured logs | SubVulpes | Official interface and log-output plugins | Core supplies validated operations and events. |
| Server-side anti-cheat checks | SubVulpes | `serverrecv` | |

## Security work

The base includes the completed input checks.
The following items remain open because they change accepted values or add server policy.
These identifiers refer to the inherited security notes, not the SubVulpes milestones.
They are not a list of features to hardcode in the core. Classify the invariant and policy before implementation.
Check compatible base fixes for OpenSubRosa24 separately. New configurable administration belongs in official plugins.

| Item | Work reserved for SVS |
| --- | --- |
| A7 | Clamp float inputs. Define the accepted range for each field. |
| A8 | Limit the rate of query replies. |
| A9 | Define a window for action replay. |
| A10 | Deferred. The security plan lists this identifier without a description. Define its scope before implementation. |
| B5 | Count admin login attempts by IP address and require a strong default password. |
| C3 | Handle object item lists above the 255 entries that the wire count can represent. |
| C4 | Handle human health above 117 without a wrap in the 7-bit field. |
| D11, D12 | Deferred. The security plan lists these identifiers without descriptions. Define their scope before implementation. |
| Persistent `ban_time` | Save and load ban time. The account file does not store this field. |
| Server name | Limit the server name to 31 characters and a null byte. The query reply sends 32 bytes. |


### Placement decisions for S3

- A7: core validates data representation and required invariants. Gameplay-specific accepted ranges need a separately defined policy.
- A8 and A9: configurable query/replay policies can use required official plugins. Core transport and protocol safety remain unconditional.
- B5: the administration permission/authentication plugin owns login policy. Remote interfaces declare that service as a dependency.
- C3 and C4: core codecs enforce representable output. Plugins cannot bypass protocol bounds; gameplay limits remain a separate decision.
- Persistent `ban_time`: an official moderation plugin owns saving, expiration policy and data format through supported lifecycle/storage interfaces.

Core emits diagnostic and service events. Official plugins implement commands, RCON and selected structured log outputs.
Some official plugins remain always active. Define the required module list separately from default-enabled and optional features.
Plugin packages need independent versions, API compatibility checks and an update path that preserves their private settings and data.
A10, D11 and D12 remain undefined. This placement decision does not supply their missing requirements.


## System code

- Call `timeBeginPeriod(1)` before the main loop. The loop sleeps 1 ms, and the system timer resolution sets how long the sleep lasts, so the ticks have jitter. Measure the tick time with and without it. (SubVulpes)
- The S4 checks bound file sizes in `loadtextfile` and name copies in `listfiles`. See D8 and D9 above. (both)
- Stop `getint` from returning a random value when the text has no number. The clean code returns 0. (both)
- Fix the offset of `gettimeofday` (11644473600000000 in place of 116444736000000000). The function is not used now. (both)


## Future client

OverRosa may add custom assets and new protocol features. Keep the existing client usable where possible.
A browser client needs a WebSocket or WebRTC transport and user-supplied assets.

## Big ideas

All are for SubVulpes and need the plugin API first ([plugins.md](plugins.md)). All work with the original client unless a note says otherwise. Each part is useful alone.

| Idea | What it needs | Notes |
| --- | --- | --- |
| Replay and rewind | A log of the inputs, the `os_rand()` seed and the tick clock. Snapshots | The server is deterministic. Replay, kill-cam, rewind by 30 s and proof against cheats. A snapshot is about 600 MB, so it needs incremental snapshots or smaller pools. A plugin that reads outside data must record it. |
| Bots that talk | Plugin with a language model, chat and phone hooks | The original client sees only text messages. |
| Persistent world | A plugin that saves buildings, territories, stocks and reputation between rounds | The game already has economy, stocks, accounts and missions. A government plugin can charge taxes and run elections by chat. |
| Event director | Creation of items, objects and bots, and chat messages | A convoy, a police sweep or a truck strike. The difficulty follows the group. The cheapest big idea. |
| Linked servers | A safe account identity, and a server to server API | A player moves between servers with money and items. The protocol sends `account_id` as plain text, so identity needs new work. |
| Game modes as packages | Teleport, items, chat, rounds | Battle royale with a shrinking zone, capture the flag, races, a campaign with missions in a chain. |
| Discord and Twitch bridge | Chat events and a network plugin | The in-game chat and a Discord channel are one. The audience votes for events or spends points on items. |
| Live web map and analytics | A plugin that sends positions, deaths and stock prices over WebSocket | A top-down map of the city in a browser, heat maps of deaths, charts of the economy. Read only, so it is safe. |
| Seasons, ladder and achievements | Accounts and a database | Rankings by season, rewards, titles. |
| Mutators | State changes only | For example a random item for everyone each minute, or a bot swarm. Do not change physics: the client calculates it alone and would desync. |
| Random cities | The level builder of `src/game/level.c` | Only if the client builds the same level from a shared seed. If the client reads its own map files, a changed map desyncs. Check this first. |
