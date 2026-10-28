# SDK progress

## M1: native SDK

WP0 is complete. WAMR passes the x86 build, memory cap and loop-stop checks.
The runtime pins and their sources are in [the runtime report](sdk-runtimes.md).

The first WP1 change adds display metadata to the launcher.
The list accepts the old rows and the new rows with runtime, capabilities and display fields.
The display name and the stable ID have separate columns.
Every plugin action uses the ID.
The review shows the runtime, capabilities, author, description, homepage and hash.
Native approval always shows the full-process warning, even when a cached row names another runtime.
The file dialog accepts `.svplugin` and `.zip`.

The launcher test covers old rows, new rows, identity, bounds, characters, runtime, capabilities, homepage and hashes.
Nine removal checks fail with their check removed and pass after restoration.
The test catches errors and returns 1, so expected failures do not open a Windows crash dialog.
No visual launcher check, Start/Stop check or client playtest ran for this change.

WP1 is complete (2026-10-28). The server reads `runtime`, `capabilities`, `name`, `description`, `author` and `homepage`. The capability list is a wall at `get_api`. The command `--plugin-validate-folder` prints every problem, and `tools/plugins/validate_plugin.ps1` checks a folder or a package. The installer asks the server before it copies a file. See [the manifest page](plugin-manifest.md). Three removal proofs hold.
One change from the plan: the server command checks a folder only. A ZIP needs a reader in C, so the PowerShell script unpacks a package and calls the server.

WP2 is complete (2026-10-28). `sv_add_plugin` builds a plugin, `tools/sdk/pack.ps1` makes `ID-VERSION-RUNTIME.svplugin`, and `plugins/official.ini` drives the approval file and the server package. See [the packaging page](sdk-packaging.md). One removal proof holds.

WP3 is complete (2026-10-28). `make_sdk.ps1` makes the SDK package, and the test builds a plugin from the unpacked package only. See [the packaging page](sdk-packaging.md). One removal proof holds.
WP4 remains open. The SDK has no test runner yet. No SDK package is published.

Build state after the checks: idle. The marker file is `build/state.txt`.
