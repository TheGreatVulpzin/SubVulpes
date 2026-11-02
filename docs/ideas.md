# SubVulpes ideas

These ideas are proposals. See [the milestone plan](plano.md) for the implementation order.
Complete the existing framework proof first, then the whole mapped API with tests, then production gameplay and administration plugins.

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

The base includes earlier input checks and the implemented C3/C4 codec guards.
Other items below remain open because they change accepted values or add server policy.
These guards do not establish that all inputs are safe or that S3 is complete.
These identifiers refer to the inherited security notes, not the SubVulpes milestones.
They are not a list of features to hardcode in the core. Classify the invariant and policy before implementation.
Check compatible base fixes for OpenSubRosa24 separately. New configurable administration belongs in official plugins.

| Item | Status and work |
| --- | --- |
| A7 | Clamp float inputs. Define the accepted range for each field. |
| A8 | Limit the rate of query replies. |
| A9 | Define a window for action replay. |
| A10 | Deferred. The security plan lists this identifier without a description. Define its scope before implementation. |
| B5 | Count admin login attempts by IP address and require a strong default password. |
| C3 | Implemented core wire guard. Both object packet writers select the first 255 eligible items in the existing order. Later identifiers can receive no updates while 255 earlier identifiers remain eligible. |
| C4 | Implemented core wire guard. Encode health plus ten from 0 through 127, with bounds checks before addition. Server health remains unchanged, including values below -10 or above 117. |
| D11, D12 | Deferred. The security plan lists these identifiers without descriptions. Define their scope before implementation. |
| Persistent `ban_time` | Save and load ban time. The account file does not store this field. |
| Server name | Limit the server name to 31 characters and a null byte. The query reply sends 32 bytes. |

Commit `bba5b84` implements C3/C4 in `src/net/netpack.c`.
The real [codec tests](../tests/codec_bounds.c) passed for both writers, including integer extremes, item selection and trailing payload.
See [network limits](game/network.md#representation-limits) and [S3 evidence](s3.md).
Unchecked contact-bond capacity and mission slot 16 remain open in the [API safety catalog](plugin-api-map.md#source-checks-that-change-the-api-design).

### Placement decisions for S3

- A7: core validates data representation and required invariants. Gameplay-specific accepted ranges need a separately defined policy.
- A8 and A9: configurable query/replay policies can use required official plugins. Core transport and protocol safety remain unconditional.
- B5: the administration permission/authentication plugin owns login policy. Remote interfaces declare that service as a dependency.
- C3 and C4: implemented core codecs enforce representable output. Plugins cannot bypass protocol bounds. Gameplay limits remain a separate decision.
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
| Boombox item for the jukebox | The jukebox plugin, `SV_EVENT_ITEM` (pick up and drop), an item tag in storage (`SV_STORAGE_ENTITY`), `sound` at the position of the item, chat messages | A briefcase (or another item) that plays the music while a player holds it. When a player picks it up, the chat says "You picked up the NAME", with a fun name for each song or for the item. The sound comes from the item, so it follows the player. The music runs only while the item is held, and stops when it is dropped (no new notes start). The holder controls it: the use of the item, or chat commands, for next, stop and the channel list. A tag in storage marks which items are boomboxes, and the host removes the tag when the item goes. The client shows the normal item and cannot show a new name, so the chat carries the name. Not started: the jukebox comes first. |
| Flight (`/fly`) | `human.bone_velocity` with `add` (N12), key and look reads, a plugin tick; `object.velocity` for vehicles | Each tick the plugin cancels gravity on bone 0 and adds speed along the look direction. A toggle command starts and stops it. A flying car uses `object.velocity` the same way. The body keeps the walk or fall pose, because there is no flight animation. The client may predict the movement and cause jitter, and fall damage is unknown. Test both in the test room before any other plugin uses flight. Not started. |
| Touhou danmaku mode | Flight, `item_create`, `teleport` for free items, a bot as the boss, `storage`, the jukebox | A boss bot in a closed room throws patterns of small items (circle, spiral, fan, aimed burst). The plugin moves each item every tick and tests the distance to the player with a small hitbox. Extras: graze points, a bomb command that clears the items, lives and score in storage, spell card names in chat, four difficulty levels, and MIDI themes from the jukebox. Limits: the game caps the item count (about 100 to 300 at once, to be measured), `teleport` each tick may flicker (as in test room case 59), and there are no new models. A replay needs the plugin random with its own state and tick timers (R9). First step: one boss, one circle of 32 items, and a measure of how many items the server holds. Not started. |
| Gensokyo total conversion | Flight, the `bullet` write method, `bullet_hit`, shot events, `set_name`, shop and mission texts, the jukebox, World mode | A shot event is cancelled and the plugin creates bullet patterns (circle, fan, spiral) with `bullet`. The game holds 16384 bullets. Lives, bombs and score live in storage. Player and NPC names, shop rows, missions and announcements use Touhou names. The jukebox plays MIDI themes. The original client cannot change the map, the textures, the models or the item names, so the look stays the same and the chat carries the new names. A real bullet is fast and falls with gravity, so slow danmaku bullets may need moved items instead. Order: flight, one boss with one pattern, the weapon conversion, the texts. Not started. |
| Other total conversions | Round or World mode, rounds, a scoreboard, a map reset | Zombies, battle royale, hide and seek, street racing, cops and robbers, rescue, cold war spies, a roleplay city, prison break, tower defense, delivery trucks, horror. Zombies and racing need only the current API. All need one shared game mode module with rounds, a scoreboard and a reset. A mode with a winner and an end uses Round mode (team select, timer, `resetgame`). A place where players stay uses World mode (saved accounts, a continuous clock, 32 traffic bots). A plugin cannot call `resetgame` yet (N53). Not started. |
| Plugin store repo | A catalog repo with `index.json`, a CI check, and a store client in the launcher | A separate repo that holds only data: id, version, author, `.svplugin` link, SHA-256 hash, requested capabilities and the minimum API version. A pull request adds an entry. CI checks the format and the hash. The launcher reads the catalog by URL and shows the capabilities before install (the approval flow and `--plugin-inspect` exist). A native plugin runs C code, so the store warns about it and marks verified plugins. The official plugins stay in `official/` until the first release. Not started; after the release. |
| Official-only plugins, install review kept | The approval flow of the host and the launcher, `--dev-plugins`, the official hash list of the package, the store catalog (later) | Keep the install review for every new plugin, Lua or native: the window that shows the identity, the capabilities and the risk, and asks the owner to confirm. A plugin that nobody reviewed or approved is blocked automatically at start. Remove the way around it for normal users: the launcher box that allows unapproved native DLLs, and the same switch as a normal feature. A plugin author keeps the developer mode from the command line (`--dev-plugins`). Official plugins (their hash is in the list of the server package) load without a review. Later, a plugin from the store with a verified hash gets the same install review. The first-steps guide of the SDK changes to teach the developer mode for a plugin that the author is still writing. The jukebox and the test room need to become official, or the owner reviews them once. Open question: is the box the only thing to remove? Not started. |
| Plugin install without the stop and the extra step | The install flow of the launcher (`Launcher.cs`, the approval dialog), the plugin installer script, later the host lock and live enable (`live=1`) | Today the install stops the server and a new plugin starts disabled, so the owner must enable it by hand after a review that already asked for consent. Step A (do first, only the launcher): the launcher remembers if the server was running, installs, enables the plugin and starts the server again. A box "Turn the plugin on after install" is checked by default. The dialog text becomes: "The server will restart to install this plugin." The sentence about a new plugin that starts disabled goes away. Step B (after the first release, with the plugin store): a new plugin installs while the server runs and turns on at once, with `live=1`. Only the update of a plugin that is already loaded needs a restart. This needs a change of the host lock. Test: install and update a plugin with the server stopped and with the server running, with the box on and off, and check that the players are told before the restart. Not started. |
| Random cities | The level builder of `src/game/level.c` | Only if the client builds the same level from a shared seed. If the client reads its own map files, a changed map desyncs. Check this first. |
