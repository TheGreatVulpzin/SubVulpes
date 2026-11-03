SubVulpes plugin SDK
====================

Build a native plugin for the SubVulpes server. A plugin is a 32-bit DLL and a plugin.ini file.
Start with docs/sdk-first-steps.md. It shows the full path from this ZIP to a working hello plugin.

Folders
  include/    The public headers (sv_*.h). They are the whole API.
  template/   A small plugin. Keep it next to include/ and tools/.
  tools/      The pack, validate, install and test scripts. The runner and probe plugin are here too.
  docs/       Start with sdk-first-steps.md, plugin-sdk.md, plugin-manifest.md and plugin-abi.md.
  examples/   The source of the status plugin that comes with the server.

First plugin
  1. Install MSVC Build Tools with the C++ x86 tools and a Windows SDK.
     build.bat finds MSVC with vswhere. Set VCVARS to a vcvars32.bat path to select another installation.
  2. Run template\build.bat. It makes template\out\hello.dll and template\out\plugin.ini.
  3. Pack and test it from this SDK folder:
       powershell -NoProfile -ExecutionPolicy Bypass -File tools\pack.ps1 -Folder template\out -OutputDir dist -ServerExe SERVERFOLDER\subvulpes_server.exe
       powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_plugin_test.ps1 -Plugin dist\my.hello-0.1.0-native.svplugin -Scenario template\test.svtest -ServerExe SERVERFOLDER\subvulpes_server.exe
     Expect PASS test.svtest. Use a server that matches this SDK version.
  4. Open Plugins in the server launcher. Choose Install/Update and select the .svplugin file.
     Read the identity and capability risks. Check the native full-process box, then select Install.
     The launcher stops the server after consent. A new plugin stays disabled.
  5. Enable my.hello in Plugins. Start the server and type hello in its console.
     Expect a log line that starts with Hello from.
     After a DLL change, build, test, pack and review the updated package again.

Publish
  To share the plugin, read docs/sdk-workshop.md. template\build.ps1 is the build script that the Workshop uses.

Reviewed approval
  The launcher uses --plugin-inspect and the scoped store user/plugins/approvals.ini.
  The clean-machine check uses the same installer path without the GUI.
  Read docs/sdk-approval.md for the record, CLI path and one-run development mode.

Legacy native approval CLI
  These compatibility commands grant full native-content approval and replace the scoped review for that ID.
  Prefer the launcher review for capability tracking. Run from SERVERFOLDER while the server is stopped:
    .\subvulpes_server.exe --plugin-hash my.hello
    .\subvulpes_server.exe --plugin-approve my.hello SHA256
    .\subvulpes_server.exe --plugin-enable my.hello
    .\subvulpes_server.exe --plugins-check
  Replace SHA256 with the 64 hexadecimal characters on the SHA256 line from --plugin-hash.
  Approve the installed DLL hash, not the package hash. Each command must return 0.
  Start the test server and type hello in its console. Expect a log line that starts with Hello from.
  A new DLL needs approval of its new hash. The test script approves only its temporary test folder.

Rules
  - List each API table that the plugin opens in capabilities. An unlisted table gives SV_DENIED.
  - Native code has full access to the server process. Approve only plugins that you trust.
  - Read docs\plugin-abi.md before you add a field to a table. Rebuild your plugins when the SDK version changes.
