# Level API additions

These reads and one write come from the level data of the game. They cover N52 (buildings and bases), N39 (block and surface reads) and N40 (block edits).
All of them need a host callback on the server thread and a ready world. Any other call returns `SV_STATE`.
A bad pointer, a short output size or a value out of range returns `SV_INVALID`. A missing record returns `SV_NOT_FOUND`.
A failed call leaves the output unchanged. No call allocates memory. A plugin gets copies and never a pointer into the level.
Indices last until the next round reset.

## Buildings and bases (N52)

`building_count` gives the number of buildings, from 0 to 4096.
`building(index)` copies one building into `SvBuildingState`: the index, the kind, the native type, the position and the box.
The native type is the type number of the city data. The kind groups it for plugins:

| Kind | Native type |
| --- | --- |
| `SV_BUILDING_DEALERSHIP` | 2 |
| `SV_BUILDING_SHOP` | 4, 5 and 7 |
| `SV_BUILDING_HOSPITAL` | 6 |
| `SV_BUILDING_OTHER` | any other type |

The index is the index of the shop catalog (N15), so a plugin can join the two lists.
`base(team)` gives the base box of a team from 0 to 4 as kind `SV_BUILDING_BASE`, with the team set. The position is the minimum corner of the box.
A base with no box in this level returns `SV_NOT_FOUND`.

## Areas and blocks (N39)

`area(index)` copies the shape of one block area: the size in blocks on each axis, the size of one block, the origin and the three axes.
There are at most 4 areas. An area that is not active returns `SV_NOT_FOUND`.
`block(area, x, y, z)` copies one cell into `SvBlockState`: the block word, the interior word and the eight face materials.
The coordinates are grid coordinates in the area, from 0 to the size in blocks minus 1. They are not world coordinates.
An empty cell gives word 0 and is a normal answer.

`surface(from, to)` is a line query against the level only. It has the same bounds as `line`.
The result holds the hit of `line`, the material of the surface, the area, the cell and the face.
Material -1 means unknown. This happens for the landscape and for city objects.
The call copies the closest surface and then restores the shared collision record.

## Block edits (N40)

`block_set(area, x, y, z, value, material)` is a write method. It needs a safe callback.
The value is `SV_BLOCK_CLEAR` (0) or `SV_BLOCK_SOLID` (1). The material is from 0 to 1023 and must exist in the material list.
Only an empty cell or a normal cube can change. A cell with an interior, a city object or another block kind returns `SV_DENIED`.
The core updates the exposed faces on both sides of each cube.

The host keeps the first state of each changed cell, and of the six cells around it, for up to 1024 cells.
A write that needs more cells returns `SV_LIMIT`. A cell or a neighbor that another plugin changed returns `SV_CONFLICT`.
When the plugin stops, the host puts the saved state back and frees the sectors and chunks that it made.
A round reset clears the list.

The server changes its own level. The original client may not show the change, and it may predict movement against the old blocks.
Client effect unknown. A test with the original client did not run.
