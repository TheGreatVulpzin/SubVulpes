# Plugin packaging

This page covers the package of a native plugin: a `.svplugin` file.

Start with [the first steps](sdk-first-steps.md) to build, test, install and approve the template plugin.

## Build a plugin with CMake

The function `sv_add_plugin` is in `tools/sdk/sv_plugin.cmake`.

```cmake
include(tools/sdk/sv_plugin.cmake)
sv_add_plugin(my-plugin ID my.plugin MANIFEST "${CMAKE_SOURCE_DIR}/plugins/my/plugin.ini"
    SOURCES plugins/my/my.c)
```

It makes a 32-bit DLL with the flags of the server (`/W4 /WX /utf-8`). It exports `sv_plugin_entry`.
It puts the DLL and `plugin.ini` in `build/server/plugins/ID`. The version and the library name come from the manifest, so no value sits in two places.
It adds the target `my-plugin-package`. That target makes `build/server/packages/ID-VERSION-native.svplugin`.
The optional word `OFFICIAL` supplies plugin identity macros. The release pin controls package inclusion.

## Pack a folder

```text
tools\sdk\pack.ps1 -Folder FOLDER -OutputDir DIR -ServerExe subvulpes_server.exe
```

1. The script asks the server to check the manifest. A problem stops the script, and it prints every problem.
2. The folder must hold `plugin.ini` and the library, and nothing else. Another file stops the script, so a note or a private file cannot enter a package.
3. The script writes `ID-VERSION-RUNTIME.svplugin` (decision D4). The file is a ZIP.
4. The ZIP has a fixed order of entries and a fixed time stamp. The same folder gives the same bytes, so the hash of a package is stable.

The script prints one line: `PACKAGE`, a tab, the path, a tab and the SHA-256.

## Install a package

The installer and the launcher accept `.svplugin` and the old `.zip` name. Both are the same ZIP. The installer asks the server about the manifest before it copies any file.

## The official plugins

The file `tools/release/embedded-plugins.txt` pins Workshop source and lists the embedded plugins and their default states.
`tools/release/build_embedded.ps1` builds them with the SDK of the current server into `build/embedded`.
It writes the profile and calls `tools/plugins/write_official_approvals.ps1` to hash the DLLs.
`make_package.ps1` checks the source pin, profile and DLL hashes before it makes the server zip.
`plugins/status` stays here as the core fixture and SDK example. The server package does not carry it.

## The SDK package

The target `sdk-package` runs `tools/sdk/make_sdk.ps1`. It makes `SubVulpes-sdk-VERSION.zip` in `build/server/packages`. The default version is `v0.0.0-dev`. Set `SV_SDK_VERSION` for another one.

| Folder | Content |
| --- | --- |
| `include/` | The 16 public headers `sv_*.h`. No other header. |
| `template/` | A small plugin: `hello.c`, `plugin.ini`, `build.bat`, `CMakeLists.txt` and `test.svtest`. |
| `tools/` | `pack.ps1`, `validate_plugin.ps1`, `install_plugin.ps1`, `sv_plugin.cmake`, the test script, the runner and the probe plugin. |
| `docs/` | The pages of the API, starting with the first steps, the SDK page, the manifest page and the ABI rules. |
| `examples/status/` | The source of the status plugin. |

The package has a fixed order and a fixed time stamp, so the same sources give the same hash.
The script stops when the count of public headers is not 16. A new public header must be a decision.
The SDK does not carry the server program. The scripts ask for `-ServerExe`, because the server holds the manifest rules and the author knows which server runs the plugin.
