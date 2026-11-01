# Shop and phone API

Capability 9 uses `SvGameShop`, version 1. Its four methods read phones, end calls, read catalogs and replace catalogs.
Every method requires the current plugin callback, the server thread and a ready game.
An error leaves output records and game state unchanged.
Each caller sets the input or output record's `size` to at least `sizeof` that record.

This API supplies general shop and phone operations.
The plugin sets its prices, wait times and purchase rules through the common API.
No public method creates a purchase, charges money or changes an existing item type.

## Public methods

| Method | Behavior | Results |
| --- | --- | --- |
| `phone(owner, reference, out)` | Copies `SvPhoneState` after it checks the phone, its holder and its peer. | `SV_OK`, `SV_STATE`, `SV_INVALID`, `SV_NOT_FOUND` |
| `phone_idle(owner, reference)` | Ends a valid outgoing ring, incoming ring or talk pair. It also clears an idle or dialing phone. | `SV_OK`, `SV_STATE`, `SV_INVALID`, `SV_NOT_FOUND` |
| `catalog(owner, building, out)` | Copies `SvShopCatalog`, including kind, capacity, count, `row_size` and rows. It zeros unused output rows. | `SV_OK`, `SV_STATE`, `SV_INVALID`, `SV_NOT_FOUND` |
| `catalog_replace(owner, building, input)` | Checks every row before it replaces the catalog. A count of zero clears it. | `SV_OK`, `SV_STATE`, `SV_INVALID`, `SV_NOT_FOUND` |

`SV_STATE` covers a wrong callback owner, another thread, an unready game or a denied write context.
Read methods still work when the shared context forbids writes.
Write methods require the installed `sv_plugins_write_context` adapter.
A null pointer or a short record returns `SV_INVALID`.

For phone methods, a missing, stale or wrong-kind reference returns `SV_NOT_FOUND`.
A live item of another type returns `SV_INVALID`.
An invalid scalar, holder or peer graph returns `SV_STATE`.
The methods do not repair an invalid graph by changing one end alone.

For catalogs, a building index outside 0 through 4095 returns `SV_INVALID`.
An index beyond the current building count returns `SV_NOT_FOUND`.
A corrupt building count, unsupported building kind, corrupt native row or corrupt native catalog count returns `SV_STATE`.
Replacement rejects invalid input counts or rows with `SV_INVALID`.
It can repair a corrupt old catalog because it does not read the old rows or count.
It still requires a valid building count and a supported building kind.

## Phone records and cleanup

A phone is an item of type 23. These states retain the native values:

| State | Value | Link rule |
| --- | --- | --- |
| Idle | 0 | No outgoing or incoming link. |
| Dial | 1 | No outgoing or incoming link. |
| Ringing | 2 | One caller points to one receiver. The receiver has no outgoing link. |
| Talk | 3 | Both phones point to each other. |

`SvPhoneState` copies the phone, direct human holder and peer references.
A loose phone or a phone inside an item has a zero human reference.
The record also copies the state, number, displayed info, cooldown and ring timer.
The scalar bounds are inclusive:

| Native field | Range |
| --- | --- |
| `call_state` | 0 to 3 |
| `phone_number` | 0 to 9999 |
| `info` | 0 to 65535 |
| `phone_line` | 0 to 3 |
| `cooldown` | 0 to 180 ticks |
| `ring_ticks` | 0 to 768 ticks |

The checks reject self-links, invalid indices, nonphone peers, duplicate callers and inconsistent reciprocal links.
Both ends must have valid declared holders.
A direct human holder must contain the phone exactly once, in its declared inventory slot.
An item container must contain it exactly once, in the declared child slot.

`phone_idle` clears state, number, info and ring timer, and removes links at both ends.
It keeps each phone's cooldown, id, line and inventory placement.
It sends the existing item-info event 6 for each changed phone.
Cleanup has no owner rollback. Stopping a plugin must not reopen a call that it ended.

## Catalog records and limits

Each `SvShopRow` contains three signed integers in native order: type, price and color.
`SvShopCatalogWrite` has a size, a count and space for ten rows.
The building kind selects the meaning and capacity:

| Native building kind | Public kind | Capacity | Type | Color |
| --- | --- | --- | --- | --- |
| 2 | `SV_CATALOG_VEHICLES` | 8 | Object model 0 to 13 | 0 to 5 |
| 7 | `SV_CATALOG_ITEMS` | 10 | Item type 0 to 26 | 0 |
| 4 | `SV_CATALOG_CLOTHES` | 2 | Row ordinal 0 or 1 | 0 |

Every price must be between zero and `0xffffff` (16777215).
All counts range from zero through the capacity of that building kind.
The core uses zero-based building and public row indices. Native menu purchases use one-based row choices.
Replacing a catalog creates no entity, takes no money and performs no purchase.
It clears unused native rows within that catalog's capacity.

The building records use a stride of `0x9dc`. The checked backing storage supports 4096 buildings.
The generated `City` declaration names eight shop rows, while the native gun market already writes ten.
This API uses whole-`City` byte offsets and bounds gun catalogs to ten rows within the existing record storage.
It changes no generated layout.
Native market refills can change these rows later. A plugin that keeps an offer must apply its policy again after a refill.

## Shared phone decisions

`SvPhoneEvent` is 40 bytes with the public packing rules.
It starts with `SvEvent base`, then contains phone and human references and the numeric prefix.
A listener must check `base.size >= sizeof(SvPhoneEvent)` before it casts the record.
The base carries the size, event kind and current game tick.
`SvGameEvent` carries the fields `item`, `object`, `value` and `extra`.
Phone decisions use the separate `SvPhoneEvent`. A listener checks the kind and the size of that record.

The shared event bits are `SV_EVENT_PHONE_DIGIT = 256` and `SV_EVENT_PHONE_CALL = 512`.
The callback has the full API at a safe point. It can claim the request with `SV_DENIED`, or pass with `SV_OK`.
The shared dispatcher calls subscribers in load order. The first claim ends delivery.
The host handles other plugin replies through its existing failure path.

## Shared property rows

The property table has these item rows for phones:

| Name | Type and access | Bound | Write rule |
| --- | --- | --- | --- |
| `item.phone_state` | Integer, get and set | 0 to 3 | Only state zero is writable. The check validates the phone and both holders before the shared table writes. |
| `item.phone_number` | Integer, get only | 0 to 9999 | No setter. |
