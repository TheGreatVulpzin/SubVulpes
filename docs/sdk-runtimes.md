# SDK runtime pins

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
