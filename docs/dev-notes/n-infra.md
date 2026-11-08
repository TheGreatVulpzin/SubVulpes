# Infrastructure API development notes

Worktree: `SubVulpes-SVS-n-infra`. Branch: `codex/n-infra`.
This draft records source changes and test cases. All execution results below remain pending.

## Needs status

| Need | Status | Reason and remaining work |
| --- | --- | --- |
| N53 | Done in part | Save and controlled stop use deferred requests. Restart gives `SV_STATE` until the owner defines a launcher restart convention. |
| N41 | Existing implementation complete | The host owns bounded timers and an independent random stream for each plugin. The existing timer limit result stays `SV_LIMIT`. |
| N50 | Implemented | The capability, masked record, salted identifiers and byte-order test exist. Added privacy tests and removal proof await execution. |
| N54 | Part 1 implemented | Registered plugin commands run as console actor zero with bounded text replies. Part 2 is intentionally skipped. |
| N47 | Not done | Ordered voice work remains pending after the earlier rows and their tests. |
| N48 | Not done | Ordered voice work remains pending after the earlier rows and their tests. |

The N41 brief requests `SV_INVALID` or `SV_STATE` for a full timer table.
The existing API and focused tests define `SV_LIMIT`. This work keeps that result to preserve the published ABI behavior.

## Added operations and limits

`sv_server.h` defines two new version 1 tables. Existing table names, numbers and layouts stay unchanged.
`server_control` has bit 13, API ID 14 and the `high` danger level.
`console` has bit 14, API ID 15 and the `high` danger level.
No new property row or event kind is required.

`SvServerControl.request` accepts one pending save or stop request across all plugins.
The method requires the current owner, a ready world, a safe callback phase and the capability.
A second request gives `SV_STATE`. An unknown request gives `SV_INVALID`. A missing capability gives `SV_DENIED`.
The main loop consumes the request before callbacks and simulation at a later tick.
Stopping the owner cancels its unconsumed request. A completed account save remains persistent.

The save writes the full version 3 file to a new temporary file and flushes it before atomic replacement.
The header holds three 32-bit values: version, account key and count.
Each 60-byte record holds two IDs, the full 32-byte name, money, play time and three look selectors.
All numeric fields use the existing Windows x86 little-endian representation.
The count ranges from 0 through 65,535. The file length is `12 + 60 * count` bytes.
Version 3 excludes `ban_time`. The writer adds no synchronization of player balances into account records.
Failed saves keep the previous file and remove the temporary file.
Success posts the existing disk-save notice with account zero. The console records success or failure.

A stop request uses controlled shutdown after the plugin call returns. It adds no automatic account save.
A restart request gives `SV_STATE` because the launcher has no server-originated restart convention.
The launcher Restart button already stops the process and starts it again under launcher control.
No launcher file changes belong to this work.

`SvConsole.execute` runs registered plugin commands as actor zero without a living player.
Console authority satisfies public, administrator and local permissions. The command owner retains its own capabilities.
Input lines have at most 300 bytes. Arguments have at most 255 bytes.
The reply has at most 4,095 bytes plus its zero terminator. Its reported length excludes that terminator.
The host captures command-owner logs and `message_to` replies to actor zero, with newline separators.
It returns `SV_LIMIT` after a command runs if the reply does not fit.
Game-owned, unknown and host management commands give `SV_DENIED`.
Nested calls and unsafe phases give `SV_STATE`. Invalid text or output arguments give `SV_INVALID`.
There is no subscription to arbitrary console lines. An optional log plugin can keep its own event log.

## Existing N41 and N50 behavior

Each plugin owns at most 32 timers. Delays range from 1 through `UINT32_MAX` ticks.
Timer IDs replace only that owner's timers. Delivery precedes the tick event.
Round resets keep timers. Subscription removal and plugin stop remove them.
The host seeds each random stream from the game seed and plugin ID.
Plugin random calls change neither `os_rand()` nor the game's Mersenne Twister state.

The `network` capability remains bit 12 with high danger.
A caller without it receives zero address bytes, the masked flag and the same salted identifiers as an authorized caller.
The host uses a private 16-byte profile salt with HMAC-SHA256.
It reuses a valid salt and refuses a malformed salt without replacement.
The address identifier uses four IPv4 bytes. The network identifier uses the first three bytes and a separate domain.
The 32-bit identifiers can collide. Native plugins retain full memory access.

The byte-order test sends a real UDP join from `127.0.0.1` through `serverrecv`.
It checks the stored value `0x7f000001`, the API bytes `127, 0, 0, 1` and the host-order source port.
The test selects unequal port bytes to expose a missing byte swap.
This test uses real sockets and the native server receive path. It does not use the original game client.

## Focused test cases

| Source | Cases |
| --- | --- |
| `tests/infra_plugin.c` | New table sizes and versions, invalid requests, duplicate requests, save, stop, unavailable restart, missing capabilities through a borrowed table, and refusal during simulation decisions. |
| `tests/infra_plugin.c` | Console execution within one owner and across owners, actor zero, restored caller context, logs, direct console replies, arguments, command failure, recursion refusal and bounded output. |
| `tests/infra_plugin.c` | Empty and excessive input, newline and carriage return, null output arguments, invalid capacities, game commands, unknown commands and host management commands. |
| `tests/infra_host.c` | Deferred single consumption, exact complete version 3 file, locked-target failure, invalid account counts, old-file preservation, temporary-file cleanup and deferred stop. |
| `tests/plugin_runtime_fixture.c` | A caller without either new capability cannot open or call the tables. Independent plugin random state and timer cleanup use the existing fixture. |
| `tests/plugin_game_fixture.c` | Existing timer bounds, replacement, exact delivery, rearm, random values, masked and real connection records, stable address IDs and shared network IDs. |
| `tests/plugin_game_test.c` | Existing timer behavior across game tick wrap and reset, random isolation, connection bounds and real UDP address conversion. |
| `tests/test_game_api.py` | Both network permission variants, stable salt bytes and timestamps, stable IDs, no raw/hex/Base64 salt in output, and startup refusal for short and long salt files. |

The infrastructure host calls the plugin listing, so the captured output privacy assertion also covers that listing.
The salt assertion checks raw bytes before any test output reaches the parent console.
Malformed-salt tests restore the original bytes and timestamps in a `finally` block.

## Execution evidence

| Required check | Result |
| --- | --- |
| Normal server and launcher build | `tools\build_hook.bat` built both, on 2026-11-08, after the merge of the three branches. |
| Focused infrastructure, game API and lifecycle tests | `game-api` failed after the merge. Two causes: the connection check made and deleted a player, which posted an extra leave event, and the property table grew to 38 rows. The fixture was fixed and `game-api` passed. |
| Full rebuild and CTest suite | 29 of 31 passed on the first run. The two failures were `abi-snapshot` and `game-api`. Both passed after the fixes. The whole suite did not run again in one pass. |
| Storage generator check | Passed. |
| Native dependency check after the normal build | Passed: 92 sources, no image or address dispatch. |
| Repository check before each commit | Passed. |
| Security removal proof | Not run. |
| Documentation lint | Not run. |

Do not treat the presence of a test case as a passing result.

## Limits and owner decisions

The owner must define the signal for a requested restart and the launcher response to that signal.
The launcher currently starts a replacement only after its own Restart action.
No restart code or automatic restart behavior is assumed.

No original-client session, audible voice test or complete launcher Start/Stop test ran for this draft.
The deferred save completion has an existing event and console result, but no new reply callback.
The atomic file tests cannot prove behavior during power loss or every filesystem failure.
Native plugin faults can still stop the server.

Voice injection and routing remain pending ordered work. Both rows must state `client effect unknown` if implemented without a client session.
The transport uses 64 ring entries, payloads of at most 255 bytes and modes from 0 through 3.
The receive path takes six frames. The send path transmits the latest four frames for each of eight slots.
Voice human IDs have eight bits with 255 reserved for no body, while the human pool has 512 slots.
Any voice implementation must state that representation limit and prove its memory bounds.
