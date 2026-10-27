# Cleanup subplan

Small cleanups of inherited code style. They are not a milestone and they do not block one.
Fix an item only when the current task already touches that code, or when it is a few minutes of work.
Add new items here when you find them.

## Rules

- Keep behavior unchanged. Keep float conversions and x87 settings until tests prove the same output.
- Make one small commit for each item. Do not mix cleanup with a feature or a fix.
- Run the focused tests and `python -I tools/ci/check_repo.py` before each commit.
- Put field names in `analysis/structs/names.csv`. Do not assign a second name to an existing offset.
- Do not edit generated files by hand.

## Open items

- `NUM_SLOTS` has two meanings. It is 8 in `human_body.c` and 7 in `human_items.c`. Give each a clear name.

## Backport candidates

These are ideas to review for the sibling OpenSubRosa24 repository. Nothing here changes that repository.
A backport there must pass its own verification tools, which this repository does not have.

- The shared limit names from the first open item, when the same files exist there.
- The bond capacity guards and spawn rollback. They change behavior when a pool is full, so they need their own review there.

## Done

- Duplicate pool limits. `NUM_HUMANS`, `MAX_HUMANS` and `MAX_ITEMS` are gone from seven files in `src/game/` and `src/net/`.
  They now use `SR_MAX_HUMAN` and `SR_MAX_ITEM` from `sr_limits.h`. The values were equal (512 and 1024).
  The build and all seven CTest groups pass. No test fails on a changed pool size, so a wrong value would not show there.
