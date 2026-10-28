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

Each plugin can store at most 128 keys and 65536 value bytes across all scopes.
Replacing a value releases its old byte count before the new quota check.
Removing a key releases its entry and bytes.
The entry bound also applies to empty values. A full table cannot create another key.
A plugin keeps counters and policy state here, without borrowing game record fields.

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

The core open seam also reports invalid arguments, conflicting ids/owners, allocation limits and file errors.
Opening a malformed snapshot fails without registering a partially loaded namespace.
No method changes the native account layout or its fixed disk format.

## Persistent saves

Each module directory contains `storage.bin`, `storage.tmp` and `storage.lock`.
The host pins the plain directory and holds its checked lock file while the namespace is open.
It refuses reparse directories. The loader and lock/temporary file checks require a single regular file.
The current implementation follows the native host's ANSI Windows paths and `MAX_PATH` bound.
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

## Core seams

The production host does not call these seams in this branch. Public calls return `SV_STATE` until registration.
The fixture supplies registration and retirement adapters. It does not prove the production hook sites.
The first editor must add these calls in owned files:

- After `SvModule.id` passes metadata and descriptor validation, call
  `sv_storage_open(owner, module.id, private_root)` before the module starts.
  Use a preserved root such as `user/plugins`. Refuse activation on open failure.
  The function creates only the validated module-id child directory.
- In `plugin_host.c`, call `sv_storage_release_owner(owner)` after the disable callback and on every failed enable or callback.
  Include shutdown and dependent-module failures. This clears sessions and closes the namespace.
  Persistent snapshots stay on disk. Do not delete the module directory or its snapshot on disable or update.
- In `plugin_entities.c`, at the existing player retirement path, capture the current player reference before its handle is erased.
  Call `sv_storage_leave(reference)` as part of that retirement. The helper accepts a stale reference by exact identity.
  Alternatively, use the existing leave notice before the next tick. Do not add another leave event.

Every persistent `put` and `erase` already saves synchronously in the safe callback.
No separate flush event, queue or background worker exists.
The shutdown path needs no pending-save drain because `SV_OK` means the save finished.
The installer preserves `user/plugins` while it replaces public DLL and manifest files.
Do not put data in the public `plugins/<id>` directory that the installer replaces.

## Hazards read before implementation

- `plugin_host.c` assigns owners by profile order. Using an owner in a disk path would mix data after reorder.
- `plugin_files.c` validates module ids and rejects reparse metadata/lock paths. Storage reuses the id validator and checks its own paths.
- `plugin_read.c` omits account identity. Storage resolves references internally and never adds that identity to an API record.
- `account_db.c` compares both identity words. A name or slot alone can bind data to the wrong account.
- `plugin_entities.c` erases player handles on retirement. A leave notice can therefore contain a stale reference.
- Fixed arrays need row, key, value and file bounds before any index, copy or quota arithmetic.
- Session keys count toward both quotas. Clearing only persistent rows at stop would retain counters from an old plugin session.
- A direct write to the live file can truncate the previous snapshot. The candidate-file replacement avoids that path.
- File replacement can fail after the candidate is written. The old memory row must remain unchanged on this error.
- A file can contain duplicate keys, another module id or an oversized count. Loading must finish before namespace registration.
- Reparse paths and extra hardlinks can make a private name refer elsewhere. The host refuses those storage paths/files.

## Work report

Changed files: `src/plugins/sv_game_storage.h`, `src/plugins/plugin_storage.c`,
`tests/systems/storage_plugin.c`, `tests/systems/storage_host.c` and this page.
Inspection used `Get-Content`, `rg` and read-only `git show main:docs/legacy-modules.md`.
The investigator read the module loader, installer, account identity and retirement paths.
Microsoft's file API documentation supplied the flush and replacement contracts linked above.
No shared source, native account layout, network protocol or generated storage file changed.
No merge, push or tag ran.

The tick fixture calls all methods and result codes, including wrong owners, another thread, wrong references,
short records, invalid scopes/names and oversized values. Reads on error preserve the whole output record.
The fixture proves both quotas with session values before it creates persistent data.
It checks replacement accounting, empty values and zeroed output tails.
The host uses real player and account constructors. It checks identical keys in different scopes and accounts,
owner reorder, preserved persistent data, missing-backend errors and cleanup after a stale player reference.
The command callback publishes success through an existing public player write, and the host checks that marker.

The host blocks file replacement, then checks both the old memory value and exact snapshot bytes after failed writes/removals.
It checks 17 malformed snapshots, two directory junctions and a hardlinked data file.
Each malformed-load failure leaves the namespace inactive. A restored valid snapshot must load and return the same values.
The two memory-bound removal variants caused a heap error or access fault. Their restored source passed again.
No crash artifact is part of the commit.

The first owner-check removal did not compile because it left an unused parameter.
That attempt does not count. The corrected removal keeps the parameter referenced while it removes the owner comparison.
The final run proved 28 distinct checks: context, owner/account/session privacy, input sizes and ranges,
entry/byte quotas, file bounds and duplicate rows, file-owner identity, directory reparse paths, hardlinks,
failed-save rollback, lifecycle cleanup and the module-id validator.
Every removal compiled, failed the systems test and restored the exact source bytes.
The script records `git diff` after each restoration. The restored build and systems test passed.
A depth-one log search confirmed the expected assertions or memory faults in all 28 negative runs.
The first broad log search also reached inaccessible unrelated temporary cases. The targeted search passed.

| Command | Result |
| --- | --- |
| `call <vcvars32.bat>` then `cmake --build build\server --target sv-systems-test sv-systems-fixture` | Passed. |
| `ctest --test-dir build/server -R systems-api --output-on-failure` | Passed, including the storage host assertions. |
| `python -I build/prove_storage.py` | Twenty-eight removals compiled and failed the test. Exact restored source built and passed. |
| `tools\build_hook.bat` | Passed for the normal server, launcher and all test targets. |
| `ctest --test-dir build/server --output-on-failure` | All eight tests passed in 58.45 seconds. A temporary safe-directory environment allowed Git metadata reads. |
| `python -I tools/re/gen_storage.py --check` | Passed. Checked 138 storage objects and 499799832 bytes. |
| `python -I tools/ci/check_native.py` | Passed. Checked 82 native sources. |
| `python -I tools/ci/check_repo.py` and `git diff --check` | Passed. Checked 311 files without private-file or whitespace errors. |

Scripts, temporary snapshots, compiled variants and logs stay under ignored `build/` paths.
The original client, live host lifecycle wiring, a production plugin update, power loss, out-of-memory failures,
UNC roots and long/Unicode path support were not tested.

## For OverRosa

No storage method needs a client protocol change. All scopes are server data.
The production registration and lifecycle seams above remain core integration requests.
There is no native account-file extension or new client event.
