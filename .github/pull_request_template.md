## What this changes

<!-- Say what the change does and why. Name the need row (N number) if it adds to the plugin API. -->

## Checklist

- [ ] I read [CONTRIBUTING.md](../CONTRIBUTING.md).
- [ ] I ran the full test suite after a fresh rebuild. Commands and results are below.
- [ ] Game rules or client compatibility change: I state it in `docs/game/`. If not, I say "no change" below.

### Only for a plugin API change

- [ ] I added or updated the row in `docs/legacy-modules.md`.
- [ ] New methods are at the end of their table. Nothing that exists was renamed, removed or changed.
- [ ] I documented what each method returns when it fails, and a test covers each failure.
- [ ] A change to the game goes away when the plugin stops, or I explain why it does not.
- [ ] A plugin gets copies of data. It gets no pointer into the game, no account ID of a client and no raw address.
- [ ] I updated the docs, `tests/abi_snapshot.c` and `tests/abi_snapshot.txt`.
- [ ] I wrote "client effect unknown" where the original client might not show the change.
- [ ] Security checks (permissions, memory, overflow, privacy) have a removal proof: I took the check out and a test failed.

## What I ran

<!-- Paste the commands and the results. -->

## What I could not run

<!-- For example: no game playtest, no launcher check. Write "nothing" if you ran everything. -->
