# Development notes for the plugin-shop page

These sections describe how the host implements this API, the checks that proved it and the open work.
They are not part of the SDK reference.

## Hazards read before implementation

- Native phone functions dereference linked indices without complete bounds checks.
  The API resolves references and checks the full ring or talk graph before any write.
- Inventory and child counts can exceed their arrays. A phone can also have two declared parents or duplicate links.
  The API checks parent bounds, counts, declared slots and uniqueness before it uses those arrays.
- A callback can remove or replace either entity, change hands, end a call or expire a phone.
  The helper resolves the generations again and checks the required state before native continuation.
- Native digit multiplication has no prefix bound. Core integration must bound the append before the safe decision.
  The helper accepts only a bounded resulting prefix.
- Catalog indices and counts can address another record. The named eight-row array also hides two native gun rows.
  The API bounds whole-record byte access and checks every replacement row before it copies any row.
- Native purchases skip constructor failures and can change money, counters or offers after allocation fails.
  Production catalog writes remain gated until the core owner fixes those paths.
- Native phone input and call lookup run inside simulation loops. Unrestricted plugin callbacks need the shared safe phase.
  The install flags do not replace that integration.

## Tests and work report

The five owned files are `src/plugins/sv_game_shop.h`, `src/plugins/plugin_shop.c`,
`tests/systems/shop_plugin.c`, `tests/systems/shop_host.c` and this page.
The fixture creates real type-23 items and attaches the main phone with native `linkitem`.
It calls every public method inside the tick and uses safe command callbacks for later checks.
Each command publishes success through a player-reference write and a host assertion.
The dispatcher return value alone does not prove a command passed.

The checks cover wrong owners and threads, stale and wrong-kind references, short records and unchanged outputs.
They cover ring and talk cleanup, malformed peer graphs, holder checks and cooldown preservation.
The host checks native event 6 with state zero, info zero and a preserved line value of three.
Catalog checks cover all three kinds, empty and full counts, invalid rows, atomic replacement and building 4095.
Host checks show that replacement changes neither entity counts nor player money.
Decision checks cover records, pass and claim, invalid inputs, callback revalidation, death threshold 9 and retirement at lifetime zero.
The test adapter checks the event size before reading its payload.

The shared dispatcher and property registry are absent from this branch.
The fixture uses adapters for decisions and write context. It does not prove production subscription or property delivery.
The implementation review used `rg`, native function reads and read-only `git show main:<path>` for the shared event contract.
The documentation draft used `build/shop-proof-original.c` while the removal script changed the live source temporarily.

The initial fast build and focused `systems-api` run passed. The initial test run took 4.55 seconds.
Thirty distinct removals compiled and made the systems test fail.
The cases cover context, write permission, record sizes, item/human/container/inventory bounds, phone scalar bounds,
building and catalog bounds, prices and types, feature gates, both core thread guards, holder identity, callback revalidation and replication.
Removing the item, human and container guards caused access faults.
The first holder comparison removal still passed because another invalid condition hid it.
The corrected fixture puts another real phone in a living human's same hand and makes that removal fail.
A first post-callback removal failed compilation because it left unused locals under `/WX`. It counted as no proof.
The corrected removal compiles and fails the test.
The script restores the exact source bytes and records `git diff` after each removal.
A final audit found thirty successful compiler logs, thirty failing tests and thirty identical restored source diffs.
The restored source built and passed `systems-api` again in 6.48 seconds.

| Command | Result |
| --- | --- |
| `Get-Content`, `rg`, read-only `git show main:<path>`, `git log`, `git status` and `git diff` | Inspected contracts, native functions, commits and file ownership. A wildcard-path probe and a missing-file probe failed. Corrected directory searches supplied the reads. |
| `call <vcvars32.bat>` then `cmake --build build\server --target sv-systems-test sv-systems-fixture` | Baseline and exact restored source passed. |
| `ctest --test-dir build/server -R systems-api --output-on-failure` | Initial run passed in 4.55 seconds. Final restored run passed in 6.48 seconds, including `shop_FIXTURE_PASS`, `SHOP_PASS` and `SYSTEMS_PASS`. |
| `python -I build/prove_shop.py` | Twenty-six removals compiled and failed the test. The initial holder removal passed the test, so the script stopped and restored the source. That attempt proved nothing. |
| `python -I build/prove_shop.py holder-match decision-postcheck replication` | The corrected holder test made the compiled removal fail. A post-check variant then failed compilation for unused locals. The script stopped and restored the source. |
| `python -I build/prove_shop.py retire-thread decision-postcheck replication` | All three corrected removals compiled and failed the test. The exact restored source built and passed. |
| Python audit of the thirty removal logs and restored diffs | All thirty current compiler logs passed, all thirty negative tests failed, and all thirty restored diffs matched. Source bytes matched the original backup. |
| CL `/W4 /WX /std:c11 /Zs`, with read-only main property headers and `/FIsv_game_props.h` | Passed before and after the proofs. An earlier relative forced-include path failed with C1083. The corrected include name passed. This is a syntax check, not a shared-registry runtime test. |
| `tools\build_hook.bat` | Passed for the normal server, launcher and test targets. |
| `ctest --test-dir build/server --output-on-failure` | All eight tests passed in 73.36 seconds. A temporary safe-directory environment allowed Git metadata reads. |
| `python -I tools/re/gen_storage.py --check` | Passed. Checked 138 storage objects and 499799832 bytes. |
| `python -I tools/ci/check_native.py` | Passed. Checked 82 native sources. |
| `python -I tools/ci/check_repo.py` and `git diff --check` | Passed. Checked 313 files with no repository or whitespace problems. |

Build outputs and proof logs stay in ignored `build/` paths.
All builds ran one at a time. No `WinError 1455` appeared.
Only the five owned shop files changed in this slice. No shared game, network, generated or third-party file changed.
The original client, real shared dispatcher, conditional property runtime, native call continuation and native purchases were not tested.
The fixture does not prove a full live world or safe pool exhaustion during native purchases.

## Area checkpoints

These six local commits hold the earlier area reports. Their command evidence and integration limits remain in those reports.

| Area | Commit | Report |
| --- | --- | --- |
| AI, N20 | `e9a0bd7` | [AI control](../plugin-ai.md) |
| Economy, N21 | `6a92b84` | [Economy](../plugin-economy.md) |
| Missions, N22 | `1b26172` | [Mission slots](../plugin-mission.md) |
| Core settings, N14 | `c51d9dc` | [Core settings](../plugin-settings.md) |
| Plugin storage, N18 | `c358c8e` | [Private storage](../plugin-storage.md) |
| Engine parameters, N17 | `ae10306` | [Engine parameters](../plugin-engine.md) |

The shop slice does not replace their core integration requests or close S3.
The earlier setters still need the shared kind-3 mutation audit before integration.

## For OverRosa

Phone cleanup uses native item-info event 6.
Its packet carries three state bits, sixteen info bits and two line bits.
Native menu 10 sends catalog type in eight bits, price in twenty-four bits and color in four bits.
The API keeps these formats, existing models and item types.
The original client was not run, so this is source evidence rather than a display or gameplay proof.

- Safe digit and call routing remains unbuilt in the shared game loops because those files have another owner.
  It needs the shared kind-2 safe point, not a new client packet.
- Production catalog writes remain disabled until native purchase allocation has the required checks.
  This is a core safety dependency, not a known client limit.
- Shared phone property registration has no runtime integration in this branch because the registry exists only on `main`.
  Conditional descriptors compile against its headers. They need no new client field.
- Automatic native calls with four entered digits that start with zero are not built.
  The native integer prefix loses leading zeros. A plugin can count digit decisions in N18 and claim its own number.
- New phone UI, custom shop displays and new item models are not built. The original client cannot display new protocol fields.
  Existing messages, menus and item types remain the supported path.
- A knife represented by item type 24 is not built. This server identifies that type as a vehicle key.
  A plugin must use a compatible existing type or wait for separate client support.

The effect of engine parameters on the original client remains unknown, as recorded in the engine report.
Earlier unbuilt operations and core requests remain listed in each linked area report.
The user must choose speed-range semantics and accept the unsupported-operation list before any later S3 closure.
This work does not close S3.

The current file-ownership rule prevents the earlier requested separate `docs/codex-report.md`.
This page and the six linked reports contain the area evidence. No merge, push or tag ran.

## Required core seams

`sv_shop_install` takes the shared decision function, shared write-context function, reserved bits and feature flags.
It rejects null functions, other event bits and unknown feature flags.
A successful call records the server thread and the adapters.
Installation can change the feature flags. If the shared registry exists, installation also requires successful property registration.

`SV_SHOP_SAFE_DECISIONS` is bit 1. `SV_SHOP_SAFE_PURCHASES` is bit 2.
The fixture sets both as assertions about its adapters. That does not prove production hook safety.
Catalog replacement returns `SV_STATE` until the core enables the safe-purchase bit.
Public phone cleanup also requires the shared write adapter.
Production does not install these adapters in this branch, so public writes remain gated.

The core owner must complete these integrations:

1. Supply the existing `sv_plugins_decide` and `sv_plugins_write_context`. Keep the reserved bits and the shared subscription rules.
2. Route digits and completed numbers through a kind-2 safe point.
   Native digit handling currently runs in `logic_playeractions`, and native call lookup runs inside `itemlogic`.
   Do not invoke unrestricted callbacks inside these simulation loops.
   In `item_action`, bound the old prefix to 0 through 999 and the new digit to 0 through 9 before the append.
   Deliver the digit decision after that append. Preserve the default path when no subscriber claims it.
   In the dialing branch of `phone_logic`, deliver the call decision before `phone_dial` clears the number.
   Resume these actions only after the safe callback and the helper revalidation.
   Honor the helper result before resetting digits, starting cooldown or looking for the native peer.
3. Call `sv_shop_phone_retire` before invalidating a phone reference or clearing its active flag.
   A lifetime of zero is valid for this cleanup. An invalid graph still returns an error without a partial cleanup.
4. Fix native purchase allocation before enabling `SV_SHOP_SAFE_PURCHASES`.
   `logic_gunstore_buy` can charge after item allocation fails.
   `logic_cardealer_buy` can charge, add a key and remove an offer after object allocation fails.
   Reserve or check the needed objects and items before committing money, counters or catalog changes.
5. Register the phone rows through the shared property registry and preserve its kind-3 write guard.
   Audit earlier area setters against `main`'s `sv_plugins_write_context` before integration.
   Keep their read access separate from the mutation guard.

Extend the shared release and poll aggregators with all required area hooks.
Do not replace an existing aggregator or drop another area's cleanup, pending work or callback order.
The common host owns event delivery, stop handling and callback phases.

## Removed from the reference page

`sv_shop_phone_decision` accepts `SV_PHONE_DIGIT_DECISION` (1) or `SV_PHONE_CALL_DECISION` (2).
The caller supplies stable phone and human references.
The phone must be the only item in hand 0 or 1 of that human.
The human must have health above `sv_engine_integer(SV_ENGINE_DEATH_START, 0)`.
The phone must be dialing, with zero cooldown and positive lifetime.
The prefix must be between zero and 9999. A call decision also requires a prefix of at least 1000.
The digit record contains the complete prefix after the core appends the bounded digit.

| Helper result | Meaning |
| --- | --- |
| `SV_OK` | The callback passed, and the references and required state still match. The core may continue the native action. |
| `SV_DENIED` | A callback claimed the request, or its writes invalidated the pending native action. |
| `SV_STATE` | The thread, readiness, feature flag, phone graph, holder or decision state is invalid. An unexpected adapter reply also gives this result. |
| `SV_INVALID` | The decision mode is invalid, or a resolved item is not a phone. |
| `SV_NOT_FOUND` | The phone or human reference no longer resolves. |

After a pass, the helper resolves both references again and checks the holder, hand, number, graph, state, cooldown, health and lifetime.
An item or human generation change cancels continuation.
A harmless health or lifetime change can pass if the human remains alive and the phone lifetime stays positive.
The helper does not undo a callback's writes or start the native call itself.

## Removed from the reference page

Both descriptors set `restore = 0`.
The state after-hook clears both link ends through the same cleanup function as `phone_idle`.
Property reads do not replace the full phone snapshot's graph and holder checks.
This branch lacks the shared registry. Its `sv_shop_props_install` returns `SV_STATE`.
The conditional registration and after-hook remain untested at runtime here.
