# Plugin approval

## Scoped reviews

The review store is `user/plugins/approvals.ini`.
It binds approval to the plugin ID, payload hash, manifest hash, runtime and complete capability set.
It also keeps the package version, ZIP hash and UTC time.
An installed-ID review has no ZIP hash.

The file has one value per ID in the `[approvals]` section:

```text
ID=1|VERSION|RUNTIME|CAPABILITIES|PAYLOAD_SHA256|MANIFEST_SHA256|ZIP_SHA256|UTC
```

A dash represents an empty capability set or ZIP hash.
The other hashes contain 64 lower-case hexadecimal characters.
UTC uses `yyyy-MM-ddTHH:mm:ssZ`.
The server replaces the whole value in one profile write.

A saved scoped review takes precedence over both legacy approval files.
A wrong hash, runtime or capability set rejects the plugin before DLL execution.
An invalid scoped record also rejects it.
The server does not fall back to a legacy hash for that ID.
Changing display metadata changes the manifest hash and needs a new review.

## Review commands

Inspect the package or installed ID first with `--plugin-inspect ZIP|ID`.
Use this command to read the last saved review:

```text
subvulpes_server.exe --plugin-trust-status ID
```

It prints `TRUST_NONE`, a tab and the ID when no scoped record exists.
A saved review prints this tab-separated record:

```text
TRUST 1 ID VERSION RUNTIME CAPABILITIES PAYLOAD_SHA256 MANIFEST_SHA256 ZIP_SHA256 UTC
```

Empty fields stay empty in this report.
An invalid store or record returns a nonzero exit code.

After review, save the exact inspected values with:

```text
subvulpes_server.exe --plugin-trust ID PAYLOAD_SHA256 MANIFEST_SHA256 native CAPABILITIES ZIP_SHA256 --native-confirmed
```

Use a dash for an empty capability set or ZIP hash in this command.
The command checks the installed files and declaration again before it writes.
Native code requires the extra confirmation argument.
It still has full process access. The capability list does not isolate native code.

The launcher uses the previous ID, runtime and capability set to select the review.
The same set permits one update question with the version change.
A different set, a different runtime or no previous scoped review requires the full review.
The launcher dialog follows in task 3.

## Legacy behavior

Both `plugins/native-approvals.ini` and `user/plugins/native-approvals.ini` remain readable.
Their exact DLL hash grants native-content approval when no scoped record exists for that ID.
These old records contain no capability history.
The first scoped review therefore shows all capabilities.

`--plugin-approve ID HASH` keeps its full native-content meaning.
It checks the current DLL hash, writes the legacy entry and removes any scoped entry for that ID.
This is an explicit operator action.
`--allow-unapproved-native` keeps its content-approval bypass.
Neither command skips manifest, API, path or dependency checks.

## Evidence

The CTest group `plugin-trust` checks the real loader and a minimal DLL fixture.
It checks rejection before DLL execution, malformed records, metadata changes, both legacy files and both legacy commands.
Separate removal probes isolate the payload hash, manifest hash, runtime and capability comparisons.
Each rejection probe fails with its comparison removed and passes after restoration.
