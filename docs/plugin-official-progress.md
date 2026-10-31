# Official plugin progress

Work order: Part 1, permissions, admin commands, bans, chat additions, welcome and rules, then audit logs.

| Step | State | Evidence |
| --- | --- | --- |
| Part 1: N56, N57 and N51 | Done | Normal build, 25 test groups, nine removal proofs and restored tests passed. |
| 2.1: sv.permissions | Open | Follows the Part 1 checks. |
| 2.2: sv.admcmd | Open | Follows permissions. |
| 2.3: sv.bans | Open | Follows admin commands. |
| 2.4: sv.chatadd | Open | Follows bans. |
| 2.5: sv.motd | Open | Follows chat additions. |
| 2.6: sv.auditlogs | Open | Follows welcome and rules. |
| Packaging and final checks | Open | Follow all six plugins. |

Build and tests use `build/fresh` after `vcvars32`.
The server package must carry all six plugins. Only permissions starts enabled.
No client playtest or launcher window check ran for this work.

# Part 1 evidence

The normal server and launcher build passed in `build/fresh`.
All 25 CTest groups passed across the full run and four focused repeats.
The first run hit memory pressure in `jukebox-sdk` and `plugin-inspect`.
The owner cleared memory before the repeats. Both tests then passed.
The launcher and package checks now receive the selected build folder. Their repeats passed.

Nine removal proofs made `game-api` fail: core deny, core allow, login deny, password redaction, the chat byte bound,
decision precedence, the network capability, the lower kick count bound and the upper kick count bound.
The final three proofs ran again after the duplicate-address case changed. The restored test passed.
`kicked_remove` removes every entry for its address, so duplicate entries cannot leave that address blocked.

Local evidence is in `build/planning/part1-full-ctest.log`, `part1-retry-ctest.log`, `part1-removals.log` and `part1-kick-removals.log`.
Storage generation, native dependencies and repository privacy checks passed.
The added prose scored 1.20 per 100 words before this evidence update. The evidence update scored 0.57 per 100 words.

Next step: implement `sv.permissions` (2.1).
Original client and manual launcher checks remain open.
External changes in `docs/ideas.md` remain outside these commits.
