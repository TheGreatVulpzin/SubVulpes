using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;

namespace SubVulpes
{
    static class LauncherUpdateTests
    {
        static string serverBuild;
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
            if (!review.Contains("Runs inside the server with full access to it."))
                throw new Exception("Native approval lost its full process warning for a cached script row.");
            current.Runtime = "native";
            if (current.DisplayName != "Health Commands" || current.Id != "sv.health" || current.Hash != hash ||
                current.Version != "0.1.0" || current.State != "ready" || current.Required != "0" || current.Reason != "Ready.")
                throw new Exception("Plugin display changed identity, hash or column data.");
            // An action takes the id of the parsed row. The display name never becomes an identifier.
            current.Name = "other.plugin";
            if (LauncherForm.PluginActionId(current.Id) != "sv.health")
                throw new Exception("Plugin display text became an action identifier.");
            if (LauncherForm.PluginActionId("../escape") != null || LauncherForm.PluginActionId("") != null || LauncherForm.PluginActionId(null) != null)
                throw new Exception("The action accepted an invalid plugin identifier.");
            current.Name = "Health Commands";
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

        static string InspectionText(string capabilities, string version = "0.2.0")
        {
            string hash = new string('a', 64), manifest = new string('b', 64), zip = new string('c', 64);
            var lines = new List<string> {
                "MANIFEST\ttest.review\t" + version + "\treview.dll\tnative\t" + capabilities +
                    "\tReview & Example\tChanges player health.\tAn Author\thttps://example.test/review",
                "INSPECT\t1\ttest.review\t" + hash + "\t" + manifest
            };
            foreach (string capability in capabilities.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries)) {
                string level = capability == "read" || capability == "storage" || capability == "config" ? "low" : "high";
                lines.Add("DANGER\t" + capability + "\t" + level + "\tPermission: " + capability);
            }
            lines.Add("DANGER\tnative\thighest\tNative code gets full process access.");
            lines.Add("PACKAGE\t" + zip);
            return string.Join("\n", lines);
        }

        static void CheckPluginApproval()
        {
            string text = InspectionText("read,write,storage");
            var next = LauncherForm.ParsePluginInspection(text, true);
            if (next == null || next.DisplayName != "Review & Example" || next.Author != "An Author" ||
                next.ManifestHash != new string('b', 64) || next.Dangers.Count != 4)
                throw new Exception("The inspection report lost identity, display fields or risks.");
            string trust = "TRUST\t1\ttest.review\t0.1.0\tnative\tread,write,storage\t" +
                new string('d', 64) + "\t" + new string('e', 64) + "\t\t2026-10-30T00:00:00Z";
            var previous = LauncherForm.ParsePluginTrust(trust, "test.review");
            if (previous == null || previous.Version != "0.1.0" || LauncherForm.NeedsFullPluginReview(next, previous))
                throw new Exception("An update with unchanged permissions did not use one version review.");
            previous.Capabilities = "storage,read,write";
            if (LauncherForm.NeedsFullPluginReview(next, previous))
                throw new Exception("Capability order changed the review mode.");
            previous.Capabilities = "read,storage";
            if (!LauncherForm.NeedsFullPluginReview(next, previous))
                throw new Exception("A new capability skipped full review.");
            previous.Capabilities = next.Capabilities; previous.Runtime = "lua";
            if (!LauncherForm.NeedsFullPluginReview(next, previous))
                throw new Exception("A new runtime skipped full review.");
            previous.Runtime = next.Runtime;
            if (!LauncherForm.NeedsFullPluginReview(next, null))
                throw new Exception("A first approval skipped full review.");
            foreach (string invalid in new[] {
                text.Replace("DANGER\twrite\thigh\tPermission: write\n", ""),
                text.Replace("DANGER\tnative\thighest", "DANGER\tnative\tlow"),
                text + "\nPACKAGE\t" + new string('c', 64),
                text.Replace("INSPECT\t1\ttest.review", "INSPECT\t1\tother.id"),
                text.Replace("DANGER\tread\tlow\tPermission: read", "DANGER\tread\tlow\tPermission: read\nDANGER\tread\tlow\tDuplicate"),
                text.Replace(new string('b', 64), "bad"),
                text.Replace("DANGER\tnative\thighest\tNative code gets full process access.\n", "")
            }) if (LauncherForm.ParsePluginInspection(invalid, true) != null)
                throw new Exception("An incomplete or unsafe review was accepted.");
            // The risk colors come from the theme, so that they read on both themes. Each level has its own color.
            if (LauncherForm.PluginDangerColor("low").ToArgb() != SubVulpes.Ui.Theme.Light.Ok.ToArgb() ||
                LauncherForm.PluginDangerColor("high").ToArgb() != SubVulpes.Ui.Theme.Light.Warn.ToArgb() ||
                LauncherForm.PluginDangerColor("highest").ToArgb() != SubVulpes.Ui.Theme.Light.Bad.ToArgb())
                throw new Exception("Review risk colors changed.");

            using (var dialog = new LauncherForm.PluginApprovalDialog(next, previous, true)) {
                var risks = (System.Windows.Forms.RichTextBox)dialog.Controls.Find("capabilityReview", true)[0];
                foreach (var pair in new[] { new[] { "native", "highest" }, new[] { "write", "high" }, new[] { "read", "low" } }) {
                    int position = risks.Text.IndexOf(pair[0] + " (", StringComparison.Ordinal);
                    if (position < 0) throw new Exception("The dialog omitted a requested capability.");
                    risks.Select(position, pair[0].Length);
                    if (risks.SelectionColor.ToArgb() != LauncherForm.PluginDangerColor(pair[1]).ToArgb())
                        throw new Exception("The risk text lost its danger color: " + pair[0]);
                }
                if (dialog.FullReview || dialog.NativeConfirmation.Checked || dialog.ApproveButton.Enabled || dialog.TryApprove())
                    throw new Exception("Native content can be approved without its extra checkbox.");
                dialog.NativeConfirmation.Checked = true;
                if (!dialog.ApproveButton.Enabled || !dialog.TryApprove())
                    throw new Exception("Explicit native consent did not permit approval.");
            }
            using (var dialog = new LauncherForm.PluginApprovalDialog(next, null, true)) {
                if (!dialog.FullReview || dialog.ApproveButton.Text != "Install")
                    throw new Exception("The first review lost its full disclosure or action.");
            }
            bool refused = false;
            try { LauncherForm.PluginApprovalArguments(next, false); }
            catch (IOException) { refused = true; }
            if (!refused || !LauncherForm.PluginApprovalArguments(next, true).Contains(" -NativeConfirmed") ||
                !LauncherForm.PluginApprovalArguments(next, true).Contains(next.ManifestHash))
                throw new Exception("The installation command lost consent or content binding.");

            // Read the real server output as well as the synthetic parser cases.
            string build = serverBuild;
            var start = new ProcessStartInfo(Path.Combine(build, "subvulpes_server.exe"), "--plugin-inspect sv.status") {
                WorkingDirectory = build, UseShellExecute = false, CreateNoWindow = true,
                RedirectStandardOutput = true, RedirectStandardError = true
            };
            using (var process = Process.Start(start)) {
                string report = process.StandardOutput.ReadToEnd();
                string error = process.StandardError.ReadToEnd();
                process.WaitForExit();
                if (process.ExitCode != 0 || LauncherForm.ParsePluginInspection(report, false) == null)
                    throw new Exception("The launcher cannot parse the real server inspection: " + error);
            }
        }

        [STAThread]
        static int Main(string[] arguments)
        {
            serverBuild = arguments.Length == 1 ? Path.GetFullPath(arguments[0]) :
                Path.GetFullPath(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "..", "server"));
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
            CheckPluginApproval();
            CheckStore();
            CheckStoreVersions();
            CheckUninstall();
            CheckCommunityCard();
            CheckVersionNames();
            CheckLook();
            Console.WriteLine("Launcher update, backup, admin password, plugin metadata, Workshop and uninstall checks passed.");
        }

        static void Fail(string text) { throw new Exception(text); }

        static string Fixture(string name)
        {
            for (string dir = AppDomain.CurrentDomain.BaseDirectory; dir != null; dir = Path.GetDirectoryName(dir)) {
                string path = Path.Combine(dir, "tests", "launcher-fixtures", name);
                if (File.Exists(path)) return path;
            }
            throw new Exception("Fixture not found: " + name);
        }

        static void CheckStore()
        {
            // The signature: the fixture is signed with a test key. A change in the data, the signature or the key must fail.
            byte[] index = File.ReadAllBytes(Fixture("index.json")), seal = Convert.FromBase64String(File.ReadAllText(Fixture("index.json.sig.b64")).Trim());
            string testKey = File.ReadAllText(Fixture("test-public.pem")), realKey = LauncherForm.StoreKeyText();
            if (!LauncherForm.VerifyIndex(index, seal, testKey)) Fail("A good signature was refused.");
            if (LauncherForm.VerifyIndex(index, seal, realKey)) Fail("A signature of another key was accepted.");
            var changed = (byte[])index.Clone(); changed[changed.Length - 3] ^= 1;
            if (LauncherForm.VerifyIndex(changed, seal, testKey)) Fail("A changed index was accepted.");
            var badSeal = (byte[])seal.Clone(); badSeal[10] ^= 1;
            if (LauncherForm.VerifyIndex(index, badSeal, testKey)) Fail("A changed signature was accepted.");
            if (LauncherForm.VerifyIndex(index, new byte[10], testKey)) Fail("A short signature was accepted.");
            if (LauncherForm.VerifyIndex(index, seal, "not a key") || LauncherForm.VerifyIndex(index, seal, "")) Fail("A bad key was accepted.");
            // The key inside the launcher is a 3072-bit RSA key.
            if (LauncherForm.ReadPublicKey(realKey).Modulus.Length != 384) Fail("The key of the launcher has an unexpected size.");

            // The index: good entries pass, entries that break a rule are skipped.
            string hash = new string('a', 64);
            string good = "{\"id\":\"sv.one\",\"name\":\"One\",\"description\":\"d\",\"author\":\"A\",\"version\":\"0.1.0\",\"kind\":\"plugin\",\"source\":\"Official\"," +
                "\"channel\":\"beta\",\"icon\":\"icons/sv.one.png\",\"images\":[\"images/sv.one-1.png\",\"../x.png\"]," +
                "\"package\":{\"file\":\"packages/sv.one-0.1.0-native.svplugin\",\"sha256\":\"" + hash + "\",\"size\":1234}}";
            string text = "{\"format\":1,\"plugins\":[" + good + "," +
                good.Replace("sv.one", "sv.two").Replace("\"plugin\"", "\"mod\"") + "," +
                good.Replace("sv.one", "sv.three").Replace("\"Official\"", "\"Mine\"") + "," +
                good.Replace("sv.one", "sv.four").Replace("packages/", "../") + "," +
                good.Replace("sv.one", "sv.five").Replace(hash, "xyz") + "," +
                good.Replace("sv.one", "sv.six").Replace("1234", "99999999999") + "," +
                good.Replace("sv.one", "Bad Id") + "]}";
            var list = LauncherForm.ParseStoreIndex(text, "C:\\cache");
            if (list.Count != 1 || list[0].Id != "sv.one") Fail("The index kept " + list.Count + " entries.");
            LauncherForm.CatalogEntry one = list[0];
            if (!one.Remote || one.Size != 1234 || one.Sha256 != hash || one.Channel != "beta" || one.Source != "Official") Fail("An entry lost a field.");
            if (one.Icon != "C:\\cache\\icons\\sv.one.png" || one.Images.Count != 1 || one.ImageRemote[0] != "images/sv.one-1.png") Fail("The paths of the pictures are wrong.");
            foreach (string badIndex in new[] { "{\"format\":2,\"plugins\":[]}", "{\"plugins\":[]}", "[]", "{\"format\":1}" }) {
                bool refused = false;
                try { LauncherForm.ParseStoreIndex(badIndex, "C:\\cache"); } catch (Exception) { refused = true; }
                if (!refused) Fail("A bad index was accepted: " + badIndex);
            }

            // The package must match the size and the hash of the signed index. A bad file is deleted.
            string dir = Path.Combine(Path.GetTempPath(), "launcher-store-" + Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(dir);
            try {
                string file = Path.Combine(dir, "p.svplugin");
                File.WriteAllBytes(file, new byte[] { 1, 2, 3 });
                string right = "039058c6f2c0cb492c533b0a4d14ef77cc0f78abe8f9c9e5d6d8c6ef8fa2e7ff";
                using (var sha = System.Security.Cryptography.SHA256.Create())
                    right = BitConverter.ToString(sha.ComputeHash(new byte[] { 1, 2, 3 })).Replace("-", "").ToLowerInvariant();
                LauncherForm.VerifyStorePackage(file, 3, right);
                if (!File.Exists(file)) Fail("A good package was deleted.");
                foreach (var bad in new[] { new object[] { 3L, hash }, new object[] { 4L, right } }) {
                    bool refused = false;
                    try { LauncherForm.VerifyStorePackage(file, (long)bad[0], (string)bad[1]); } catch (Exception) { refused = true; }
                    if (!refused || File.Exists(file)) Fail("A package that does not match was kept.");
                    File.WriteAllBytes(file, new byte[] { 1, 2, 3 });
                }
            } finally { Directory.Delete(dir, true); }
        }

        // The list of versions: parsing, the rules that hide a version, the best version, the daily check and the downgrade rule.
        static void CheckStoreVersions()
        {
            string h = new string('a', 64), h2 = new string('b', 64), h3 = new string('c', 64);
            Func<string, string, string, string> item = (version, extra, hash) =>
                "{\"version\":\"" + version + "\",\"file\":\"packages/sv.one-" + version + "-native.svplugin\",\"sha256\":\"" + hash + "\",\"size\":100" + extra + "}";
            string head = "{\"id\":\"sv.one\",\"name\":\"One\",\"version\":\"0.3.0\",\"kind\":\"plugin\",\"source\":\"Official\",\"server_min\":\"0.3.0\"," +
                "\"package\":{\"file\":\"packages/sv.one-0.3.0-native.svplugin\",\"sha256\":\"" + h + "\",\"size\":300},\"versions\":[";
            string versions = item("0.3.0", ",\"server_min\":\"0.3.0\"", h2) + "," +   // the newest again: the top level wins
                item("0.2.0", "", h2) + "," + item("0.1.0", ",\"server_min\":\"0.1.5\"", h3) + "," +
                item("0.4.0", ",\"min_api\":2", h) + "," +          // needs a newer plugin API
                item("0.2.5", ",\"channel\":\"beta\"", h2) + "," +   // a beta
                item("bad", "", h) + "," +                           // not a version
                item("0.2.1", "", "xyz") + "," +                     // a bad hash
                item("0.2.2", ",\"min_api\":0", h) + "," +           // a bad min_api
                item("0.2.3", ",\"server_min\":\"x\"", h) + "]}";    // a bad server_min
            var list = LauncherForm.ParseStoreIndex("{\"format\":1,\"plugins\":[" + head + versions + "]}", "C:\\cache");
            if (list.Count != 1) Fail("The entry with versions was lost.");
            LauncherForm.CatalogEntry one = list[0];
            string order = string.Join(",", one.Versions.Select(v => v.Version).ToArray());
            if (order != "0.4.0,0.3.0,0.2.5,0.2.0,0.1.0") Fail("The versions are wrong or not newest first: " + order);
            LauncherForm.StoreVersion top = one.Versions.First(v => v.Version == "0.3.0");
            if (top.Size != 300 || top.Sha256 != h || top.ServerMin != "0.3.0") Fail("The top level did not win for the newest version.");
            if (one.Version != "0.3.0" || one.Size != 300) Fail("The old fields of the entry changed.");

            // Which versions a server can run. The suffix of a server version does not count. A development build accepts any.
            Func<string, bool, string> usable = (server, betas) =>
                string.Join(",", LauncherForm.UsableVersions(one, server, betas).Select(v => v.Version).ToArray());
            if (usable("0.2.0", false) != "0.2.0,0.1.0") Fail("Usable on 0.2.0: " + usable("0.2.0", false));
            if (usable("0.2.0", true) != "0.2.5,0.2.0,0.1.0") Fail("Usable on 0.2.0 with betas: " + usable("0.2.0", true));
            if (usable("0.3.0-rc.1", false) != "0.3.0,0.2.0,0.1.0") Fail("An rc must meet its own version: " + usable("0.3.0-rc.1", false));
            if (usable("v0.3.1", false) != "0.3.0,0.2.0,0.1.0") Fail("Usable on v0.3.1: " + usable("v0.3.1", false));
            if (usable("", false) != "0.3.0,0.2.0,0.1.0") Fail("A development build must accept any server_min: " + usable("", false));
            if (usable("0.1.0", false) != "0.2.0") Fail("Usable on 0.1.0: " + usable("0.1.0", false));
            if (LauncherForm.BestVersion(one, "0.2.0", false).Version != "0.2.0" || LauncherForm.BestVersion(one, "0.2.0", true).Version != "0.2.5")
                Fail("The best version is wrong.");
            string future = "{\"id\":\"sv.four\",\"name\":\"Four\",\"version\":\"0.5.0\",\"kind\":\"plugin\",\"source\":\"Official\",\"server_min\":\"0.5.0\"," +
                "\"package\":{\"file\":\"packages/sv.four-0.5.0-native.svplugin\",\"sha256\":\"" + h + "\",\"size\":50}}";
            LauncherForm.CatalogEntry later = LauncherForm.ParseStoreIndex("{\"format\":1,\"plugins\":[" + future + "]}", "C:\\cache")[0];
            if (LauncherForm.BestVersion(later, "0.2.0", false) != null) Fail("A server that cannot run any version got one.");
            if (!LauncherForm.ServerBelow("0.2.0", "0.3.0") || LauncherForm.ServerBelow("0.3.0", "0.3.0") || LauncherForm.ServerBelow("0.3.0-rc.1", "0.3.0") ||
                LauncherForm.ServerBelow("0.10.0", "0.9.0") || LauncherForm.ServerBelow("", "0.3.0") || LauncherForm.ServerBelow("garbage", "0.3.0") ||
                !LauncherForm.ServerBelow("0.2", "0.2.1"))
                Fail("The server version rule is wrong.");

            // An entry with no list has one version. An odd newest number still installs, as before.
            string plain = "{\"id\":\"sv.two\",\"name\":\"Two\",\"version\":\"0.1.0\",\"kind\":\"plugin\",\"source\":\"Official\"," +
                "\"package\":{\"file\":\"packages/sv.two-0.1.0-native.svplugin\",\"sha256\":\"" + h + "\",\"size\":50}}";
            var odd = LauncherForm.ParseStoreIndex("{\"format\":1,\"plugins\":[" + plain + "," + plain.Replace("sv.two", "sv.three").Replace("\"0.1.0\"", "\"0.1\"") + "]}", "C:\\cache");
            if (odd.Count != 2 || odd[0].Versions.Count != 1 || odd[0].Versions[0].PackageFile != odd[0].PackageFile) Fail("An entry without a list lost its version.");
            if (odd[1].Versions.Count != 1 || odd[1].Versions[0].Version != "0.1") Fail("An odd version number lost its entry.");

            // The check runs once in 24 hours. A bad or future time counts as due.
            DateTime now = new DateTime(2026, 11, 9, 12, 0, 0, DateTimeKind.Utc);
            Func<double, string> ago = hours => now.AddHours(-hours).ToString("o", CultureInfo.InvariantCulture);
            if (!LauncherForm.PluginCheckDue("", now) || !LauncherForm.PluginCheckDue(null, now) || !LauncherForm.PluginCheckDue("garbage", now)) Fail("A missing time must be due.");
            if (LauncherForm.PluginCheckDue(ago(23), now)) Fail("A check 23 hours ago was due.");
            if (!LauncherForm.PluginCheckDue(ago(24), now) || !LauncherForm.PluginCheckDue(ago(30), now)) Fail("A check 24 hours ago was not due.");
            if (!LauncherForm.PluginCheckDue(ago(-1), now)) Fail("A time in the future must be due.");

            // An older version is a downgrade and asks first.
            if (!LauncherForm.IsDowngrade("0.3.0", "0.2.0") || LauncherForm.IsDowngrade("0.2.0", "0.3.0") || LauncherForm.IsDowngrade("0.2.0", "0.2.0") ||
                LauncherForm.IsDowngrade("0.3.0", "bad"))
                Fail("The downgrade rule is wrong.");
        }

        // The short card of a community plugin: Install stays off until the user allows the plugin to run.
        static void CheckCommunityCard()
        {
            var metadata = new LauncherForm.PluginMetadata { Id = "ann.two", Version = "0.1.0", Runtime = "native", Name = "Two", Author = "Ann" };
            using (var card = new LauncherForm.CommunityCard(metadata)) {
                if (card.InstallButton.Enabled) Fail("The card allowed Install before the box was checked.");
                card.Allow.Checked = true;
                if (!card.InstallButton.Enabled) Fail("The card refused Install after the box was checked.");
                card.Allow.Checked = false;
                if (card.InstallButton.Enabled) Fail("The card kept Install after the box was cleared.");
            }
        }

        static void CheckVersionNames()
        {
            string[][] cases = { new[] { "v0.2.0", "Kit" }, new[] { "v0.0.0-s3-smoke", "Kit" }, new[] { "v1.0.0-rc.1", "Arctic" }, new[] { "v1.3.2", "Fennec" },
                new[] { "2.0.0", "Red" }, new[] { "v3.0.0", "" }, new[] { "nonsense", "" }, new[] { "", "" } };
            foreach (string[] c in cases)
                if (LauncherForm.VersionName(c[0]) != c[1]) Fail("The version name of " + c[0] + " is " + LauncherForm.VersionName(c[0]) + " and not " + c[1] + ".");
        }

        // ------------------------------------------------------------ the look

        static double Luminance(System.Drawing.Color c)
        {
            Func<int, double> channel = v => { double x = v / 255.0; return x <= 0.03928 ? x / 12.92 : Math.Pow((x + 0.055) / 1.055, 2.4); };
            return 0.2126 * channel(c.R) + 0.7152 * channel(c.G) + 0.0722 * channel(c.B);
        }

        static double Contrast(System.Drawing.Color a, System.Drawing.Color b)
        {
            double la = Luminance(a), lb = Luminance(b);
            return (Math.Max(la, lb) + 0.05) / (Math.Min(la, lb) + 0.05);
        }

        static void Press(System.Windows.Forms.Control control, System.Windows.Forms.Keys key)
        {
            var method = control.GetType().GetMethod("OnKeyDown", System.Reflection.BindingFlags.Instance | System.Reflection.BindingFlags.NonPublic);
            method.Invoke(control, new object[] { new System.Windows.Forms.KeyEventArgs(key) });
        }

        // Every text color passes a contrast of 4.5 on the surfaces that it sits on, in both themes.
        // The custom controls answer the keyboard and have names for a screen reader.
        static void CheckLook()
        {
            foreach (var theme in new[] { SubVulpes.Ui.Theme.Light, SubVulpes.Ui.Theme.Dark }) {
                string name = theme == SubVulpes.Ui.Theme.Light ? "light" : "dark";
                var surfaces = new[] { theme.Win, theme.Panel, theme.Chip };
                var texts = new[] { theme.Fg, theme.Muted, theme.AccentFg, theme.Ok, theme.Warn, theme.Bad };
                foreach (var text in texts)
                    foreach (var surface in surfaces)
                        if (Contrast(text, surface) < 4.5) Fail("A text color in the " + name + " theme has a contrast under 4.5.");
                if (Contrast(theme.BarMuted, theme.Bar) < 4.5 || Contrast(theme.Fg, theme.Bar) < 4.5 || Contrast(theme.Muted, theme.SubBar) < 4.5)
                    Fail("The tab bars of the " + name + " theme have a contrast under 4.5.");
                if (Contrast(System.Drawing.Color.White, theme.OkFill) < 4.5) Fail("The text on the Start button of the " + name + " theme has a contrast under 4.5.");
            }

            // A new accent color keeps a readable text color.
            foreach (var accent in new[] { System.Drawing.Color.FromArgb(37, 99, 235), System.Drawing.Color.FromArgb(22, 163, 74), System.Drawing.Color.FromArgb(219, 39, 119), System.Drawing.Color.FromArgb(217, 99, 30) }) {
                SubVulpes.Ui.Theme.Light.SetAccent(accent);
                if (SubVulpes.Ui.Theme.Contrast(SubVulpes.Ui.Theme.Light.AccentFg, SubVulpes.Ui.Theme.Light.Chip) < 4.5) Fail("The accent text on the light theme is too faint.");
            }
            SubVulpes.Ui.Theme.Light.SetAccent(System.Drawing.Color.FromArgb(217, 99, 30));

            int clicks = 0;
            var button = new SubVulpes.Ui.FlatButton("Start");
            button.Click += delegate { clicks++; };
            Press(button, System.Windows.Forms.Keys.Space);
            Press(button, System.Windows.Forms.Keys.Return);
            Press(button, System.Windows.Forms.Keys.A);
            if (clicks != 2) Fail("A button must answer Space and Enter, and nothing else.");
            button.Enabled = false;
            Press(button, System.Windows.Forms.Keys.Space);
            if (clicks != 2) Fail("A disabled button answered a key.");
            if (button.AccessibleName != "Start" || button.AccessibleRole != System.Windows.Forms.AccessibleRole.PushButton) Fail("A button has no accessible name.");
            button.Text = "Stop";
            if (button.AccessibleName != "Stop") Fail("The accessible name does not follow the text.");

            var toggle = new SubVulpes.Ui.Switch();
            Press(toggle, System.Windows.Forms.Keys.Space);
            if (!toggle.On || toggle.AccessibleRole != System.Windows.Forms.AccessibleRole.CheckButton) Fail("A switch must toggle with Space and read as a check box.");

            var list = new SubVulpes.Ui.Dropdown(120);
            list.Items.Add("One"); list.Items.Add("Two"); list.Items.Add("Three");
            list.SelectedIndex = 0;
            Press(list, System.Windows.Forms.Keys.Down);
            Press(list, System.Windows.Forms.Keys.Down);
            Press(list, System.Windows.Forms.Keys.Down);
            if (list.SelectedIndex != 2) Fail("Down must stop at the last item.");
            Press(list, System.Windows.Forms.Keys.Up);
            if (list.SelectedIndex != 1 || list.AccessibleRole != System.Windows.Forms.AccessibleRole.ComboBox) Fail("A drop-down must change with Up and Down.");

            var choice = new SubVulpes.Ui.Segmented("A", "B", "C");
            choice.SelectedIndex = 0;
            Press(choice, System.Windows.Forms.Keys.Right);
            Press(choice, System.Windows.Forms.Keys.Right);
            Press(choice, System.Windows.Forms.Keys.Right);
            if (choice.SelectedIndex != 2) Fail("Right must stop at the last option.");

            int picks = 0;
            var tab = new SubVulpes.Ui.TabLabel("Plugins");
            tab.Click += delegate { picks++; };
            Press(tab, System.Windows.Forms.Keys.Return);
            if (picks != 1 || tab.AccessibleName != "Plugins" || tab.AccessibleRole != System.Windows.Forms.AccessibleRole.PageTab) Fail("A tab must answer Enter and have a name.");

            var icon = new SubVulpes.Ui.IconButton("copy") { Caption = "Copy all." };
            if (icon.AccessibleName != "Copy all.") Fail("An icon button has no accessible name.");
        }

        static void CheckUninstall()
        {
            string root = Path.Combine(Path.GetTempPath(), "launcher-remove-" + Guid.NewGuid().ToString("N"));
            try {
                foreach (string sub in new[] { "plugins\\a.lib", "plugins\\b.user", "plugins\\c.free", "user\\plugins\\b.user" })
                    Directory.CreateDirectory(Path.Combine(root, sub));
                File.WriteAllText(Path.Combine(root, "plugins\\a.lib\\plugin.ini"), "[plugin]\r\nid=a.lib\r\n");
                File.WriteAllText(Path.Combine(root, "plugins\\b.user\\plugin.ini"), "[plugin]\r\nid=b.user\r\ndependencies=x.other@1.0,a.lib@1.2\r\n");
                File.WriteAllText(Path.Combine(root, "plugins\\c.free\\plugin.ini"), "[plugin]\r\nid=c.free\r\n");
                File.WriteAllText(Path.Combine(root, "user\\plugins\\b.user\\data.txt"), "saved");
                File.WriteAllText(Path.Combine(root, "plugins\\plugins.ini"), "[plugins]\r\na.lib=enabled\r\nb.user=enabled\r\nc.free=disabled\r\n");
                File.WriteAllText(Path.Combine(root, "user\\plugins\\approvals.ini"), "[approvals]\r\nb.user=1|x\r\nc.free=1|y\r\n");
                File.WriteAllText(Path.Combine(root, "user\\plugins\\native-approvals.ini"), "[native]\r\nb.user=aa\r\nc.free=bb\r\n");
                if (LauncherForm.PluginNeededBy(root, "a.lib") != "b.user") Fail("The needed-by check missed a plugin.");
                if (LauncherForm.PluginNeededBy(root, "c.free") != "") Fail("The needed-by check found a plugin that does not exist.");
                LauncherForm.UninstallPluginFiles(root, "b.user");
                if (Directory.Exists(Path.Combine(root, "plugins\\b.user"))) Fail("The plugin folder stayed.");
                if (!File.Exists(Path.Combine(root, "user\\plugins\\b.user\\data.txt"))) Fail("The saved data of the plugin was deleted.");
                string enabled = File.ReadAllText(Path.Combine(root, "plugins\\plugins.ini")), approvals = File.ReadAllText(Path.Combine(root, "user\\plugins\\approvals.ini"));
                if (enabled.Contains("b.user") || !enabled.Contains("a.lib=enabled") || !enabled.Contains("c.free=disabled")) Fail("plugins.ini is wrong after the removal.");
                if (approvals.Contains("b.user") || !approvals.Contains("c.free=1|y")) Fail("approvals.ini is wrong after the removal.");
                if (File.ReadAllText(Path.Combine(root, "user\\plugins\\native-approvals.ini")).Contains("b.user")) Fail("native-approvals.ini kept the plugin.");
                if (LauncherForm.PluginNeededBy(root, "a.lib") != "") Fail("The needed-by check still sees the removed plugin.");
                foreach (string bad in new[] { "..\\x", "../x", "a.lib\\..", "", "A.Upper", "con" }) {
                    bool refused = false;
                    try { LauncherForm.UninstallPluginFiles(root, bad); } catch (Exception) { refused = true; }
                    if (!refused) Fail("A bad plugin id was accepted: " + bad);
                }
                if (!Directory.Exists(Path.Combine(root, "plugins\\a.lib"))) Fail("A bad id removed another plugin.");
            } finally { if (Directory.Exists(root)) Directory.Delete(root, true); }
        }
    }
}
