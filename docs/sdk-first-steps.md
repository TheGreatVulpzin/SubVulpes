# First plugin from the SDK

Use a Windows test server and MSVC Build Tools with the C++ x86 tools and a Windows SDK.
No game client or game data is needed for the scenario test.
Use a server that matches the SDK version. Rebuild old DLLs after the ABI reset.

1. Extract `SubVulpes-sdk-VERSION.zip` into an empty folder. Keep its folders together.
   Open Windows PowerShell in the folder that holds `README.txt`.
   Use `powershell -NoProfile -ExecutionPolicy Bypass` to run the unsigned SDK scripts in this session.
   Set the path to a test server:

   ```powershell
   $server = 'C:\SubVulpes-test'
   ```

2. Build the template:

   ```powershell
   .\template\build.bat
   if ($LASTEXITCODE -ne 0) { throw 'The build failed.' }
   ```

   The script finds MSVC with `vswhere`. To select another installation, set `$env:VCVARS` to its `vcvars32.bat` path.
   The output folder `template\out` holds `hello.dll` and `plugin.ini`.

3. Pack the plugin and run its scenario:

   ```powershell
   .\tools\pack.ps1 -Folder template\out -OutputDir dist -ServerExe "$server\subvulpes_server.exe"
   .\tools\run_plugin_test.ps1 -Plugin dist\my.hello-0.1.0-native.svplugin -Scenario template\test.svtest -ServerExe "$server\subvulpes_server.exe"
   if ($LASTEXITCODE -ne 0) { throw 'The scenario failed.' }
   ```

   Expect `PASS test.svtest`. The SDK supplies the runner and probe plugin.
   The scenario needs no client and changes no approval on your server.

4. Open the launcher in the test server folder. Select **Plugins**, then **Install / update plugin package**.
   Choose `dist\my.hello-0.1.0-native.svplugin`.
   Read the name, author, description and capability risks.
   For native code, check the box for full process access.
   Select **Install and approve**.
   The launcher stops the server after consent. A new plugin stays disabled.
   An update shows the version change. New capabilities require the full review.

5. Enable `my.hello` in the plugin list. Start the test server and type `hello` in its console.
   Expect a log line that starts with `Hello from`.
   To check activation from the CLI while the server is stopped, use:

   ```powershell
   Push-Location $server
   .\subvulpes_server.exe --plugin-enable my.hello
   .\subvulpes_server.exe --plugins-check
   Pop-Location
   ```

   Each command must return 0. After a DLL change, build, test, pack and review the updated package again.

Native code has full access to the server process. Approve only code that you trust.
The [approval guide](sdk-approval.md) describes the shared CLI path, legacy approvals and one-run development mode.
The clean-machine test uses the same reviewed install path without the GUI.

Edit `template\hello.c` for your code. Keep its ID and version equal to those in `plugin.ini`.
Read [the API guide](plugin-sdk.md), [the manifest rules](plugin-manifest.md) and [the scenario guide](sdk-test-runner.md).
