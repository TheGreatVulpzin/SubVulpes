# Development notes for the plugin-storage page

These sections describe how the host implements this API, the checks that proved it and the open work.
They are not part of the SDK reference.

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

## Test evidence removed from the reference page

The tests `entity` and `entity_gone` cover these rules. Removal proofs on 2026-10-28: without the subject check, without the clear at death, without the clear at the world reset, and with entity rows saved to disk, the test fails each time.
