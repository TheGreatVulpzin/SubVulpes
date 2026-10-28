# Plugin ABI rules

ABI means the exact byte layout and the call rules that a compiled plugin and the server agree on. A change to these rules breaks every plugin that was built before it.
This page is the freeze rule for ABI version 1 (`SV_PLUGIN_ABI`). The first editor and the second editor follow it. A change that this page forbids needs a new ABI number and a decision of the owner.

## What the ABI contains

- The entry point `sv_plugin_entry` and the record `SvPlugin`.
- The host table `SvHost` and its size `SV_HOST_V1_SIZE`.
- Every capability table (`SvGameRead`, `SvGameWrite`, `SvGameProps`, `SvPlugins` and the others), the records that they use and the event records.
- The result codes, the event kinds, the permission levels and the numbers of the capabilities.
- The calling convention (`__cdecl`), the packing (8 bytes) and the 32-bit x86 layout.

## Rules

1. **Growth goes at the end.** A new method goes after the last method of its table. A new field goes after the last field of its record. Nothing moves, shrinks or changes its type.
2. **Every table and every record starts with `size`.** The host sets it. A plugin that reads a field after the first ones checks `size` first. A table also has `version`.
3. **The version counts breaking changes only.** A new method is not a break, and the version stays. A break is a method that changes meaning, a field that changes unit, or a removal. Avoid it. If it cannot be avoided, raise the version and keep the old table for the old number (`SV_VERSION_ACCEPTED`).
4. **A request for an older version gets the current table.** The current table must work for the older callers. A request for a newer version than the host knows gives `SV_INCOMPATIBLE`.
5. **No number is reused.** An enum value, a result code, an event kind or a capability number keeps its meaning forever. A removed item leaves a hole.
6. **No pointer into the game.** A record holds copies and 64-bit references. The only pointers are function tables that the host owns, and buffers that the caller gives.
7. **Text is a fixed array of printable ASCII with a final zero.** The limit is part of the record.
8. **Units.** A position is a float in game units. Time is in ticks (60 for each second), except `SvStatus.uptime_ms`. Money is an `int32_t`. An angle is in radians.
9. **Events.** The first 32 events are bit values in a 32-bit kind. Any later event is numbered: kind `SV_EVENT_EXT(i)`, bit `SV_EVENT_EXT_BIT(i)` in `SvPlugins.subscribe64`, `i` from 0 to 31. A new event record starts with `SvEvent` and sets its own `size`.
10. **Results.** The twelve codes keep their meaning. A call that fails changes nothing, and it checks the context before the other arguments.

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

`tests/abi_snapshot.c` prints the size of every public record and table. The file `tests/abi_snapshot.txt` holds the expected output. The test fails when a size changes.
A deliberate growth at the end changes the file in the same commit, with a line in the message that names the new method or field. A change that is not at the end is a break and the review stops it.

## Not frozen

The headers `plugin_*.h` and the files of `src/plugins/*.c` are internal. The ABI is only what the public `sv_*.h` headers say.
