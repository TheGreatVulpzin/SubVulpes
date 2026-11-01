# Development notes for the sdk-test-runner page

These sections describe how the host implements this API, the checks that proved it and the open work.
They are not part of the SDK reference.

## Test evidence removed from the reference page

`tests/scenario_test.py` (group `plugin-scenarios`) runs the scenario of the status plugin twice and compares the output.
`tests/sdk_package_test.ps1` runs the scenario of the template with the runner from the unpacked SDK.
Removal proof on 2026-10-28: without the limit of 63 characters for a chat text, the test fails, because the long text overflows the buffer of the runner. The restored runner passes.

## Tests

It runs 5 scenarios that pass and 16 that must fail, one for each kind of wrong step, and one plugin with a bad manifest that must not reach the runner.
