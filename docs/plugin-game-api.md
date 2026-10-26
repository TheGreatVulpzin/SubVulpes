# Game read API

Status: first read slice implemented. The build, fixture and real server checks passed within the limits below.
This first game API adds reads. It does not complete the [mapped API](plugin-api-map.md) or close S3.
Most mutation and gameplay hook capabilities remain future work.
The user ordered framework tests, then the full mapped API, then production plugins.

## Capability lookup and compatibility

Include `sv_game_read.h`. It includes `sv_game.h` and the common native ABI.
The read table uses capability `SV_API_GAME_READ` (`1`) and version `SV_GAME_READ_VERSION` (`1`).
Its table type is `SvGameRead`, with explicit `size` and `version` fields.

`SvHost.get_api` is an optional append after the original 24-byte Windows x86 prefix.
`SV_HOST_V1_SIZE` identifies that prefix. The base plugin ABI remains `1`.
Before accessing `get_api`, check that `host->size` covers the field and that the function pointer exists.
Earlier hosts can supply only the original prefix.

For the current header, a lookup inside a valid callback can use:

```c
const void *table = NULL;
const SvGameRead *read;
if (host->size < sizeof(SvHost) || !host->get_api)
    return SV_INCOMPATIBLE;
if (host->get_api(owner, SV_API_GAME_READ, SV_GAME_READ_VERSION, &table) != SV_OK)
    return SV_INCOMPATIBLE;
read = (const SvGameRead *)table;
if (read->size < sizeof(*read) || read->version != SV_GAME_READ_VERSION)
    return SV_INCOMPATIBLE;
```

Lookup needs the current callback's owner on the server thread, with no stop transition in progress.
An invalid owner, a worker-thread call or a call outside that context returns `SV_STATE`.
A null output-pointer argument returns `SV_INVALID` after the context check.
For a valid context and output pointer, lookup clears `*api` before checking the capability and version.
An unknown capability returns `SV_NOT_FOUND`. An unsupported version returns `SV_INCOMPATIBLE`, with `*api` still null.
A context failure leaves the output pointer unchanged.

Successful lookup does not mean the world is ready.
Read methods additionally require world readiness. They return `SV_STATE` before readiness and during stop callbacks.
Use the table only inside permitted callbacks, even if the plugin retains its address.

## Copies and references

Every snapshot copies data into caller-owned storage. No record contains an internal game pointer.
Set each output record's `size` before the call. The buffer must cover the current record size.
A larger buffer is permitted, but the method writes only the current record size.
Snapshot errors leave the caller's buffer unchanged.
Context failure returns `SV_STATE`, unresolved or wrong-kind references return `SV_NOT_FOUND`, and invalid buffers or indices return `SV_INVALID`.
Malformed internal counts return `SV_FAILED` where the method checks them.

`SvEntity` is an opaque 64-bit reference. Entity kinds are player, human, item, object and account.
Zero denotes no reference. Relations can contain zero when the related record is absent or has no valid reference.
Do not treat a reference as a pool index, persistent account identifier or connection identity.
Retirement, slot reuse and world reset invalidate the affected references.
Resolve each reference again through the relevant method. A previous successful read does not guarantee later validity.

## Entity pages

`entities(owner, kind, cursor, entities, capacity, count, next)` scans physical pool slots.
Start at cursor zero and continue with the returned `next` cursor.
The capacity must be between 1 and `SV_ENTITY_PAGE_MAX` (`256`). Supply all three output pointers.
The cursor must not exceed the selected pool's capacity. An unknown kind returns `SV_INVALID`.

Each successful call scans at most 256 physical slots and returns at most `capacity` references.
`next` identifies the next physical slot to examine.
Empty or short pages can precede later active entities. Continue until `next` equals the selected pool capacity.
A call at that final cursor succeeds with count zero and the same cursor.
Errors leave the entity array, count and next outputs unchanged.
Separate pages do not form an atomic world snapshot.

| Entity kind | Pool capacity |
| --- | --- |
| Player | 256 |
| Human | 512 |
| Item | 1,024 |
| Object | 1,024 |
| Account | 65,536 |

## Snapshot methods and bounds

| Method | Copied state |
| --- | --- |
| `player` | Name, account/human references, money, stocks, team, flags, input, look selectors and appearance vectors |
| `human` | Player/account/vehicle references, seat, health, bleeding, timers, position, velocity, input, stance and appearance |
| `item` | Parent relations, type, ammo, timers, mass, motion, child references, phone state and bills mask |
| `object` | Driver reference, type, health, color value, timers, motion, bounds, wheel counts, seat count and engine state |
| `account` | Name, money, `playtime_raw` and three look selectors |
| `bone` | One human bone's motion, rotation, mass and radius |
| `inventory` | One human slot's count and item references |
| `world` | Tick/reset sequence, mode/round/state, timers, raw clock, server name, limits, spawn position/sizes and type counts |

Bone indices are `0..14`: 15 bones. Inventory slot indices are `0..7`: eight slots.
Each inventory slot and item child list holds at most eight references.
Invalid inventory or child counts return `SV_FAILED` without modifying the snapshot buffer.
Object reads reject `wheel_count` outside `0..32`, `wheel_body_count` outside `0..16` and `seat_count` outside `0..7`.
`wheel_count` excludes the separately reported `wheel_body_count`.

## Association and raw-field meanings

`SvHumanState.account` comes from the body record. It can differ from the associated player's account.
`SvHumanState.seat` is `-1` when the snapshot has no valid vehicle reference.
`SvItemState.parent_slot` is `-1` when the snapshot has no valid human parent reference.

`SvObjectState.driver` identifies a player whose active, living human occupies vehicle seat zero.
The player's human reference must also point to that body.
The API derives this relation from current reciprocal records, rather than the raw last-driver index.

The API omits `SvItemState.vehicle`, `SvItemState.linked_item` and `SvObjectState.linked_object`.
Those historical raw indices can target a different entity after slot reuse.
Future support needs generation-safe associations, not a conversion of the current raw index into a fresh reference.

`SvHumanState.despawn_counter` reports raw engine state. It is not a remaining-duration promise.
`SvWorldState.spawn_sizes` copies the three raw spawn sizes. They do not define symmetric extents around the spawn position.
For item and object `time_to_live`, `65535` is the unlimited sentinel.
Item lifetime can decrease in both game logic and time-to-live (TTL) processing. One decrement per simulation tick is not guaranteed.

Player and account names use 32-byte arrays. They contain at most 31 copied bytes plus a null terminator.
The server name uses 72 bytes, with at most 71 copied bytes plus a terminator.
The API does not copy passwords or persistent account identifiers.
An account reference identifies only a live API record.

`playtime_raw`, `world_clock_raw`, appearance vectors, selectors and color values retain their current engine representation.
This contract does not assign invented time units or RGB meaning to raw values.
The world type counts currently report 27 item types and 14 object types.
Those counts do not establish arbitrary new client type support.

## Validation and remaining limits

The fixture command is:

```powershell
ctest --test-dir build/server -R game-api --output-on-failure
```

The implementation is committed as `92854b1`.
The normal build exited with code 0. All six CTest groups passed in 34.71 seconds.
A focused `game-api` rerun after the build completed passed in 0.79 seconds.
Its local evidence is `build/server/Testing/Temporary/LastTest.log`.
The fixture reported `PUBLIC_READ_API_PASS` and `STOP_READ_DENIED`, and exercised real constructors and entity retirement paths.

The native dependency check passed with 74 sources.
Storage validation passed for 138 independent objects and 499799832 declared bytes.
The repository privacy check found zero problems in 262 files.
These checks describe this read slice, not later API work.

### Real server runs

Two real server cycles used the previous public framework package for data and its original status DLL.
The test setup used the current server executable and added the `test.read` fixture.
This was an isolated runtime check, not a new release package.
Both cycles reported `PUBLIC_READ_RUNTIME_PASS` and `STOP_READ_DENIED`, exercised live disable/enable and exited cleanly with code 0.

| Cycle | PID | Duration | Tick sample |
| --- | --- | --- | --- |
| First | 16928 | 64.53 seconds | 62.3 ticks/s, one sample |
| Second | 16076 | 3.54 seconds | No periodic sample |

The single tick sample is not a performance comparison or proof of sustained loaded-server behavior.
The result record is `build/game-read-review/94cd5dbd49d647ee816e5121ed4afbd3/result.json`.
The same directory contains `cycle-1.log` and `cycle-2.log`.
Its base package is `SubVulpes-server-v0.0.0-s3-framework.1.zip`.
The result records these exact SHA-256 identities:

| Component | SHA-256 |
| --- | --- |
| Current server | `6191ea5849e8662dd46c1721c8176b94cdef7075459ce77777670e5862ac77f8` |
| Read probe | `b6ac3f5927fbc608ca168850feee3094008cadd09fb944c0bc229e1b261d5a74` |
| Previous package's status DLL | `e5f2171160988276c6fa7994bd3be1f116dfbcc0c0972e110dc5932370242b11` |

The older status DLL remained usable with its original ABI prefix during these runs.
This does not establish compatibility for every third-party module.

### Remaining limits

The current slice introduces no public game mutation operations or original-client behavior changes.
Original-client gameplay and manual launcher acceptance remain pending.
The full mapped API and S3 remain open. Production gameplay and administration plugins follow the complete mapped API proof.
These results do not establish client gameplay parity, broader hook coverage, Lua or WASM support.
See [native SDK](plugin-sdk.md) and [S3 evidence](s3.md) for the framework contract and earlier package evidence.
