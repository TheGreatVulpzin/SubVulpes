# Development notes for the sdk-approval page

These sections describe how the host implements this API, the checks that proved it and the open work.
They are not part of the SDK reference.

## Test evidence removed from the reference page

Separate removal probes isolate the payload hash, manifest hash, runtime and capability comparisons.
`tests/sdk_approval_install_test.py` tests this path without the GUI.
Separate removal probes cover the persistent-command guard, outside-folder approval and the folder boundary.

## More test notes removed from the reference page

The CTest group `plugin-trust` checks the real loader and a minimal DLL fixture.

## Evidence

It checks rejection before DLL execution, malformed records, metadata changes, both legacy files and both legacy commands.
Each rejection probe fails with its comparison removed and passes after restoration.

## Removed test lines

It covers first installation, reviewed updates, private data, missing native consent and changed review inputs.
The launcher tests read real server output and check the real dialog controls, risk colors and review mode.
