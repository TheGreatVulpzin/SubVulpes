# Engine parameters API

Capability 8 uses `SvGameEngine`, version 1. Its methods are `describe`, `get` and `set`.
The table gives named parameters, types, inclusive ranges and captured defaults.
A new parameter adds a row to the same mechanism. No row names a legacy module or a vehicle model.
Every method first requires the current plugin callback and a ready game.
An error leaves the game and output records unchanged.

## Parameters

| Name | Scope and type | Range | Default | Note |
| --- | --- | --- | --- | --- |
| `object.engine_power` | Object type, float | 0 to 16 | Native type power at installation | Updates the type and all active object copies. New objects use the native constructor. Client effect not verified. |
| `physics.ragdoll_orientation_reset` | Global, boolean | 0 or 1 | 1 | Controls the identity reset of a collapsed body's target bone orientation. Needs the core consumer. Client effect not verified. |
| `human.death_start` | Global, integer | 0 to 99 | 0 | Inclusive health threshold for early collapse, item drop and related living/dead choices. Keeps the final death threshold separate. Needs core consumers. Client effect not verified. |

Use object types 0 through 13 for a type parameter. Use `SV_ENGINE_GLOBAL` for a global parameter.
The target is a type index, not an entity reference. No wrong-entity reference case applies to this table.
Wrong owners return `SV_STATE`. Invalid targets return `SV_INVALID`.
Names contain 1 to 47 bytes before the zero terminator. An unknown bounded name returns `SV_NOT_FOUND`.

`SvEngineValue` contains its size, a type tag and a float or integer value.
Boolean values use the integer member and the boolean tag. The setter refuses the wrong tag, NaN, infinity and values outside the range.
`describe` accepts a row index and target. It copies the name, scope, range and target's default.
`get` copies the current typed value. The output must have its full structure size.

The native type power is a scale. `createobject` multiplies it by 213.0 and stores a float in the object.
The API uses that same conversion for existing objects. It does not change the constructor, solver, float expressions or x87 controls.
The range permits zero power and bounds the copied force at 3408. It does not encode the legacy module's factor of four.
The health range keeps a native full-health spawn at 100 above the early-collapse threshold.
The boolean range has no other state.

## Ownership and defaults

Installation captures each type's native power after type setup.
It captures no plugin policy or per-object lifetime state.
A successful setter records the last writer for that row and target.
Another plugin can write the same parameter. The last successful writer then owns its restoration.
Stopping an earlier writer does not undo the later writer's value.
The host must call `sv_engine_release_owner(owner)` after disable, callback failure, failed enable and shutdown.
That helper restores only the owner's rows. It restores both native type power and the copies in active objects.
Global values return to 1 and 0. Repeated cleanup is safe. Owner zero does nothing.
An explicit write of the default still records its writer.

Defaults stay fixed across round resets. Do not rerun native type setup to restore them: those functions append geometry.
Installation runs once per process after native types load. Live type-table reload is outside this contract.

## Results

| Method | Results |
| --- | --- |
| All | `SV_STATE` for context. `SV_INVALID` for an invalid target or short/null record. |
| `describe` | `SV_INVALID` for a row outside the table. `SV_OK` copies metadata. |
| `get` | `SV_INVALID` for a null/empty/overlong name. `SV_NOT_FOUND` for an unknown name. `SV_OK` copies the value. |
| `set` | `SV_INVALID` for name, tag or range. `SV_NOT_FOUND` for an unknown name. `SV_STATE` until the game hooks for the parameter are installed. `SV_OK` commits the value and writer. |

Reads work before installation. A default read then reflects the current native type or the global native default.
No setter allocates an entity, changes a parent link or changes a native pool counter.
The per-type setter scans the fixed active-object pool and never indexes another object through an untrusted link.
