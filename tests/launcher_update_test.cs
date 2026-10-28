using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text.RegularExpressions;

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

        static void CheckAdminPassword()
        {
            var seen = new HashSet<string>();
            for (int i = 0; i < 50; i++) {
                string candidate = LauncherForm.NewAdminPassword();
                if (!Regex.IsMatch(candidate, "^[A-HJ-NP-Za-km-z2-9]{16}$") || !seen.Add(candidate))
                    throw new Exception("A generated admin password left the safe set or repeated.");
            }
            if (!LauncherForm.MissingAdminPassword(null) || !LauncherForm.MissingAdminPassword("") ||
                !LauncherForm.MissingAdminPassword("   ") || LauncherForm.MissingAdminPassword("abc"))
                throw new Exception("A blank admin password was not counted as missing.");
        }

        static void CheckPluginMetadata()
        {
            string hash = new string('a', 64), packageHash = new string('b', 64);
            string legacy = "PLUGIN\tsv.health\t0.1.0\tready\t0\t" + hash + "\tReady.";
            string module = "MODULE\tsv.health\t0.1.0\t" + hash + "\t" + packageHash;
            string[] display = { "native", "read,write,props,storage", "Health Commands", "Changes player health.", "An Author", "https://example.test/health" };
            string suffix = "\t" + string.Join("\t", display);
            var old = LauncherForm.ParsePluginRow(legacy, false);
            var current = LauncherForm.ParsePluginRow(legacy + suffix, false);
            var package = LauncherForm.ParsePluginRow(module + suffix, true);
            if (old == null || old.DisplayName != "sv.health" || old.Runtime != "native" || old.Capabilities != "" ||
                current == null || current.DisplayName != "Health Commands" || current.Id != "sv.health" ||
                package == null || package.PackageHash != packageHash || package.Author != "An Author" ||
                LauncherForm.ParsePluginRow(module, true) == null)
                throw new Exception("Plugin metadata or legacy rows did not parse.");
            current.Runtime = "lua";
            string review = LauncherForm.PluginReviewDetails(current, "Approve this content?", true);
            if (!review.Contains("Native code gets full process access.") || !review.Contains("crash the server."))
                throw new Exception("Native approval lost its full process warning for a cached script row.");
            current.Runtime = "native";
            var item = LauncherForm.MakePluginItem(current);
            if (item.Text != "Health Commands" || item.Name != "sv.health" || (string)item.Tag != hash ||
                item.SubItems[1].Text != "sv.health" || item.SubItems[2].Text != "0.1.0" ||
                item.SubItems[3].Text != "ready" || item.SubItems[4].Text != "No" || item.SubItems[5].Text != "Ready.")
                throw new Exception("Plugin display changed identity, hash or column data.");
            item.Text = "other.plugin";
            if (LauncherForm.PluginActionId(item) != "sv.health")
                throw new Exception("Plugin display text became an action identifier.");
            item.Name = "../escape";
            if (LauncherForm.PluginActionId(item) != null)
                throw new Exception("The action accepted an invalid plugin identifier.");
            display[2] = "";
            if (LauncherForm.ParsePluginRow(legacy + "\t" + string.Join("\t", display), false).DisplayName != "sv.health")
                throw new Exception("An empty display name did not use the identifier.");
            foreach (string runtime in new[] { "native", "lua", "wasm" }) {
                display[0] = runtime;
                if (LauncherForm.ParsePluginRow(module + "\t" + string.Join("\t", display), true) == null)
                    throw new Exception("The parser rejected a supported runtime.");
            }
            display[0] = "native";
            int[] limits = { 48, 160, 48, 96 };
            for (int i = 0; i < limits.Length; i++) {
                int field = i + 2;
                string saved = display[field];
                display[field] = field == 5 ? "https://example.test/" + new string('x', limits[i] - 21) : new string('x', limits[i]);
                if (LauncherForm.ParsePluginRow(legacy + "\t" + string.Join("\t", display), false) == null)
                    throw new Exception("The parser rejected a display field at its size limit.");
                display[field] += "x";
                if (LauncherForm.ParsePluginRow(legacy + "\t" + string.Join("\t", display), false) != null ||
                    LauncherForm.ParsePluginRow(module + "\t" + string.Join("\t", display), true) != null)
                    throw new Exception("The parser accepted an oversized display field.");
                foreach (string bad in new[] { "bad\ntext", "bad\ttext", "bad\x7f", "bad\u00e9" }) {
                    display[field] = bad;
                    if (LauncherForm.ParsePluginRow(legacy + "\t" + string.Join("\t", display), false) != null)
                        throw new Exception("The parser accepted a display field outside printable ASCII.");
                }
                display[field] = saved;
            }
            foreach (string bad in new[] { "python", "Native", "" }) {
                display[0] = bad;
                if (LauncherForm.ParsePluginRow(module + "\t" + string.Join("\t", display), true) != null)
                    throw new Exception("The parser accepted an unknown runtime.");
            }
            display[0] = "native";
            foreach (string bad in new[] { "read,read", "Read", "read,,write", "read, write", new string('x', 256) }) {
                display[1] = bad;
                if (LauncherForm.ParsePluginRow(legacy + "\t" + string.Join("\t", display), false) != null)
                    throw new Exception("The parser accepted an invalid capability list.");
            }
            display[1] = "";
            display[5] = "ftp://example.test";
            if (LauncherForm.ParsePluginRow(legacy + "\t" + string.Join("\t", display), false) != null ||
                LauncherForm.ParsePluginRow(legacy + suffix + "\textra", false) != null ||
                LauncherForm.ParsePluginRow(module + "\tnative", true) != null ||
                LauncherForm.ParsePluginRow(module.Replace(packageHash, "invalid"), true) != null ||
                LauncherForm.ParsePluginRow(module.Replace(hash, "invalid"), true) != null ||
                LauncherForm.ParsePluginRow(legacy.Replace(hash, "invalid"), false) != null ||
                LauncherForm.ParsePluginRow(legacy.Replace("sv.health", "con.health"), false) != null)
                throw new Exception("The parser accepted invalid plugin metadata.");
        }

        static int Main()
        {
            try { Run(); return 0; }
            catch (Exception error) { Console.Error.WriteLine(error.Message); return 1; }
        }

        static void Run()
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
            CheckAdminPassword();
            CheckPluginMetadata();
            Console.WriteLine("Launcher update, backup, admin password and plugin metadata checks passed.");
        }
    }
}
