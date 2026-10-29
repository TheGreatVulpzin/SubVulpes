# SDK runtime checks

Checked on 2026-10-28. Archives and runtime sources stay under `build/runtimes/`.
The script checks the pinned SHA-256 before extraction, including each cached archive.

## Origin of each digest

| Runtime archive | SHA-256 | Digest origin |
| --- | --- | --- |
| `lua-5.4.9.tar.gz` | `2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6` | Publisher: [Lua download table](https://www.lua.org/ftp/) |
| `wasm3-0.5.0.tar.gz` | `b778dd72ee2251f4fe9e2666ee3fe1c26f06f517c3ffce572416db067546536c` | trust on first use |
| `wamr-2.4.5.tar.gz` | `1ab09d51099f276ca4a1d6629f6b589aab2bd0caa01445e05031a4bed22c199b` | trust on first use |

The Lua digest matches the publisher's row for version 5.4.9 and the downloaded archive.
The two GitHub digests come from the first source archive downloads over HTTPS.
A second local hash check matches each recorded value.

For wasm3, the [v0.5.0 release page](https://github.com/wasm3/wasm3/releases/tag/v0.5.0)
and its [release metadata](https://api.github.com/repos/wasm3/wasm3/releases/tags/v0.5.0)
list no publisher checksum or signed checksum for the downloaded source tarball.

For WAMR, the [2.4.5 release page](https://github.com/wasm-micro-runtime/wasm-micro-runtime/releases/tag/WAMR-2.4.5)
and its [release metadata](https://api.github.com/repos/wasm-micro-runtime/wasm-micro-runtime/releases/tags/WAMR-2.4.5)
list no publisher checksum or signed checksum for the downloaded source tarball.
The metadata gives digests for binary assets. Those are different files, so their digests do not apply to this archive.

These findings cover the checked release pages, release notes and asset lists.
The trust on first use pins detect later byte changes. They do not prove publisher identity or authorship.
The fetch command prints the origin beside each digest.

## Checks

Run `python -I tests/sdk/test_runtime_download.py` for the offline archive tests.
The cases cover fixed and changed bytes for both cached files and fresh downloads.
A changed download must not become the accepted cache file.
Removing either SHA-256 check fails a test. Restoring each check passes.

The normal build, all 11 CTest groups, storage, native and repository checks pass.
This change does not link a runtime into the server.
No client playtest, launcher Start/Stop test or soak test ran.
Build state after these checks: idle. The marker file is `build/state.txt`.

## D2 result

Use WAMR 2.4.5 with its fast interpreter, thread manager and an asynchronous stop.
It builds as MSVC x86 code, caps linear memory and stops the endless-loop probe.
This meets the three conditions of D2. The fallback to wasm3 is not needed.

The WAMR probe caps linear memory at 2 pages, or 131,072 bytes.
It also gives the runtime a fixed 2,097,152-byte allocation pool.
A watchdog asks `wasm_runtime_terminate` to stop the loop after a 10 ms wait.
The measured stop takes 17.082 ms, including thread startup and timer delay.
The probe joins the watchdog before it frees the instance.

WAMR's instruction counter does not stop the loop in the MSVC fast-dispatch path.
That first probe reaches its 5-second process timeout.
The pinned source omits the instruction check in its switch dispatch.
Do not use that counter as a work budget on this build.
WP9 still needs a separate work budget and depth proof. The watchdog proves the stop, not an instruction limit.


## Measurements

Each call mean covers 10,000 successful calls that return 42.
Lua includes a global lookup, a protected call and result removal.
WASM uses a resolved function. The numbers do not claim a general speed ratio.

| Runtime | Static library bytes | Probe EXE bytes | Mean call, microseconds | Endless-loop result |
| --- | ---: | ---: | ---: | --- |
| Lua 5.4.9 | 768678 | 444928 | 0.150 | Count hook stops at 10,000 instructions, 129.900 microseconds |
| wasm3 0.5.0 | 391728 | 217088 | 0.046 | No stop in the stock loop path. The probe process times out after 1 second. |
| WAMR 2.4.5 | 725842 | 294912 | 8.893 | Thread-manager stop, 17,081.700 microseconds |

Lua uses a custom allocator with a 131,072-byte cap.
A 1 MiB string request fails with `not enough memory`. Peak allocation is 17,729 bytes.
Closing the Lua state returns the tracked allocation count to zero.
The count hook stops the loop with `instruction limit`.

wasm3 uses `d_m3MaxLinearMemoryPages=2`.
A growth from 1 to 2 pages succeeds. Growth to 3 pages fails.
The stock wasm3 loop has no yield call, so this pin has no loop stop in the probe.
No sandbox claim depends on wasm3.

WAMR uses its instance limit for the same 2-page growth test.
WASI, AOT, JIT and SIMD stay off. The fast interpreter and thread manager stay on.
The static client clears `WASM_RUNTIME_API_EXTERN` to avoid DLL import calls.
The assembly uses `/safeseh`. The client keeps the normal SafeSEH linker check.

All three runtimes preserve x87 control word `0x027f` during calls and resource failures.
Each also computes `82 * 0.5 + 1` through its interpreter with control word `0x067f`.
That nondefault rounding mode remains unchanged. Each result is 42.
These probes do not replace the adapter rule to save and restore x87 state around every callback.

## Repeat and checks

Run `tools\sdk\spikes\build.bat` from the repository.
The command checks and extracts pins, generates the small WASM module, builds the probes and runs them.
It records BUILD START and BUILD END in `build/state.txt`.
It sets compiler temporary paths under `build/runtimes/`.
The WASM module contains call, floating-point, growth and endless-loop functions. It needs no external WASM compiler.

The offline archive tests reject changed cached bytes and changed download bytes.
Removing either hash check fails a test. Restoring each check passes.
Five more removals cover Lua memory, Lua work, wasm3 memory, WAMR memory and WAMR stop.
Each removal fails its probe. Each restoration passes.
The scripts and detailed logs stay under `build/`.

The normal server and launcher build passes. All 11 CTest groups pass.
Storage, native dependency, repository and prose checks pass.
The probes do not load into the server. They do not change S0.

No production adapter, capability sandbox, per-plugin runtime allocation policy or depth limit exists yet.
No client playtest, launcher Start/Stop test or soak test ran.
Build state after these checks: idle.

## Licenses

Lua and wasm3 use MIT licenses.
WAMR uses Apache 2.0 with the LLVM exception, as its [license](https://github.com/wasm-micro-runtime/wasm-micro-runtime/blob/WAMR-2.4.5/LICENSE) states.
The source location and runtime choice stay as the owner decided.
