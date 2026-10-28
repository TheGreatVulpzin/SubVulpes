# ABI version 1 header audit

Date: 2026-10-28. Base: `65effe6`. Scope: all 15 public `sv_*.h` headers.
This audit compares the headers with [the ABI rules](plugin-abi.md) and the C implementations.
No field, method, result code or capability number changes in this audit.

## Layout findings

| Header | Finding |
| --- | --- |
| `sv_plugin.h` | Records start with `size`. `SvHost` uses `abi`, rather than `version`. `SvPlugin` has callback fields. |
| `sv_config.h` | The table starts with `size, version`. The event starts with `SvEvent`. |
| `sv_plugins_api.h` | The table starts with `size, version`. Service tables pass native pointers. |
| `sv_game.h` | `SvVector` has no `size`. Its layout is three floats. |
| `sv_game_read.h` | All records start with `size`. Snapshot getters require the full current size. |
| `sv_game_write.h` | The table starts with `size, version`. All 24 methods use `SV_CALL`. |
| `sv_game_props.h` | `SvValue` is a union without `size`. The table and `SvPropInfo` have size fields. |
| `sv_game_events.h` | Each event starts with `SvEvent`. Its `base.size` covers the full event. |
| `sv_game_ai.h` | `SvAiThreat` and `SvAiStreetPoint` have no `size`. Other records have size fields. |
| `sv_game_economy.h` | Both records have size fields. The table has a version. |
| `sv_game_engine.h` | All records have size fields. The table has a version. |
| `sv_game_mission.h` | `SvMissionState` starts with `SvMissionRef`, without a separate size field. |
| `sv_game_settings.h` | Records have size fields or an `SvEvent` prefix. The table has a version. |
| `sv_game_shop.h` | `SvShopRow` has no `size`. Other records have size fields or an event prefix. |
| `sv_game_storage.h` | All records have size fields. The table has a version. |

The snapshot covers these 53 records, including `SvVector` and `SvShopRow`, plus 14 callback tables.
The five records without size fields cannot gain a prefix under ABI version 1.
Arrays of these records also cannot change their stride under the current contract.
`SvMissionState.reference.size` describes the reference only. It is zero for an inactive slot.
Adding an independent size prefix would change offsets. Such changes need an owner decision and a new ABI.

All capability tables use 8-byte packing. `SvVector` and `SvValue` sit outside explicit packing scopes.
Their current members need at most 4-byte alignment. The x86 snapshot checks their current sizes.
All public callback members use `SV_CALL`. Public records contain no pointer into game storage.
`SvPlugins.provide` and `find` pass native service tables, as the library contract states.
These pointers are an existing exception to the narrower pointer rule in the ABI page.

## Units and results

- `SvAccountState.playtime_raw` copies account play time without a conversion.
  The world pass adds 5 when the world clock reaches a multiple of 18,000.
- `SvWorldState.world_clock_raw` copies the world clock. The world pass adds 10 to that clock.
  Mission `expire` values use this absolute clock, rather than a duration in server ticks.
  Keep these raw fields and limits. A change of units needs a new field or a new contract.
- Snapshot names copy bytes from game storage and add a final zero.
  They do not filter each byte to printable ASCII. A new filter would change observable output.
- `SvGameRead.within` returns partial results and a count with `SV_LIMIT`.
  Its header states this exception to the rule that a failed call changes nothing.
- Callers set the capacity in output `size` fields before calls. The host then sets the copied record size.
  The ABI page says only that the host sets size. The SDK must explain both steps.
- Snapshot readers require the full current size. Future tail growth must retain the accepted older prefix size.
  Do not ship a larger record with only a snapshot update and a larger `sizeof` check.

## Number domains and comments

Capability IDs 1 through 12 are distinct. Result codes 0 through 11 are distinct.
Each named event uses a distinct bit. Extended event indices use their own numbered domain.
Enums reuse numbers across separate domains, such as property types and operation flags. No same-domain alias exists.
`SV_LINE_ALL` is a deliberate union of three flags. Count and limit constants are not new enum identities.

The mission seam comment requires N16 event rows, but mission notices and decisions remain open.
The SDK introduction said that game mutation was absent. This audit corrects that text to match the current headers.
The internal shop and capability comments still describe earlier stubs. The gate change will correct those comments.
These comment defects do not justify a binary layout change.

## Limits

This audit does not prove client effects, older binary interoperability or script isolation.
The snapshot checks sizes, callback counts and endpoint offsets. It cannot detect every change of field meaning or order.
Review every public-header diff under the ABI rules, even when the snapshot passes.
