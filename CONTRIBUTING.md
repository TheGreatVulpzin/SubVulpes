# Contributing

Report bugs and propose changes in the SubVulpes repository.
For a large change, discuss the scope in an issue first.
Read [the plan](docs/plano.md) and [development rules](CLAUDE.md).

Write code, comments, docs and commits in English. Keep comments short and explain reasons.
State any change to game rules or client compatibility. Add focused tests for that behavior.
Build with `tools\build_hook.bat`. Run `python -I tools/ci/check_repo.py`.
For runtime changes, run the server with public test data and check launcher Start and Stop.
Include the commands and results in the pull request. State checks you could not run.

Use [the security policy](SECURITY.md) for vulnerabilities.
Never post passwords, tokens, `config.txt`, accounts, captures, dumps or proprietary game files.
Check and redact logs before sharing them.

## Changing the plugin API

The plugin API can only grow. Please don't rename, remove or change anything that already exists: a method, a property, a record or a number. Plugins built against ABI 1.0 have to keep working. If there is a reason to do any of that, please explain on the Pull Request.

When your change adds to the API:

- **Add a row to the need list** in `docs/legacy-modules.md`, or mark the existing row as done.
- **Put new methods at the end** of their table. Plugins check the table size before they call a method.
- **Say how each method behaves.** Does it run in a safe callback or anywhere? What are its bounds? What does it return when it fails (`SV_INVALID`, `SV_DENIED`, `SV_STATE`, `SV_NOT_FOUND`)? Write that in the docs and test each failure.
- **Undo your writes.** If a method changes the game, the change goes away when the plugin stops.
- **Copy data out.** Plugins should never get a pointer into the game, a client's account ID or a raw address.
- **Update the docs and the ABI snapshot:** `docs/plugin-game-api.md` (or the page for your area), `tests/abi_snapshot.c` and `tests/abi_snapshot.txt`.
- **Be honest about the client effects.** If the original client might not show your change, write "client effect unknown".

Security checks (permissions, memory, overflow, privacy) also need a removal proof: take the check out and show that a test fails.

Run the full test suite after a fresh rebuild. In the pull request, list what you ran and what you couldn't run.

The Branch/Pull Request will be reviewed fully and we're going to see if it fits our standards for approval.
