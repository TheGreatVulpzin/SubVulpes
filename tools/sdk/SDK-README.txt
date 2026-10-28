SubVulpes plugin SDK
====================

This package lets you build a native plugin for the SubVulpes server. A plugin is a 32-bit DLL and a plugin.ini file.

Folders
  include/    The public headers (sv_*.h). They are the whole API.
  template/   A small plugin. Copy this folder and change it.
  tools/      pack.ps1 makes the .svplugin file. validate_plugin.ps1 checks a plugin. install_plugin.ps1 installs one.
  docs/       The pages of the API. Start with plugin-sdk.md, plugin-manifest.md and plugin-abi.md.
  examples/   The source of the status plugin that comes with the server.

First plugin, in four steps
  1. Install MSVC (Visual Studio Build Tools) with the x86 tools.
  2. Run template\build.bat. It makes template\out\hello.dll and template\out\plugin.ini.
  3. Pack it:
       powershell -File tools\pack.ps1 -Folder template\out -OutputDir dist -ServerExe PATH\subvulpes_server.exe
     The server program holds the rules of the manifest, so the script asks it. Use the server you run.
  4. Install it on a test server:
       powershell -File tools\install_plugin.ps1 -ServerDir SERVERFOLDER -Package dist\my.hello-0.1.0-native.svplugin
     Then approve the DLL (the server lists the command), start the server and type "hello" in the server console.

Rules that matter
  - The manifest must list the tables that the plugin opens in "capabilities". A table that is not listed gives SV_DENIED.
  - A native plugin runs inside the server process. It can do anything the server can do. Install only plugins you trust.
  - The ABI (docs\plugin-abi.md) says what never changes. Read it before you add a field to a table.
