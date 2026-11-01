# Development notes for the plugin-settings page

These sections describe how the host implements this API, the checks that proved it and the open work.
They are not part of the SDK reference.

## Core seams

This branch lacks the shared N16 event source files on `main`.
It also lacks settings reads in the two native startup consumers.
For this reason, writes return `SV_STATE` until the core calls `sv_settings_install`.
The fixture installs a test queue adapter. This does not prove production delivery.
No second event dispatcher or subscription mechanism exists in this area.

The first editor must make these changes in owned core files:

- In `src/game/account_db.c`, replace only the new account's `a->money = 1000` expression with
  `sv_settings_native(SV_SETTING_NEW_ACCOUNT_MONEY, 1000)`. Keep the existing-account early return.
- In `src/game/logic_sim.c`, replace the traffic counts 32 and 24 with
  `sv_settings_native(SV_SETTING_BOT_COUNT, 32)` and `sv_settings_native(SV_SETTING_BOT_COUNT, 24)`.
  Keep the existing creation points. Guard the native constructor and its failure cleanup before installation.
- Add a setting-change row and typed payload case to the shared N16 queue.
  The copied record is `SvSettingEvent`. It starts with `SvEvent base` and includes the name, owner, type and before/after values.
  Select an unused event bit in `sv_plugin.h`. The area does not assign a production bit.
- After these consumers exist, call `sv_settings_install(bit, post)` from core initialization.
  The `post` adapter must copy the whole record to the shared queue without invoking a plugin.
  It returns zero without queuing anything when full. Otherwise it returns one.
  Deliver the copied notice before the next tick callback, after the setter commits the value.

The setter reserves the notice before it changes any setting.
No failure can change a setting without its notice. A full queue causes `SV_LIMIT` and no change.
The test bit 1024 is an adapter value only. It does not reserve a shared ABI bit.
The install function refuses null callbacks, tick/stop bits and masks with multiple bits.
`sv_settings_native` rejects an index outside the table and keeps each call site's fallback until a plugin sets that row.
This keeps both native bot defaults. It does not add settings to unused game fields.

## Hazards read before implementation

- `src/game/options.c` reads only the player limit among these rows. It has no money or bot-count field.
- `src/server/main_loop.c` copies the player limit once at startup. A runtime write must update the option and live limit together.
- `src/net/server.c` checks the live limit during joins. Reducing it must not delete current players.
- `src/game/account_db.c` returns existing accounts before it initializes a new record.
  Applying the setting before that return would overwrite existing money.
- Account money uses a signed native integer and four disk bytes. The setter refuses negative values.
- `src/game/scenario.c` uses shared player, object and human pools for traffic.
  Its constructor stops on allocation failure, but its cleanup calls `deleteplayer` for a team-6 bot.
  `deleteplayer` calls stock sales twice, and stock sales index a company without a team bound.
  Traffic also assumes valid road endpoints and intersections.
  Core integration needs guards in `src/game/scenario.c`, `src/game/player.c` and `src/game/economy.c` before installation.
  The setter bounds the request to the 256-player pool and does not promise that many bots.
- Native world and round setup use different traffic defaults. An unset row keeps the call-site fallback.
- Short output records, invalid row indices and overlong names must not cause table or buffer access outside their bounds.
- The shared event queue can fill. The setting changes only after the queue accepts a copied notice.

## Work report

Changed files: `src/plugins/sv_game_settings.h`, `src/plugins/plugin_settings.c`,
`tests/systems/settings_plugin.c`, `tests/systems/settings_host.c` and this page.
Read-only inspection used `Get-Content`, `rg` and `git show main:docs/legacy-modules.md`.
The investigator read the constructors and listed their initialization and consumer sites.
An initial build found a wrong test field and a missing native include. Both were corrected.
The first systems run then found a shared-account test assumption. The AI host calls `playersimulation` and synchronizes that balance.
The settings test now uses a separate account from the real constructor, detached from that native balance synchronization.
No shared source file, game record layout, protocol source or account file changed. No merge, push or tag ran.

The tick fixture calls every method and every result path available with the adapter installed.
It tests wrong owners, a wrong thread, short records, invalid names, type/range checks, queue exhaustion and unchanged outputs.
The host checks all three copied notices, exact native fields, existing-account preservation and both traffic fallbacks.
A second safe callback checks the upper boundaries. Its outcome uses an existing public write and a host assertion.
No entity reference test applies to these global methods.

The first removal run found that the short-input test also had an invalid value.
That removal compiled but passed the test, so it proved nothing. The test now isolates the size check with a valid value.
| Command | Result |
| --- | --- |
| `call <vcvars32.bat>` then `cmake --build build\server --target sv-systems-test sv-systems-fixture` | Passed after the include and test-field corrections. |
| `ctest --test-dir build/server -R systems-api --output-on-failure` | Passed after the account test used a separate constructor-created account. |
| `python -I build/prove_settings.py` | Eleven distinct removals compiled and made the systems test fail. The exact restored source built and passed the test. |
| `tools\build_hook.bat` | Passed for the normal server, launcher and all test targets. |
| `ctest --test-dir build/server --output-on-failure` | All eight tests passed in 65.26 seconds. A temporary safe-directory environment allowed Git metadata reads. |
| `python -I tools/re/gen_storage.py --check` | Passed. Checked 138 storage objects and 499799832 bytes. |
| `python -I tools/ci/check_native.py` | Passed. Checked 82 native sources. |
| `python -I tools/ci/check_repo.py` and `git diff --check` | Passed. Checked 310 files without private-file or whitespace errors. |

The removal cases cover callback context, descriptor index, three record sizes, name length, type,
both range ends, the native row bound and queue refusal.
The script checks every build result and records `git diff` after each exact source restoration.
The short-record case from the first run does not count as a proof until its isolated test fails.
All proof scripts and logs stay in ignored `build/` paths.
The original client, production consumers, real shared event delivery, native traffic creation at maximum count,
full world startup and options persistence were not tested.

## For OverRosa

No implemented setting requires a new client event or field.
The existing server-info and balance messages can carry these values. The original client was not run.
Live addition or removal of traffic bots is not built. This row controls the next native creation pass.
The native-consumer and N16 hooks above remain core integration requests, not client limitations.
