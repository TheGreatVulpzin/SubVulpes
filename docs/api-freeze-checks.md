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
