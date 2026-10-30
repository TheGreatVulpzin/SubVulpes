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
The launcher dialog uses this review policy.

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

## Launcher install and update

The launcher reads `--plugin-inspect` before it opens the review dialog.
The dialog shows the display name, author, description, runtime and version.
It shows each capability with its risk color.
Native code appears first at the highest risk level.
Storage and reads use the low risk color. Writes and kick use the high risk color.
A new capability has a visible `NEW` label and needs the full review.
An update with the same ID, runtime and capability set shows the previous and new versions in one review.

The only approval action is **Install and approve**.
Native content requires an unchecked-by-default box for full process access.
The primary action stays disabled until the operator checks that box.
Cancel and Escape save nothing.

After consent, the launcher stops the server and calls the shared installer with `-Approve`.
It sends the reviewed ZIP, payload and manifest hashes, runtime and complete capability set.
The installer checks the staged files before it changes installed files.
It saves approval last with the same server command that checks installed content.
A refused review does not replace installed files or the previous approval.
New plugins stay disabled until the operator enables them.
Updates keep private plugin data and the previous enabled state.

`tests/sdk_approval_install_test.py` tests this path without the GUI.
It covers first installation, reviewed updates, private data, missing native consent and changed review inputs.
The launcher tests read real server output and check the real dialog controls, risk colors and review mode.
Removing the native consent guard makes the real dialog test fail.
The restored guard passes.
No live launcher visual check or Start/Stop check ran.
