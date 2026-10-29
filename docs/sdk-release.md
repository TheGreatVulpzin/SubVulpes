# SDK release checks

Prepare a local SDK ZIP. This flow does not publish files or create a tag.
Use the `codex/sdk` worktree and a Windows x86 MSVC developer prompt.
Run one build at a time. Wait for a build in the other worktree to stop.

## Source and version

Check `git status --short --branch`. Start from a clean branch with the API changes that the owner chose.
Do not merge a moving `main` during these checks.
Read `docs/sdk-progress.md` for open tasks and limits.
Choose a version such as `v0.1.0-rc1`. Use the same version for the SDK ZIP and its matching server release.
The ABI reset needs new DLLs. An old DLL with the same version label does not prove compatibility.

## Build and test

Open PowerShell from the x86 developer prompt, in the repository root.
Set the release version:

```powershell
$version = 'v0.1.0-rc1'
```

Run each command in this order. Stop if a command returns a nonzero exit code.
Keep the build and test output under `build/`.

```powershell
cmd /c tools\build_hook.bat
cmake -S . -B build/server -G 'NMake Makefiles' -DCMAKE_BUILD_TYPE=Release "-DSV_SDK_VERSION=$version"
cmake --build build/server --target sdk-package
ctest --test-dir build/server --output-on-failure
python -I tools/re/gen_storage.py --check
python -I tools/ci/check_native.py
python -I tools/ci/check_repo.py
git diff --check
```

The build must include the server, launcher, runner and probe DLL.
All CTest groups must pass, including `sdk-package`, `sdk-clean` and `plugin-scenarios`.
The two SDK checks cover different cases. `sdk-package` checks headers, CMake and repeatable bytes.
`sdk-clean` removes compiler state and runs outside the source tree with Windows tools only on `PATH`.

Test the exact release ZIP after the suite passes:

```powershell
$zip = "build/server/packages/SubVulpes-sdk-$version.zip"
powershell -NoProfile -ExecutionPolicy Bypass -File tests/sdk_clean_test.ps1 -BuildDir build/server -SdkZip $zip
```

Expect `SDK_CLEAN_PASS` and exit code 0.
This check builds the template, packs and installs it, approves its DLL, and runs the template scenario.
It uses the runner from the ZIP and a copy of the matching server.
It needs MSVC Build Tools, a Windows SDK and Windows PowerShell. It needs no game data or client.

## Inspect and record

Check the ZIP before any owner review:

- `VERSION.txt` equals the chosen version.
- `include/` holds only the public `sv_*.h` headers from this commit.
- `template/` holds the source, manifest, build files and `test.svtest`.
- `tools/` holds the scripts, `subvulpes_plugin_test.exe` and the probe DLL with its manifest.
- `docs/` holds `sdk-first-steps.md` and its linked API pages.
- No game files, server executable, private settings, logs or dumps are in the ZIP.

Record the commit, version, test results and SHA-256 in the local release notes.
Record the MSVC version and the version of the matching server too.
Write the checksum next to the ZIP:

```powershell
$hash = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()
"$hash  $([IO.Path]::GetFileName($zip))" | Set-Content -LiteralPath "$zip.sha256" -Encoding ascii
git status --short --branch
```

Check that the branch is clean. Keep the ZIP and checksum under `build/`.
The owner reviews these files before any later publish or tag request.
See [the first steps](sdk-first-steps.md) and [the package layout](sdk-packaging.md).
