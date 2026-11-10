# Official plugin progress

## Workshop source move (2026-11-09)

The official plugin source and tests now live in the [Workshop](https://github.com/TheGreatVulpzin/SubVulpes-Workshop/tree/main/official/plugins).
The server package embeds `sv.permissions`, `sv.patchouli`, `sv.ayalog` and `sv.admcmd`, all enabled by default.
`plugins/status` stays here as the core fixture and SDK example. The server package does not carry it.
`tools/release/embedded-plugins.txt` pins the source and lists the default states.
The Workshop owns the plugin tests. This repository keeps the API, host, runner, SDK and package tests.
Validation after the move: the fresh MSVC x86 build and all 24 CTest groups pass.
The package check passes on repeat after the local Workshop checkout ownership fix.
Storage, native dependencies and repository checks pass.
The server zip carries four enabled plugins with matching DLL approvals and no status fixture.
Eleven plugin groups now run in the Workshop.
The two inspection groups run alone because their PowerShell process has a 30-second deadline.

The evidence below records the earlier work in this repository.

Work order: Part 1, permissions, admin commands, bans, chat additions, welcome and rules, then audit logs.

| Step | State | Evidence |
| --- | --- | --- |
| Part 1: N56, N57 and N51 | Done | Normal build, 25 test groups, nine removal proofs and restored tests passed. |
| 2.1: sv.permissions | Done | Normal build, all 27 test groups, 49 removal proofs and repository checks passed. |
| 2.2: sv.admcmd | Done | Normal build, all 29 groups, 36 removal proofs, final focused tests and repository checks passed. |
| 2.3: sv.bans | Open | Follows admin commands. |
| 2.4: sv.chatadd | Open | Follows bans. |
| 2.5: sv.motd | Open | Follows chat additions. |
| 2.6: sv.auditlogs | Open | Follows welcome and rules. |
| Packaging and final checks | Open | Follow all six plugins. |

Build and tests use `build/fresh` after `vcvars32`.
The server package must carry all six plugins. Permissions and admin commands start enabled.
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

Original client and manual launcher checks remain open.
External changes in `docs/ideas.md` remain outside these commits.

## Permission implementation evidence

The unit test and SDK scenarios passed before the full run.
The SDK loads the production DLL and a separate consumer through the public plugin service.
It tests account persistence across a fresh host, temporary and saved password groups, first-owner recovery and a bot without an account.

All 49 removal proofs failed their focused test as required.
They cover assignment nodes, group nodes, core nodes, levels, parent authority, owner protection and login refusal.
They also cover group and node quotas, names, node text, prefixes, record sizes, list strides and list capacity.
The remaining proofs cover parent depth, group indices, tokens, numeric ranges, message bytes, storage values and API prefixes.
The final proof covers private core replies to an allowed player without the admin flag.
The restored unit, SDK and game API tests passed.

Local logs: `build/planning/permissions-removals.log`, `permissions-extra-removals.log` and `permissions-reply-removal.log`.
Each proof has a separate build and test log in that folder. No build failure counted as a proof.
The prose page scored 1.69 per 100 words. The reply notes scored 1.82.

Core replies keep the inherited admin audience for administrators.
An allowed player without that flag receives a private normal message instead.
The packet test checks the recipient, another connection and the inherited admin route.
No original client playtest or manual launcher session ran.

## Disk space stop and repeat

The first full CTest run stopped during the SDK checks on 2026-10-31.
`plugin-inspect` failed with `OSError: [Errno 28] No space left on device` while it made an oversized test file.
About 62 MiB remained after that failure. The test process and its child processes were stopped.
No user files were deleted to make space. The owner freed disk space before the repeat run.
About 6.6 GiB was free at the start of that run.

The normal build passed again. All 27 CTest groups passed in 934.16 seconds.
The oversized-file case in `plugin-inspect` passed too.
Storage generation checked 138 objects and 499799832 bytes.
The native check passed for 91 sources. The repository check found no problems.
The earlier disk failure is resolved for this run.
The SDK and server package tests include the permission DLL and manifest.

Local logs: `build/planning/permissions-final-build.log`, `permissions-full-ctest.log`, `permissions-storage.log`,
`permissions-native.log` and `permissions-repo.log`.
The closure draft scored 1.08 per 100 words.
No original client playtest or manual launcher session ran.
External changes in `docs/ideas.md` remain outside these commits.

The owner chose a weapon selector for `/arm N [PLAYER]` and a separate `/kit N [PLAYER]`.
Commands that affect a player accept an optional target after their values.

## Admin Commands evidence

The full run passed all 29 CTest groups in 976.65 seconds.
All 36 removal proofs compiled and failed their focused test as required.
The restored native contracts, SDK scenarios and game API test passed.
No build failure counted as a proof.

The SDK tests cover all six weapons, the separate kit and optional player targets.
They also cover partial names, ambiguity, bots, a full hand, a full pocket and kit cleanup.
Permission cases cover a missing node, a missing service, live disable and live enable.
The native contracts cover API prefixes, function pointers, byte bounds, argument counts and the heavy input range.
The remaining proofs cover numeric limits, core service versions, dependency rules, the mass range and the velocity length.

The privacy check matched the parser local named token. That name became parsed, with no behavior change.
After that rename, the normal build, both admin groups and the server package check passed.
Storage generation, native dependencies and repository privacy checks passed too.
Explicit CMake dependencies make contract tests rebuild when they include the production command source.
The fixture runs normal item, bullet and body setup.
It sets the cached root position that a normal simulation tick supplies.
The public service lookup adds borrow at the end of SvPlugins version 1.
The existing find method keeps its dependency rules.

The plugin starts enabled. Its DLL and manifest are in the server package and the launcher update list.
The original client and manual launcher checks remain open.
The prose page scored 1.64 per 100 words. The API notes scored 0.44. The closure prose scored 0.30.

Local logs: build/planning/admcmd-removals.log, admcmd-restored-test.log, admcmd-full-ctest.log,
admcmd-closure-build.log, admcmd-closure-ctest.log, admcmd-storage.log, admcmd-native.log and admcmd-repo.log.

The owner asked to stop after Admin Commands for this session.
Next step: sv.bans (2.3), then chat additions, welcome and rules, and audit logs.
Packaging and the final suite remain open for those four plugins.
External changes in docs/ideas.md remain outside these commits.
