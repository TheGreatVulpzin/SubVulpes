# Core settings API

Capability 6 uses `SvGameSettings`, version 1. The table has `describe`, `get` and `set`.
All methods require the current plugin callback and a ready game. A failed call leaves game state and output records unchanged.
The table uses names, types and ranges. A new setting adds a row to this table.

## Rows

| Name | Type | Inclusive range | Default | Effect |
| --- | --- | --- | --- | --- |
| `player_limit` | `SV_CORE_SETTING_INT` | 1 to 128 | 16 from options | Sets the option and the active server limit. Does not kick players. |
| `new_account_money` | `SV_CORE_SETTING_INT` | 0 to `INT32_MAX` | 1000 | Applies only when the core creates a new account. Existing accounts keep their balance. |
| `bot_count` | `SV_CORE_SETTING_INT` | 0 to 256 | 24 in rounds, 32 in world mode | Requests traffic bots at the next native traffic creation point. Does not create or remove bots now. |

The player limit follows the native startup maximum and the existing eight-bit server-info field.
The lower bound permits a one-player server. The legacy module's limit of 32 is plugin policy.
Player, human and object pools can reduce the actual traffic count below the requested count.
The generic player constructor's 50000 balance is separate from the new-account default of 1000.
The account record and its disk format do not change.
The existing debug join path can raise the player balance. This API keeps that native debug behavior.

`describe` accepts an index from zero through `SV_SETTING_COUNT - 1`.
Its copied record gives the name, type, range and default. The bot default follows the current gametype.
`get` copies a typed value for a name. `set` accepts that same typed value record.
Names must have 1 to 31 bytes before the zero terminator.
Unknown bounded names return `SV_NOT_FOUND`. No method takes an entity reference because these settings are global.
A wrong owner returns `SV_STATE` before any input read.

| Method | Results |
| --- | --- |
| `describe` | `SV_OK`, `SV_STATE` for context, `SV_INVALID` for index or output size/pointer |
| `get` | `SV_OK`, `SV_STATE` for context, `SV_INVALID` for name or output size/pointer, `SV_NOT_FOUND` for an unknown name |
| `set` | `SV_OK`, `SV_STATE` for context or missing core install, `SV_INVALID` for record/name/type/range, `SV_NOT_FOUND` for an unknown name, `SV_LIMIT` when the shared event queue refuses the change |

An equal-value write succeeds without another notice. It still stores an explicit startup choice.
No setter saves the options file. A plugin can save its own policy with the storage API.
