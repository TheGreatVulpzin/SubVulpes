# Development notes for the plugin-game-api page

These sections describe how the host implements this API, the checks that proved it and the open work.
They are not part of the SDK reference.

## Validation and remaining limits

The fixture command is:

```powershell
ctest --test-dir build/server -R game-api --output-on-failure
```

The implementation is committed as `92854b1`.
The normal build exited with code 0. All six CTest groups passed in 34.71 seconds.
A focused `game-api` rerun after the build completed passed in 0.79 seconds.
Its local evidence is `build/server/Testing/Temporary/LastTest.log`.
The fixture reported `PUBLIC_READ_API_PASS` and `STOP_READ_DENIED`, and exercised real constructors and entity retirement paths.

The native dependency check passed with 74 sources.
Storage validation passed for 138 independent objects and 499799832 declared bytes.
The repository privacy check found zero problems in 262 files.
These checks describe this read slice, not later API work.

### Real server runs

Two real server cycles used the previous public framework package for data and its original status DLL.
The test setup used the current server executable and added the `test.read` fixture.
This was an isolated runtime check, not a new release package.
Both cycles reported `PUBLIC_READ_RUNTIME_PASS` and `STOP_READ_DENIED`, exercised live disable/enable and exited cleanly with code 0.

| Cycle | PID | Duration | Tick sample |
| --- | --- | --- | --- |
| First | 16928 | 64.53 seconds | 62.3 ticks/s, one sample |
| Second | 16076 | 3.54 seconds | No periodic sample |

The single tick sample is not a performance comparison or proof of sustained loaded-server behavior.
The result record is `build/game-read-review/94cd5dbd49d647ee816e5121ed4afbd3/result.json`.
The same directory contains `cycle-1.log` and `cycle-2.log`.
Its base package is `SubVulpes-server-v0.0.0-s3-framework.1.zip`.
The result records these exact SHA-256 identities:

| Component | SHA-256 |
| --- | --- |
| Current server | `6191ea5849e8662dd46c1721c8176b94cdef7075459ce77777670e5862ac77f8` |
| Read probe | `b6ac3f5927fbc608ca168850feee3094008cadd09fb944c0bc229e1b261d5a74` |
| Previous package's status DLL | `e5f2171160988276c6fa7994bd3be1f116dfbcc0c0972e110dc5932370242b11` |

The older status DLL remained usable with its original ABI prefix during these runs.
This does not establish compatibility for every third-party module.

### Remaining limits

The read slice introduces no public game mutation operations or original-client behavior changes.
The write capability below is a separate slice with its own limits.
Original-client gameplay and manual launcher acceptance remain pending.
The full mapped API and S3 remain open. Production gameplay and administration plugins follow the complete mapped API proof.
These results do not establish client gameplay parity, broader hook coverage, Lua or WASM support.
See [native SDK](../plugin-sdk.md) and [S3 evidence](../s3.md) for the framework contract and earlier package evidence.

## Test evidence removed from the reference page

The test starts with three lines.
Removing one check at a time made `game-api` fail each time (`build/logs/mutation22.log`): the gap in the index, the limit of 1,024 lines, the text length, the line count on read, the buffer capacity on read, the null buffer and the null text, the printable-text check, and the context checks of both methods.
For the move of a vehicle and of an item, the seat, the name and the speech, removing one check at a time made `game-api` fail each time: the train piece, the particle, wheel and wheel body bounds, the free slot, the held item, the position limit, the seat range, the seat taken, the human on foot, the name length and characters, the range and the dead human of a speech (`build/logs/mutation23.log`, `mutation23b.log` and `mutation24.log`).
For the restore of the type rows, the radius query and the two visual events, removing one check at a time made `game-api` fail each time: the host call at the stop, the first value, the owner match, the bounds of a restore row, the kind, capacity, radius, center and room guard of `within`, and the type, speed, weapon, kind and normal of the events (`build/logs/mutation25.log`, `mutation25b.log`, `mutation26.log` and `mutation26b.log`).
For the chat replacement, removing the character check, the empty check, the forced zero byte or the starting copy made `game-api` fail each time (`build/logs/mutation27.log` and `mutation27b.log`).
For the notice events (see [plugin events](plugin-events.md)), removing one check at a time made `game-api` fail each time: the index bounds of the five posts, the first look of the round, and the hooks for the exit of a vehicle, an item that goes in or out, a vehicle that goes and the seat of `vehicle_enter` (`build/logs/mutation28.log`). The test calls the post function for the shot.
The test program then reads the game records.
Removing one check at a time (message bytes, money range, flag range, health range, finite position, position limit, vehicle) made `game-api` fail each time.
For the item methods, removing the slot bound, the type bound, the check for a second parent item or the newspaper refusal made `game-api` fail each time (`build/logs/mutation10.log`).
For the object methods, removing the type bound, the color bound, the refusal to remove a train piece and the angle check made `game-api` fail each time (`build/logs/mutation11.log`).
The test program seats two humans in one car.
The earlier removal proofs of the range checks of the old methods (`mutation4.log`) were for code that no longer exists, and the table has its own proofs above.
For the bots, removing the cleanup after a failed human and the bot check made `game-api` fail each time (`build/logs/mutation12.log`).
Removing one check at a time made `game-api` fail each time (`build/logs/mutation14.log`, `mutation14b.log`, `mutation15.log`, `mutation15b.log`): the slot bound, the range of a set, the range of an add, the length and the finite check of a push, the bot check of the admin flag, the refusal of a train piece, the entity kind, the name length, the field function, the vector rules, the repeated name and the world entity rule.
The damage scales: removing the cap of the result, the index checks of the human, the shooter and the object tables, and the upper range of a float made `game-api` fail each time (`build/logs/mutation18.log`).
Not proven by removal: the bounds on the particle list of an object (the test world has no object with a bad list), and the choice of the order of the two scales.
Removal proofs for the slot bound and both range checks fail the game API test.
Removal proofs for the snapshot size and connection count fail the game API test.
The test room declares `network` and sends the address directly to private chat, without its usual chat log.
Tests cover both permissions, stable identifiers, distinct addresses, a shared /24, salt reuse and the capability removal proof.
Removal proofs cover timer capacity, delay bounds and owner checks.
The game API test covers both capability states, byte order, middle removal, a missing address and malformed counts.

## More test notes removed from the reference page

The tests call every method from a real plugin callback against records that real constructors made.
The seat count guard (a count above 7 gives `SV_FAILED`) has no test, because the fixture cannot break a seat count.
One bound of the restore code is redundant with the slot check of the table, and the test cannot reach it.
The length bound of 63 is redundant with the forced zero byte, and the test cannot reach it.
A local command of the fixture leaves too few bonds for a human and checks that no player stays behind.
Tests: the fixture calls every row and every code, and the test program checks the records and the events after the tick.
The tests run the chat function against a real player record.
The game API fixture checks all slots, both endpoints, addition and failed writes. No client playtest ran.
The fixture checks copies, bounds, bots and missing connections. No client playtest ran.
The fixture checks the closed file, failed writes, failed opens and the disconnect update. No client playtest ran.
`os_fclose` forwards `fclose` and returns 0 on success.
The flood test saves 2,000 accounts and checks one disk notice, with an earlier queued event intact and no loss notice.
The game API test sends a real UDP join over loopback through `serverrecv` and `add_connection`.
It checks the stored values and the public snapshot against the socket endpoint.

## Last removal-proof lines

For the checks that guard rights or privacy, the same removal made `game-api` fail too: the private-message filter, the stale-target test, the bot check and the connection admin flag of `set_admin`.
Removing the routing, the admin flag check, the zero-actor guard or the `admin` reservation made `game-api` fail each time.
The source is restored before final checks.
The source is restored before final checks.
Removal of the write context check fails the game API test. The source is restored before final checks.
The source is restored before final checks.

## Removed test lines

Two hooks have no test: the use button that seats a human (`human_entervehicle`) and the gun shot (`gun_shot`). It does not fire a gun.
They check each error code and a wrong-thread call.
It checks the two message events, the balances, the flag, the health, the bone offsets, the kick list and the idle counter.
They check the three levels, an unknown command, and a player with no reference.
