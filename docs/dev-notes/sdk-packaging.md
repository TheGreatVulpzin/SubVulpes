# Development notes for the sdk-packaging page

These sections describe how the host implements this API, the checks that proved it and the open work.
They are not part of the SDK reference.

## Test evidence removed from the reference page

- `tests/package_test.ps1` checks the package.
The test room is not in the list.
`tests/test_manifest.py` (group `manifest-rules`) runs the pack script on the status plugin.
Removal proof on 2026-10-28: without the check for an extra file, the test fails. The restored script passes.
`tests/sdk_package_test.ps1` (group `sdk-package`) builds the SDK, unpacks it in a clean folder and uses only what it holds.
Removal proof on 2026-10-28: with an internal header added to `include/`, the test fails. The restored script passes.
`tests/sdk_clean_test.ps1` (group `sdk-clean`) tests an empty folder outside the repository.
The test builds the template with MSVC discovery, packs it and installs it in a temporary server folder.
The test restores the environment and removes its temporary folder at the end.

## Tests

It checks the name, the entries, the same bytes in two runs, the validate script and the installer. It checks that the script refuses a folder with an extra file, a bad manifest and a missing library.

It compiles every public header alone and all together, builds the template with `build.bat` and with CMake, packs the result, checks it with the validate script and installs it on a test server.
It checks that the package is the same in two runs and that no internal header is in it.

It copies only the SDK ZIP and the server executable before it clears compiler state and replaces `PATH` with Windows paths.
It checks the hash, approval, enable and load commands, then runs the template scenario with the runner from the ZIP.
Pass `-SdkZip FILE` to test an exact release ZIP instead of a new test package.
