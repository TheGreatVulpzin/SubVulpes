# First plugin from the SDK

Use a Windows test server and MSVC Build Tools with the C++ x86 tools and a Windows SDK.
No game client or game data is needed for the scenario test.
Use a server that matches the SDK version. Rebuild old DLLs after the ABI reset.

1. Extract `SubVulpes-sdk-VERSION.zip` into an empty folder. Keep its folders together.
   Open Windows PowerShell in the folder that holds `README.txt`.
   Use `powershell -NoProfile -ExecutionPolicy Bypass` to run the unsigned SDK scripts in this session.
   Set the path to a stopped test server:

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

4. Install the package while the test server is stopped:

   ```powershell
   .\tools\install_plugin.ps1 -ServerDir $server -Package dist\my.hello-0.1.0-native.svplugin
   ```

   The installer leaves a new plugin disabled. Approve only native code that you trust.
   A native DLL can do anything that the server process can do.

5. Approve and enable the installed DLL from the test server folder:

   ```powershell
   Push-Location $server
   .\subvulpes_server.exe --plugin-hash my.hello
   # Replace SHA256 with the 64 hexadecimal characters on the SHA256 line.
   .\subvulpes_server.exe --plugin-approve my.hello SHA256
   .\subvulpes_server.exe --plugin-enable my.hello
   .\subvulpes_server.exe --plugins-check
   Pop-Location
   ```

   Each command must return 0. Start the test server and type `hello` in its console.
   Expect a log line that starts with `Hello from`.
   After each DLL change, build, test, install and approve the new hash.

Edit `template\hello.c` for your code. Keep the IDs in `hello.c` and `plugin.ini` equal.
Read [the API guide](plugin-sdk.md), [the manifest rules](plugin-manifest.md) and [the scenario guide](sdk-test-runner.md).
