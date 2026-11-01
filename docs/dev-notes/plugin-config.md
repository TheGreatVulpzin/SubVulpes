# Development notes for the plugin-config page

These sections describe how the host implements this API, the checks that proved it and the open work.
They are not part of the SDK reference.

## Test evidence removed from the reference page

The lifecycle test (`tests/test_plugins.py`, with the fixture of `tests/plugin_fixture.c`) runs:
Removing one check at a time made the lifecycle test fail each time (`build/logs/mutation20.log`): the range of an integer and of a decimal, the text length, the text characters, the limit of 32 settings, the limit of 8 choices, the size of a choice name, the length of a key, the administrator gate, the hiding of a secret in the event and in the listing, the reset of a retyped key, and the buffer capacity of `get_text`. The installer checks (the fields, the count, a repeated section and the `unsafe_events` value) have their own proofs (`build/logs/mutation21.log`).

## Tests

- every type, with the defaults read through the API, and every result code of the three getters;
- a live change with its event, a change that waits for a restart, and the default put back;
- ten wrong values (four for an integer, a flag, a choice, two decimals and two texts), an unknown key and an unknown plugin;
- a secret: it is saved, its length is read, and its text is in no output;
- an administrator who changes a key with `admin=1` and is refused on a key with `admin=0`, the listing for that player, and a player who is not an administrator;
- values that stay after a restart, and an update of the manifest (a kept key, a new key, a retyped key and removed keys);
- the file command: the listing, a value, the default, a value out of range, an unknown key and an unknown plugin;
- the listing beside a running server, and the refusal of a change then;
- the installer: a package with settings and `unsafe_events` installs and keeps its manifest, and eight wrong packages are refused (a value that is not 0 or 1, an unknown field, a repeated field, a repeated section, an uppercase key, another section, a section before `[plugin]`, and 33 sections);
- 19 wrong declarations, each one refused: unknown type, wrong range, default out of range (integer and decimal), wrong flag default, choice default outside the list, one choice, nine choices, a choice name that is too long, a secret with a default, a text default longer than the limit, `maxlen` of 64, an uppercase key, a key of 33 characters, no label, a `live` or `admin` value that is not 0 or 1, a semicolon in the label, and 33 settings.

Not proven: the launcher form (no automatic test runs a window), the reparse-point checks of the private file (they copy the pattern of the approval file), and a crash of the host in the middle of the write (the design leaves the old file, and the test does not kill the process).
