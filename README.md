# SubVulpes

SubVulpes is a modified Sub Rosa 0.24 server, based on OpenSubRosa24.
It supports changes to core rules, admin tools and a future plugin API.
The future modified client is OverRosa.

Source: `svs-base`, commit `eb68519b0982583e188ceb619ff49f14f332e6d9`.
This repository started with a source import because `git filter-repo` was not installed.
It now includes the direct-call and native-storage changes from upstream commits `ed4aef5` and `6e56d22`.
It does not contain the upstream verification tools or proprietary binaries.

Status: **S1 and S2 code migration imported**. The Windows x86 server uses direct C calls and owned typed storage.
See [synchronization checks and limits](docs/upstream-sync.md). Earlier checks remain in [S0 results](docs/s0.md).
Later milestones are in [the plan](docs/plano.md).

## Build and run

Install MSVC x86 Build Tools, CMake and Python 3. The launcher uses the Windows .NET Framework 4 compiler.
Open an x86 Native Tools prompt, or set `VCVARS` to your `vcvars32.bat` path.

```bat
tools\build_hook.bat
python -I tools\ci\check_repo.py
python -I tools\re\gen_storage.py --check
python -I tools\ci\check_native.py
```

Outputs: `build/server/subvulpes_server.exe` and `build/launcher/subvulpes_launcher.exe`.
Download `server-data-0.24.zip` and its checksum from the [upstream data release](https://github.com/TheGreatVulpzin/OpenSubRosa24/releases/tag/server-data-0.24).
Check and extract the data with `tools/release/expand_data_zip.ps1`.
That release contains only `data/`. The public default config comes from the source tag in `tools/release/default-config.ini`.

```powershell
.\tools\release\make_package.ps1 -Version v0.0.0-s0 -DataDir build\data-checked\data
```

Extract `build/package/SubVulpes-server-v0.0.0-s0.zip` into a test folder.
Open `subvulpes_launcher.exe`, set your server settings, then click **Start** and **Stop**.
The window title is **SubVulpes Server**. The default UDP port is 27584.
Set an admin password before public use. Keep `config.txt`, accounts, dumps and logs private.
Launcher updates come from SubVulpes releases.

## AI Disclaimer / Aviso de IA
**(PT-BR)**
Esse repositório foi feito com a ajuda de OpenAI Codex e Claude Code para a escrita do código, decompilamento e montagem de instrumentação. Apesar disso, vou falar que eu tive que ficar orientando muito a IA pra fazer o que eu queria...

**(EN-US)**
This repository was created with the help of OpenAI Codex and Claude Code for writing the code, decompiling, and setting up instrumentation. That said, I have to admit that I had to give the AI a lot of guidance to get it to do what I wanted...

## License

The code written for this project is licensed under the GNU General Public License v3.0. See [LICENSE](LICENSE).
This license covers only the code written for this project. It does not cover the Sub Rosa game. Its assets and binaries belong to the original creators.

## Legal notice

Sub Rosa is owned by Cryptic Sea. This repository does not contain original binaries or assets.
You need your own copy of the game to use some tools.
This project is not made by or linked to Cryptic Sea.
