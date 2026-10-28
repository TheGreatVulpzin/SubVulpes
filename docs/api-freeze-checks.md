# API freeze checks

Base: `65effe6`. Branch: `codex/sdk`. Host: Windows x86, MSVC 14.50, Release.

## F1: ABI snapshot

`abi-snapshot` checks 53 public record sizes and 14 callback tables.
It checks each table size, function count, and first and last callback offsets.
The list includes the 51 records in the brief, plus `SvVector` and `SvShopRow`.
The test uses the same MSVC x86 C11 build as the server.

The removal proof changes `SvStatus.uptime_ms` from `uint32_t` to `uint64_t` in an isolated header copy under `build/`.
The comparison fails: `SvStatus size=20` becomes `SvStatus size=24`.
The proof restores the header copy. The source headers and expected snapshot stay unchanged.

The normal build and all 9 CTest groups pass.
The storage generator, native dependency check and repository check pass.
No client or launcher Start/Stop test ran for this test-only change.

## F3: deferred action gate checks

The internal action gate holds at most 256 pairs of event kind and key.
It returns at once with no subscriber. It never calls a plugin inside the simulation.
The next poll delivers each decision. The next gate call consumes the answer once.
A full table permits the action. A stale key permits the action without a plugin call.

The core poll delivers phone digits before queued calls.
A claimed digit cancels the pending call. A claimed call returns the phone to idle.
The phone adapter keeps the original reference and number while the call waits.
A changed number or a replacement phone cannot receive the older decision.
A count lets the digit poll skip its scan when no digit waits.

The gate clears at start, stop and world reset. A delivery can cancel its own entry.
A recursive poll does nothing. Work added during delivery waits for the next poll.
An action from a button edge must run inside its delivery when allowed.
Its delivery must then cancel the entry. The gate header states this rule.

Focused game, system and ABI tests pass.
The tests cover no subscriber, allow, deny, repeated gates, stale keys, table exhaustion, cancellation and reset.
They also cover recursive polling, work added during delivery, phone replacement and a changed number.
The existing phone hold assertions pass with the same digit-before-call poll order.

Removal proofs cover table capacity, denial, pending holds, no-subscriber behavior, phone generation, phone number and reset cleanup.
Each removal fails its focused test. Each source restoration passes that test.
The scripts and logs stay under build/. No removal remains in the source.

The full normal build, all 9 CTest groups, storage, native and repository checks pass.
Public record sizes and table offsets stay unchanged.
No client playtest or launcher Start/Stop test ran.


## F4: hook cost

The timing run uses QueryPerformanceCounter and the same game sources as the server.
The combined hooks take a mean of 1.910 microseconds for 256 bots on the recorded host.
The [Cost section](plugin-abi.md#cost) records each hook, one cold idle tick and dense and sparse spatial queries.
The digit poll already skips its scan when no digit waits. No further performance change is needed.

The normal build and all 10 CTest groups pass.
Storage, native, repository and prose checks pass.
The timing test checks complete output and valid query results. It does not enforce a time threshold.
No client playtest, launcher Start/Stop test or soak test ran.

F1 through F4 are complete. The native ABI layout stays unchanged.
The SDK work starts with WP0. No SDK package ships yet.
