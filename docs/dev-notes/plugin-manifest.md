# Development notes for the plugin-manifest page

These sections describe how the host implements this API, the checks that proved it and the open work.
They are not part of the SDK reference.

## Test evidence removed from the reference page

`tests/test_manifest.py` (group `manifest-rules`) covers the valid and invalid values of each new key, the report for several problems at once, the script with a good package, a bad package and an unsafe entry, and the installer.
`tests/test_plugins.py` covers the capability wall in the mode `wall`.
Removal proofs on 2026-10-28: without the capability check in `get_api`, without the unsafe-name check of the script, and without the server check in the installer, a test fails each time. The restored source passes.
The test room declares `network` and sends the address directly to private chat, without its usual chat log.
Tests cover both permissions, stable identifiers, distinct addresses, a shared /24, salt reuse and the capability removal proof.

## Removed test lines

The focused CTest group is `plugin-inspect`.
It checks metadata, all three hashes, changed metadata, changed payloads, risk lines, invalid packages and read-only behavior.
