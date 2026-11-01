# Plugin storage API

Capability 7 uses `SvGameStorage`, version 1. Its methods are `get`, `put` and `erase`.
Each method first checks the current plugin callback and game readiness.
No method gives a game pointer, a file path or a real account identity to a plugin.
A failed call leaves the stored values and output records unchanged.

## Keys, scopes and limits

`SvStorageKey` contains its size, a scope, an opaque subject reference and a name.
Names contain 1 to 31 ASCII letters, digits, dots, underscores or dashes.
Names never become paths. The host selects every file name.
`SvStorageValue` contains its size, a length and up to 1024 bytes.
A zero-length value is present and differs from a missing key.
A successful read clears every unused output byte.

| Scope | Subject | Lifetime |
| --- | --- | --- |
| `SV_STORAGE_SESSION` | Active player reference | Memory only. Clear on player leave and plugin stop. |
| `SV_STORAGE_ACCOUNT` | Account reference | Disk. The host maps the opaque reference to both native identity words. |
| `SV_STORAGE_SERVER` | Zero | Disk. Private to the stable plugin id. |
| `SV_STORAGE_ENTITY` | A human, an item or a vehicle | Memory only. Clear when the thing dies and when the world clears things of that kind. A player or an account reference gives `SV_NOT_FOUND`. |

Each plugin can store at most 128 keys and 65536 value bytes across all scopes.
Replacing a value releases its old byte count before the new quota check.
Removing a key releases its entry and bytes.
The entry bound also applies to empty values. A full table cannot create another key.
A plugin keeps counters and policy state here, without borrowing game record fields.

An entity value (`SV_STORAGE_ENTITY`) lives in memory only. The host checks the subject at each call: a human, an item or a vehicle that is alive. A player, an account, zero or a stale reference gives `SV_NOT_FOUND`. The value goes when the thing dies (`sv_storage_leave`) and when the world clears things of that kind (`sv_storage_clear`). A new thing in an old slot never reads the old value, because the key is the full handle and not the slot.

The private namespace uses the validated module id, not its version or numeric owner.
Profile order can change the numeric owner after a restart.
The same id therefore finds its data after a version update or profile reorder.
Different module ids, accounts and player sessions cannot read each other's keys through this API.
Account names and native record indices do not identify persistent data.
A reused account slot with another identity cannot read the prior account's value.
A reused player slot has another reference and cannot read the prior session's value.

## Results

| Method | Result |
| --- | --- |
| All methods | `SV_STATE` outside the current callback, before readiness or without a registered storage namespace. |
| All methods | `SV_INVALID` for a null/short key, invalid scope/name or nonzero server subject. |
| All methods | `SV_NOT_FOUND` for a wrong-kind or stale subject reference. |
| `get` | `SV_INVALID` for a null/short output. `SV_NOT_FOUND` for a missing key. `SV_OK` copies the value. |
| `put` | `SV_INVALID` for a null/short value or length above 1024. `SV_LIMIT` for either quota. `SV_IO` for a failed persistent save. `SV_OK` commits the value. |
| `erase` | `SV_NOT_FOUND` for a missing key. `SV_IO` for a failed persistent save. `SV_OK` removes the key. |

The open call also reports invalid arguments, conflicting ids/owners, allocation limits and file errors.
Opening a malformed snapshot fails without registering a partially loaded namespace.
No method changes the native account layout or its fixed disk format.

## Persistent saves

Each module directory contains `storage.bin`, `storage.tmp` and `storage.lock`.
The host pins the plain directory and holds its checked lock file while the namespace is open.
It refuses reparse directories. The loader and lock/temporary file checks require a single regular file.
The host follows the native host's ANSI Windows paths and `MAX_PATH` bound.
It accepts local drive paths. It does not accept UNC roots.

A persistent mutation writes a full candidate snapshot to the same directory's temporary file.
It checks every write, flushes the file, closes it, then replaces `storage.bin` with one same-directory rename.
It commits the in-memory row only after that replacement succeeds.
A failed write or replacement keeps the old in-memory value.
A session mutation needs no disk write.

The file uses a separate versioned format. Its header stores the magic, version, row count and stable module id.
Each fixed row stores the scope, internal account identity, key, length and value bytes.
Session references never reach disk. The loader checks exact file size, row count, scopes, names, value bounds,
byte quota, module id and duplicate keys before it registers the store.
It clears unused key/value bytes before any later read or save.
The largest valid snapshot is 137276 bytes, including fixed metadata.
A leftover temporary file does not replace the valid snapshot at startup. The next save safely overwrites that temporary file.

The flush and replacement use [FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)
and [MoveFileEx](https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexa).
The design keeps the old or new whole snapshot at the replacement point.
Power-loss behavior and storage-device failures still need a destructive environment test.
No power-loss result is claimed here.
