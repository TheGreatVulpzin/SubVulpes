using System;
using System.Diagnostics;
using System.IO;

namespace SubVulpes
{
    static class LauncherUpdateTests
    {
        static void Expect(string path, string expected)
        {
            string result = LauncherForm.UpdateKey(path);
            if (result != expected)
                throw new Exception("Unexpected update path result: " + path);
        }

        static void CheckBackupDirectory()
        {
            string testBase = Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "tests"));
            string root = Path.Combine(testBase, "launcher-update-" + Guid.NewGuid().ToString("N"));
            string target = Path.Combine(root, "target");
            string junction = Path.Combine(root, "backup");
            Directory.CreateDirectory(target);
            try {
                string nested = Path.Combine(root, "plain", "nested");
                LauncherForm.CreateUpdateDirectory(nested);
                if (!Directory.Exists(nested)) throw new Exception("The plain backup directory was not created.");
                var start = new ProcessStartInfo("cmd.exe", "/c mklink /J \"" + junction + "\" \"" + target + "\"") {
                    UseShellExecute = false, CreateNoWindow = true,
                    RedirectStandardOutput = true, RedirectStandardError = true
                };
                using (var process = Process.Start(start)) {
                    string output = process.StandardOutput.ReadToEnd();
                    string error = process.StandardError.ReadToEnd();
                    process.WaitForExit();
                    if (process.ExitCode != 0) throw new Exception("Cannot create the backup junction: " + output + error);
                }
                bool rejected = false;
                try { LauncherForm.CreateUpdateDirectory(Path.Combine(junction, "timestamp")); }
                catch (IOException) { rejected = true; }
                if (!rejected) throw new Exception("A backup junction was accepted.");
                if (Directory.Exists(Path.Combine(target, "timestamp")))
                    throw new Exception("The rejected backup path changed its junction target.");
            } finally {
                string resolved = Path.GetFullPath(root);
                if (!resolved.StartsWith(testBase.TrimEnd('\\') + "\\", StringComparison.OrdinalIgnoreCase) ||
                    !Path.GetFileName(resolved).StartsWith("launcher-update-", StringComparison.Ordinal))
                    throw new IOException("The cleanup path leaves the test directory.");
                if (Directory.Exists(junction)) Directory.Delete(junction);
                Directory.Delete(root, true);
            }
        }

        static void Main()
        {
            Expect("subvulpes_server.exe", "subvulpes_server.exe");
            Expect("package/subvulpes_launcher.exe", "subvulpes_launcher.exe");
            Expect("package/plugins/sv.status/status.dll", "plugins/sv.status/status.dll");
            Expect("plugins/native-approvals.ini", "plugins/native-approvals.ini");
            Expect("package/plugins/sv.status/plugin.ini", "plugins/sv.status/plugin.ini");
            Expect("PACKAGE/PLUGINS/PLUGINS.INI", "plugins/plugins.ini");
            Expect("../subvulpes_server.exe", null);
            Expect("package/../subvulpes_server.exe", null);
            Expect("/subvulpes_server.exe", null);
            Expect("package//subvulpes_server.exe", null);
            Expect("package\\subvulpes_server.exe", null);
            Expect("C:/subvulpes_server.exe", null);
            Expect("a/b/subvulpes_server.exe", null);
            Expect("package/config.txt", null);
            Expect("package/user/server.srk", null);
            Expect("package/user/plugins/state.ini", null);
            Expect("package/user/plugins/native-approvals.ini", null);
            Expect("package/plugins/third.party/payload.dll", null);
            Expect("package/data/city.sbc", null);
            Expect("package/subvulpes_server.exe\n", null);
            Expect(null, null);
            if (LauncherForm.QuoteProcessArgument("C:\\Server Folder\\") != "\"C:\\Server Folder\\\\\"")
                throw new Exception("A trailing directory separator changed argument parsing.");
            if (LauncherForm.QuoteProcessArgument("a\"b") != "\"a\\\"b\"")
                throw new Exception("A quoted argument changed argument parsing.");
            CheckBackupDirectory();
            Console.WriteLine("21 launcher update path checks and backup directory checks passed.");
        }
    }
}
