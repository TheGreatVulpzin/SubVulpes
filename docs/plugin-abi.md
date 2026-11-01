# Plugin ABI rules

ABI means the exact byte layout and the call rules that a compiled plugin and the server agree on. A change to these rules breaks every plugin that was built before it.
This page lists the rules of ABI version 1 (`SV_PLUGIN_ABI`). A change that this page forbids needs a new ABI number.

Version 1 fixes these layouts: the 64-bit mask of `SvHost.subscribe`, the item size in the lists that the host fills, the pointer rule and the entity scope of the storage. The rules about growth at the end of a table bind every later version. Build every plugin against the headers of the matching SDK version.

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
6. **No pointer into the game.** A record holds copies and 64-bit references. The only pointers are function tables that the host owns, buffers that the caller gives, and the service tables that `SvPlugins.provide`, `find` and `borrow` pass between plugins.
6a. **Lists of records.** A list that the host fills sits in a container with the field `item_size` (or `row_size`). The host sets it to the size of one item, and the caller steps through the list with it. This holds for `SvAiThreats`, `SvMissionSlots` and `SvShopCatalog`. A list that the caller gives (`waypoints`, `street_route`, `SvShopCatalogWrite`) has the item layout of its table version. If such an item must grow, a new method at the end of the table takes the new layout. `SvVector` and `SvValue` never grow.
   The host reads the stride from the size that the caller set (`sv_list_stride` in `plugin_lists.h`), so an older caller gets items of the size it knows, and the host never writes more bytes than the caller gave.
7. **Text is a fixed array of printable ASCII with a final zero.** The limit is part of the record.
8. **Units.** A position is a float in game units. Time is in ticks (60 for each second), except `SvStatus.uptime_ms`. Money is an `int32_t`. An angle is in radians.
9. **Events.** The first 32 events are bit values in a 32-bit kind. Any later event is numbered: kind `SV_EVENT_EXT(i)`, bit `SV_EVENT_EXT_BIT(i)` in the 64-bit mask of `SvHost.subscribe`, `i` from 0 to 31. A new event record starts with `SvEvent` and sets its own `size`.
10. **Results.** The twelve codes keep their meaning. A call that fails changes nothing, and it checks the context before the other arguments.

## Not frozen

Only the public `sv_*.h` headers of the SDK are the ABI. Everything else in the server is internal and can change.
