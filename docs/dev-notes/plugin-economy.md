# Development notes for the plugin-economy page

These sections describe how the host implements this API, the checks that proved it and the open work.
They are not part of the SDK reference.

## Source hazards

The implementation read `economy.c`, `player.c`, `logic.h`, finance event writers and their packet cases before adding methods.

- There are five companies. An unchecked player team can index outside the stock array.
- Native stock functions accept pool indices. The API resolves a player reference before any write.
- A zero share count can cause a NaN cash fraction in later game logic.
  The API keeps the share count positive.
- A split multiplies all active holdings and company shares. A signed overflow can damage both balances.
  The API checks the entire split before it applies any change.
- Buying and selling stocks charge money and fees. They cannot implement an isolated stock grant.
  The API changes both stock counters without a cash transfer.
- Negative holdings or holdings greater than total shares break the ledger invariant.
  The API refuses that state before a price or stock change.
- The stock event converts all five prices and packs two pairs into signed integers.
  The API checks every other price before it calls the writer.
  It refuses nonfinite prices and prices outside 10 to 32767 in those existing records.
- Native economy simulation also contains money conversions and payouts.
  This slice does not prove every later valuation, sale, tax or payout safe for every possible balance.

The original game float and x87 code remains unchanged.
The new price operation retains the original split coefficient and float store.
The methods allocate no memory, use no random numbers and create no new event type.
They contain no module name, item, reward price or other module policy.

## Tests and report

Changed files are `src/plugins/sv_game_economy.h`, `src/plugins/plugin_economy.c`,
`tests/systems/economy_plugin.c`, `tests/systems/economy_host.c` and this page.
The test uses real player constructors and the existing systems world.
The plugin calls every method during the tick.
It checks each result code, short outputs, unchanged outputs, wrong references and wrong-thread calls.
The host checks share totals, player holdings, unchanged cash, opening prices and stakes.
It also checks that existing finance and stock events contain the new state.
The price tests cover 499, 500, 2500, NaN, infinity and a very large finite value.
The overflow tests cover `INT32_MAX` and a removal that would make the company share count zero.

Inspection used `Get-Content` and `rg` on the source files above and the systems fixtures.
Some exploratory paths did not exist. The verified server flag is `sr_data_server.active`.
The normal server sets that flag in `main_loop.c`.

| Command | Result |
| --- | --- |
| `call <vcvars32.bat>` then `cmake --build build\server --target sv-systems-test sv-systems-fixture` | Passed. |
| `ctest --test-dir build/server -R systems-api --output-on-failure` | The first host check failed because the fixture did not enable server events. The fixture now sets the real server flag. |
| The same focused test after the server flag change | Found a test-order error: a malformed other price returned `SV_STATE` before the large-price overflow check. The test now repairs that state first. |
| The focused test after both fixture corrections | Passed. |
| `python -I build/prove_economy.py` | Twelve removals compiled and made the systems test fail. The restored build and systems test passed. |
| `python -I build/prove_economy.py positive-share-count` | A thirteenth removal compiled and failed the test. The restored build and test passed. |
| `tools\build_hook.bat` | Normal server, launcher and all test targets passed. |
| `ctest --test-dir build/server --output-on-failure` | All eight tests passed in 73.34 seconds. A temporary Git safe-directory environment allowed worktree metadata reads. |
| `python -I tools/re/gen_storage.py --check` | Passed. Checked 138 storage objects. |
| `python -I tools/ci/check_native.py` | Passed. Checked 82 native sources. |
| `python -I tools/ci/check_repo.py` and `git diff --check` | Passed. Checked 308 files without private-file or whitespace errors. |

The removals cover context, snapshot index and size, both company-write indices, player team index,
split overflow, stock arithmetic, packed prices, negative holdings, share conservation and finite input.
The script restores exact source bytes and records a Git diff after each removal.
Proof scripts, binaries and logs stay in ignored `build/` paths.

## Requests and limits

An exported, validated stock-split helper would let this API and the game share one implementation.
That requires a change to `economy.c` and its internal header, which this area does not own.
The present wrapper preserves the source formula and leaves the native game implementation untouched.
The original client, a full world market, account persistence, sales, taxes and later payout overflow were not tested.

## For OverRosa

- None of the requested N21 operations is deferred for a client protocol change.
  They use the existing finance and stock-price messages and server-side ledger state.
- New market menus, fractional-price UI and new client fields are not built.
  They are outside this slice and would need client support if a future plugin requires them.
