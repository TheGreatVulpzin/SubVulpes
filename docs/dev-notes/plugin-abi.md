# Development notes for the plugin-abi page

These sections describe how the host implements this API, the checks that proved it and the open work.
They are not part of the SDK reference.

## Test evidence removed from the reference page

The test `list_strides` covers it. Removal proofs on 2026-10-28: without the cap on the stride, without the check of the first item size, without the check of the offset and without the bound of the count in `sv_list_put`, the test fails each time.
`tests/abi_snapshot.c` prints the size of every public record and table. The test fails when a size changes.

## Which tool for a new feature

| Need | Use |
| --- | --- |
| One more number of the game that a plugin reads and writes | A row in the property table (`plugin_props.c`). No ABI change. |
| A read of a whole record | A method at the end of `SvGameRead`, with a record that has `size`. |
| An action that needs several steps or checks | A method at the end of `SvGameWrite`. |
| A set of values with an owner, a range and a change event | A typed table of an area, as settings and engine do. Use it only when a row cannot hold the rule. |
| A decision about an action | A numbered event, a gate (`plugin_actions.h`) and a test. |
| A service between plugins | A table that a library plugin offers through `SvPlugins.provide`. |

## The snapshot test

The file `tests/abi_snapshot.txt` holds the expected output.
A deliberate growth at the end changes the file in the same commit, with a line in the message that names the new method or field. A change that is not at the end is a break and the review stops it.

## Cost

Measured on 2026-10-28 with MSVC 14.50, Windows x86 Release, and game code built with /arch:IA32.
Host: Intel Core i3-5005U at 2.00 GHz, 4 logical processors, Windows 10 build 19045.
The synthetic world holds 256 bots, 512 humans and 1,024 items.
One native plugin registers a query command and subscribes to no event.

QueryPerformanceCounter measures 10,000 calls per row, except the single idle tick.
The table gives the mean in microseconds per call.
The AI row calls the hook for all 256 players in each iteration.

| Operation | Microseconds |
| --- | ---: |
| sv_shop_poll | 0.004 |
| sv_mission_before_tick | 0.187 |
| sv_game_event_poll | 0.006 |
| sv_actions_poll | 0.003 |
| sv_ai_before_player for 256 players | 1.725 |
| All five hooks, with 256 players | 1.910 |
| One sv_plugins_tick with the idle plugin | 15.400 |
| Mean sv_plugins_tick with the idle plugin | 0.880 |
| within: dense humans | 10.030 |
| within: dense items | 9.010 |
| within: sparse humans | 10.488 |
| within: sparse items | 23.208 |

The combined hook mean meets the target of less than 100 microseconds for each tick.
No time threshold controls a test result.
The single cold tick includes timer overhead and first-call effects. It is not the mean.

Each query uses radius 4096 and capacity 256.
Dense queries place all entities inside the radius. They return SV_LIMIT after the 257th match.
Sparse queries place only the last entity inside the radius. They scan all 512 or 1,024 slots.
The benchmark runs through a public table inside the command callback.

Run ctest --test-dir build/server -R plugin-cost -V to print the costs again.
The benchmark uses the same game sources as the server.
It measures idle hooks and bounded queries, without map data, simulation work or plugin policy callbacks.
It does not prove client effects, soak behavior or worst-case latency.
