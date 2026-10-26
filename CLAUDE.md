# SubVulpes development rules

SubVulpes is a modified server, based on OpenSubRosa24 at `svs-base` (`eb68519b0982583e188ceb619ff49f14f332e6d9`).
Read `docs/plano.md` before changes. S0 establishes the runnable Windows x86 base.
Core rule changes belong here. OverRosa is the future modified client.
Keep simulation, protocol handling and unconditional memory/representation checks in the core.
Plan new administration and configurable policies as official plugins, with required modules where needed.
Official plugins use the public API and separate versions/packages so compatible updates do not require a core rebuild.

## Scope and build

- Build the normal server and launcher with `tools\build_hook.bat`.
- The server uses direct C calls and separate typed objects in `src/server/storage.c`.
- Keep image allocation, address dispatch, lifted sources and verification adapters out of the server target.
- Run `python -I tools/re/gen_storage.py --check` and `python -I tools/ci/check_native.py` after storage or build changes.
  The native dependency check needs a normal build first.
- There is no gate, lifter, harness, hook DLL or recording target in this repository.
- Preserve S0 runtime behavior until a later milestone explicitly changes it.
- Use focused tests, local server runs and launcher Start/Stop checks for changes.
- Run `python -I tools/ci/check_repo.py` before each commit.
- Run `python -I tools/re/gen_types.py` after type metadata changes. Check its generated diff.
- Put field names in `analysis/structs/names.csv`. Do not assign a second name to an existing offset.
- Do not edit generated type headers by hand. Keep their size and offset assertions.
- `analysis/structs/layout_reference.h` is a generator-only record-size reference. It is not a runtime dependency.
- `tools/re/gen_storage.py` owns storage definitions and declarations. Its inputs stay in `analysis/structs/`.
- Do not edit generated storage files by hand. Preserve the checked defaults and shared storage aliases.

## Code and text

- Write code, comments, docs and commit messages in English. Discuss work with the user in Portuguese (BR).
- Use clear C and short functions. Keep changes within the requested milestone.
- Use short comments with active voice and simple words. Explain reasons and behavior, not development history.
- Keep float conversions and x87 settings until a deliberate behavior change has tests.
- Use `os_rand()` for game randomness. Do not replace it with CRT `rand()`.
- State behavior changes and compatibility limits in `docs/game/`.
- Use SubVulpes in text. Use `subvulpes_server.exe`, `subvulpes_launcher.exe` and `SubVulpes-server-<version>.zip`.
- The launcher title is `SubVulpes Server`. Its update endpoint uses the SubVulpes repository.
- Make small commits in English. Do not add AI coauthor lines. Do not push or create tags without a user request.

## Privacy and ownership

- Never commit game executables, assets, accounts, passwords, tokens, `config.txt`, logs, dumps or recordings.
- Keep test data and build outputs in ignored `build/` folders. Do not copy private settings into reports.
- Keep `original/` and `third_party/` out of this repository.
- Public initial values may remain as typed C initializers in `src/server/storage.c` and their checked metadata.
  Do not embed executable sections or game binaries.
- Download approved game data from the upstream data release. Use only the public default config template.
- Check logs and screenshots for private data before sharing them.
- The GPL covers project code. Game assets and binaries belong to Cryptic Sea.
