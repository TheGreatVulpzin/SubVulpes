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
