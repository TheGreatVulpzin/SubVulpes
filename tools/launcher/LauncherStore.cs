// The Workshop client: the signed index, the cache, the downloads and the installs that wait for the next start.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using SubVulpes.Ui;

namespace SubVulpes
{
    partial class LauncherForm
    {
        const string StoreUrl = "https://raw.githubusercontent.com/TheGreatVulpzin/SubVulpes-Workshop/dist/";
        const long StoreIndexLimit = 2 * 1024 * 1024, StorePackageLimit = 70L * 1024 * 1024, StoreIconLimit = 131072, StoreImageLimit = 2 * 1024 * 1024;
        static readonly Regex StorePath = new Regex(@"\A(icons|thumbs|images|packages)/[A-Za-z0-9][A-Za-z0-9._+-]{0,119}\z");
        static readonly Regex Sha256Text = new Regex(@"\A[0-9a-f]{64}\z");
        static readonly Regex PluginVersionText = new Regex(@"\A[0-9]+\.[0-9]+\.[0-9]+(-[0-9A-Za-z.-]{1,16})?\z");
        static readonly Regex ServerMinText = new Regex(@"\A[0-9]+(\.[0-9]+){0,7}\z");
        // The plugin API that this launcher and its server know. A version that needs a higher one is hidden.
        const int SupportedPluginApi = 1;
        const int StoreVersionsMax = 24;

        readonly List<CatalogEntry> storeEntries = new List<CatalogEntry>();
        readonly List<string[]> pendingInstalls = new List<string[]>();   // path, SHA-256, source, id
        readonly HashSet<string> storeImagesAsked = new HashSet<string>();
        bool storeBusy, storeAsked;
        string storeProblem = "";

        // The cache sits next to the launcher, like launcher.ini.
        static string StoreCacheFolder()
        {
            return Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "workshop-cache");
        }

        static string StoreFilePath(string cache, string relative)
        {
            return Path.Combine(cache, relative.Replace('/', '\\'));
        }

        // ------------------------------------------------------------ the key and the signature

        internal static string StoreKeyText()
        {
            using (var stream = Assembly.GetExecutingAssembly().GetManifestResourceStream("SubVulpes.IndexKey"))
            using (var reader = new StreamReader(stream, Encoding.ASCII))
                return reader.ReadToEnd();
        }

        // One item of a DER structure. It moves the position past the item and gives its content.
        static byte[] DerItem(byte[] data, ref int position, byte tag)
        {
            if (position + 2 > data.Length || data[position] != tag) throw new InvalidDataException("The key is not valid.");
            int length = data[position + 1];
            position += 2;
            if (length >= 0x80) {
                int count = length & 0x7f;
                if (count < 1 || count > 3 || position + count > data.Length) throw new InvalidDataException("The key is not valid.");
                length = 0;
                for (int i = 0; i < count; i++) length = length * 256 + data[position++];
            }
            if (length < 0 || position + length > data.Length) throw new InvalidDataException("The key is not valid.");
            var item = new byte[length];
            Array.Copy(data, position, item, 0, length);
            position += length;
            return item;
        }

        static byte[] TrimZero(byte[] value)
        {
            int skip = 0;
            while (skip < value.Length - 1 && value[skip] == 0) skip++;
            if (skip == 0) return value;
            var result = new byte[value.Length - skip];
            Array.Copy(value, skip, result, 0, result.Length);
            return result;
        }

        // An RSA public key in the PEM form that openssl writes (SubjectPublicKeyInfo).
        internal static RSAParameters ReadPublicKey(string pem)
        {
            byte[] der = Convert.FromBase64String(Regex.Replace(pem, @"-----[A-Z ]+-----|\s", ""));
            int p = 0;
            byte[] info = DerItem(der, ref p, 0x30);
            p = 0;
            DerItem(info, ref p, 0x30);
            byte[] bits = DerItem(info, ref p, 0x03);
            if (bits.Length < 2 || bits[0] != 0) throw new InvalidDataException("The key is not valid.");
            var inner = new byte[bits.Length - 1];
            Array.Copy(bits, 1, inner, 0, inner.Length);
            p = 0;
            byte[] pair = DerItem(inner, ref p, 0x30);
            p = 0;
            byte[] modulus = DerItem(pair, ref p, 0x02);
            byte[] exponent = DerItem(pair, ref p, 0x02);
            return new RSAParameters { Modulus = TrimZero(modulus), Exponent = TrimZero(exponent) };
        }

        // RSA with SHA-256, as `openssl dgst -sha256 -sign` writes it. A key under 2048 bits never passes.
        internal static bool VerifyIndex(byte[] index, byte[] signature, string pem)
        {
            try {
                RSAParameters key = ReadPublicKey(pem);
                if (key.Modulus.Length < 256) return false;
                using (var rsa = new RSACryptoServiceProvider())
                using (var hash = new SHA256CryptoServiceProvider()) {
                    rsa.ImportParameters(key);
                    return rsa.VerifyData(index, hash, signature);
                }
            } catch (Exception) { return false; }
        }

        // ------------------------------------------------------------ the network

        static HttpWebRequest StoreRequest(string relative)
        {
            string url = StoreUrl + relative;
            ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072; // TLS 1.2.
            var request = (HttpWebRequest)WebRequest.Create(url);
            request.UserAgent = "SubVulpes-launcher";
            request.Timeout = 15000;
            request.ReadWriteTimeout = 15000;
            request.AllowAutoRedirect = false;
            return request;
        }

        static void CopyLimited(Stream source, Stream target, long limit)
        {
            var buffer = new byte[65536];
            long total = 0;
            int count;
            while ((count = source.Read(buffer, 0, buffer.Length)) > 0) {
                total += count;
                if (total > limit) throw new InvalidDataException("The file is too big.");
                target.Write(buffer, 0, count);
            }
        }

        // A file of the Workshop in memory. The limit stops a file that is too big.
        static byte[] StoreRead(string relative, long limit)
        {
            using (var response = (HttpWebResponse)StoreRequest(relative).GetResponse())
            using (var source = response.GetResponseStream())
            using (var memory = new MemoryStream()) {
                CopyLimited(source, memory, limit);
                return memory.ToArray();
            }
        }

        // A file of the Workshop on disk. A part file holds the data until the download ends.
        static void StoreDownload(string relative, string target, long limit)
        {
            string part = target + ".part";
            Directory.CreateDirectory(Path.GetDirectoryName(target));
            try {
                using (var response = (HttpWebResponse)StoreRequest(relative).GetResponse())
                using (var source = response.GetResponseStream())
                using (var file = new FileStream(part, FileMode.Create))
                    CopyLimited(source, file, limit);
                if (System.IO.File.Exists(target)) System.IO.File.Delete(target);
                System.IO.File.Move(part, target);
            } finally {
                if (System.IO.File.Exists(part)) System.IO.File.Delete(part);
            }
        }

        // The package must have the size and the SHA-256 of the signed index. A file that does not match is deleted.
        internal static void VerifyStorePackage(string path, long size, string sha256)
        {
            try {
                if (new FileInfo(path).Length != size) throw new InvalidDataException("The size of the package does not match the catalog.");
                using (var algorithm = SHA256.Create())
                using (var stream = System.IO.File.OpenRead(path)) {
                    string actual = BitConverter.ToString(algorithm.ComputeHash(stream)).Replace("-", "").ToLowerInvariant();
                    if (actual != sha256) throw new InvalidDataException("The hash of the package does not match the catalog.");
                }
            } catch {
                if (System.IO.File.Exists(path)) System.IO.File.Delete(path);
                throw;
            }
        }

        // ------------------------------------------------------------ the index

        // The plugins of a verified index. A pictures path is a file in the cache. An entry that breaks a rule is skipped.
        internal static List<CatalogEntry> ParseStoreIndex(string json, string cache)
        {
            var list = new List<CatalogEntry>();
            var root = new JavaScriptSerializer { MaxJsonLength = (int)StoreIndexLimit }.DeserializeObject(json) as Dictionary<string, object>;
            object format, plugins;
            if (root == null || !root.TryGetValue("format", out format) || !(format is int) || (int)format != 1)
                throw new InvalidDataException("This catalog needs a newer launcher.");
            if (!root.TryGetValue("plugins", out plugins) || !(plugins is object[])) throw new InvalidDataException("The catalog has no list of plugins.");
            foreach (object item in (object[])plugins) {
                var d = item as Dictionary<string, object>;
                if (d == null) continue;
                var e = new CatalogEntry { Id = CatalogText(d, "id", 47) };
                if (!PluginId(e.Id) || CatalogText(d, "kind", 16) != "plugin") continue;
                e.Source = CatalogText(d, "source", 16);
                if (e.Source != "Official" && e.Source != "Community") continue;
                e.Name = CatalogText(d, "name", 48);
                if (e.Name.Length == 0) e.Name = e.Id;
                e.Description = CatalogText(d, "description", 600);
                e.Author = CatalogText(d, "author", 48);
                e.Version = CatalogText(d, "version", 23);
                e.Channel = CatalogText(d, "channel", 16);
                if (e.Channel.Length == 0) e.Channel = "stable";
                e.License = CatalogText(d, "license", 32);
                e.Runtime = CatalogText(d, "runtime", 8);
                e.Capabilities.AddRange(ReadCapabilityList(d));
                object package;
                var p = d.TryGetValue("package", out package) ? package as Dictionary<string, object> : null;
                if (p == null) continue;
                string file = CatalogText(p, "file", 200), hash = CatalogText(p, "sha256", 64);
                long size;
                try { size = Convert.ToInt64(p["size"]); } catch (Exception) { continue; }
                if (!StorePath.IsMatch(file) || !file.StartsWith("packages/") || !Sha256Text.IsMatch(hash) || size <= 0 || size > StorePackageLimit) continue;
                e.Remote = true;
                e.PackageFile = file;
                e.Sha256 = hash;
                e.Size = size;
                // The fields above are the newest version. The optional list adds the older ones. An older launcher ignores it.
                var newest = ReadStoreVersion(d, e.Version, file, hash, size, e.Channel);
                if (newest != null) e.Versions.Add(newest);
                object more;
                if (d.TryGetValue("versions", out more) && more is object[])
                    foreach (object item2 in (object[])more) {
                        StoreVersion v = ReadStoreVersionItem(item2 as Dictionary<string, object>);
                        if (v == null) continue;
                        StoreVersion same = e.Versions.FirstOrDefault(x => x.Version == v.Version);
                        if (same != null) {
                            // The top level wins for the package. The list may add the changelog and the capabilities.
                            if (same.Changelog.Length == 0) same.Changelog = v.Changelog;
                            if (same.Capabilities.Count == 0) same.Capabilities.AddRange(v.Capabilities);
                            if (same.Breaking.Length == 0) same.Breaking = v.Breaking;
                        } else if (e.Versions.Count < StoreVersionsMax) e.Versions.Add(v);
                    }
                e.Versions.Sort((a, b) => SafeCompare(b.Version, a.Version));
                // An entry whose newest version has an odd number still installs, as before.
                if (e.Versions.Count == 0) e.Versions.Add(new StoreVersion { Version = e.Version, PackageFile = file, Sha256 = hash, Size = size, Channel = e.Channel });
                string icon = CatalogText(d, "icon", 200);
                if (StorePath.IsMatch(icon) && icon.StartsWith("icons/")) { e.IconRemote = icon; e.Icon = StoreFilePath(cache, icon); }
                object images;
                if (d.TryGetValue("images", out images) && images is object[])
                    foreach (object image in (object[])images) {
                        string name = image as string ?? "";
                        if (e.Images.Count < 8 && StorePath.IsMatch(name) && name.StartsWith("images/")) { e.ImageRemote.Add(name); e.Images.Add(StoreFilePath(cache, name)); }
                    }
                list.Add(e);
            }
            return list;
        }

        static int SafeCompare(string left, string right)
        {
            try { return ReleaseUpdate.CompareVersions(left, right); } catch (Exception) { return string.CompareOrdinal(left, right); }
        }

        // The version of the newest package, with the minimums that the top level of the entry may carry.
        static StoreVersion ReadStoreVersion(Dictionary<string, object> d, string version, string file, string hash, long size, string channel)
        {
            string serverMin = CatalogText(d, "server_min", 23);
            int minApi;
            if (!PluginVersionText.IsMatch(version) || !ReadMinApi(d, out minApi) || (serverMin.Length > 0 && !ServerMinText.IsMatch(serverMin))) return null;
            var v = new StoreVersion { Version = version, PackageFile = file, Sha256 = hash, Size = size, ServerMin = serverMin, MinApi = minApi, Channel = channel, Changelog = ReadChangelog(d), Breaking = ReadBreaking(d) };
            v.Capabilities.AddRange(ReadCapabilityList(d));
            return v;
        }

        // One item of the list `versions`: version, file, sha256, size, and the optional server_min, min_api, channel, changelog and capabilities.
        static StoreVersion ReadStoreVersionItem(Dictionary<string, object> d)
        {
            if (d == null) return null;
            string version = CatalogText(d, "version", 23), file = CatalogText(d, "file", 200), hash = CatalogText(d, "sha256", 64);
            string channel = CatalogText(d, "channel", 16), serverMin = CatalogText(d, "server_min", 23);
            long size;
            int minApi;
            try { size = Convert.ToInt64(d["size"]); } catch (Exception) { return null; }
            if (channel.Length == 0) channel = "stable";
            if (!PluginVersionText.IsMatch(version) || !StorePath.IsMatch(file) || !file.StartsWith("packages/") || !Sha256Text.IsMatch(hash) ||
                size <= 0 || size > StorePackageLimit || !ReadMinApi(d, out minApi) || (serverMin.Length > 0 && !ServerMinText.IsMatch(serverMin))) return null;
            var v = new StoreVersion { Version = version, PackageFile = file, Sha256 = hash, Size = size, ServerMin = serverMin, MinApi = minApi, Channel = channel, Changelog = ReadChangelog(d), Breaking = ReadBreaking(d) };
            v.Capabilities.AddRange(ReadCapabilityList(d));
            return v;
        }

        // A changelog is plain text with line breaks, at most 1200 characters. Other control characters go away.
        internal const int ChangelogMax = 1200;
        static string ReadChangelog(Dictionary<string, object> d)
        {
            object raw;
            if (!d.TryGetValue("changelog", out raw) || !(raw is string)) return "";
            var text = new StringBuilder();
            foreach (char c in ((string)raw).Replace("\r\n", "\n").Replace('\r', '\n')) {
                if (c == '\n' || c == '\t' || c >= ' ') text.Append(c);
                if (text.Length >= ChangelogMax) break;
            }
            return text.ToString().Trim().Replace("\n", "\r\n");
        }

        // A version may say what can break in an old server or an old config. Plain text, one line, at most 300 characters.
        internal const int BreakingMax = 300;
        static string ReadBreaking(Dictionary<string, object> d)
        {
            object raw;
            if (!d.TryGetValue("breaking", out raw) || !(raw is string)) return "";
            var text = new StringBuilder();
            foreach (char c in (string)raw) {
                text.Append(c < ' ' ? ' ' : c);
                if (text.Length >= BreakingMax) break;
            }
            return text.ToString().Trim();
        }

        // The capabilities of an entry or of a version: at most 24 short lower-case names.
        static readonly Regex CapabilityName = new Regex(@"\A[a-z][a-z_]{0,23}\z");
        static List<string> ReadCapabilityList(Dictionary<string, object> d)
        {
            var list = new List<string>();
            object raw;
            if (d.TryGetValue("capabilities", out raw) && raw is object[])
                foreach (object item in (object[])raw) {
                    string name = item as string ?? "";
                    if (CapabilityName.IsMatch(name) && list.Count < 24 && !list.Contains(name)) list.Add(name);
                }
            return list;
        }

        // min_api is a whole number from 1 to 99. Without it the plugin needs API 1.
        static bool ReadMinApi(Dictionary<string, object> d, out int value)
        {
            object raw;
            value = 1;
            if (!d.TryGetValue("min_api", out raw)) return true;
            try { value = Convert.ToInt32(raw); } catch (Exception) { return false; }
            return value >= 1 && value <= 99;
        }

        // The digit groups at the start of a version, as the host reads them. A suffix such as -rc.1 does not count.
        static int[] VersionGroups(string text)
        {
            Match m = Regex.Match(text ?? "", @"^v?([0-9]+(?:\.[0-9]+){0,7})");
            if (!m.Success) return null;
            string[] parts = m.Groups[1].Value.Split('.');
            var groups = new int[8];
            for (int i = 0; i < parts.Length; i++) { long n; if (!long.TryParse(parts[i], out n) || n > int.MaxValue) return null; groups[i] = (int)n; }
            return groups;
        }

        // True when the server is older than the minimum. A server with no readable version (a development build) accepts any plugin.
        internal static bool ServerBelow(string server, string minimum)
        {
            int[] have = VersionGroups(server), want = VersionGroups(minimum);
            if (have == null || want == null) return false;
            for (int i = 0; i < 8; i++) if (have[i] != want[i]) return have[i] < want[i];
            return false;
        }

        // A version that this server can run, and that the user wants to see. A beta shows only when Include Beta Versions is on.
        internal static bool VersionUsable(StoreVersion v, string server, bool betas)
        {
            if (v.MinApi > SupportedPluginApi) return false;
            if (v.Channel != "stable" && !betas) return false;
            return v.ServerMin.Length == 0 || !ServerBelow(server, v.ServerMin);
        }

        internal static List<StoreVersion> UsableVersions(CatalogEntry c, string server, bool betas)
        {
            return c.Versions.Where(v => VersionUsable(v, server, betas)).ToList();
        }

        // The newest version that this server can run, or nothing.
        internal static StoreVersion BestVersion(CatalogEntry c, string server, bool betas)
        {
            return UsableVersions(c, server, betas).FirstOrDefault();
        }

        // The number of the server in this folder, such as v0.2.0. Empty when version.txt is missing.
        string ServerNumber()
        {
            try {
                string v = Path.Combine(folder, "version.txt");
                return System.IO.File.Exists(v) ? (System.IO.File.ReadLines(v).FirstOrDefault() ?? "").Trim() : "";
            } catch (Exception) { return ""; }
        }

        StoreVersion BestVersion(CatalogEntry c) { return BestVersion(c, ServerNumber(), previewsBox != null && previewsBox.On); }

        internal static bool IsDowngrade(string installed, string chosen) { return NewerVersion(installed, chosen); }

        // The check runs at most once in 24 hours. A saved time that is unreadable or in the future counts as due.
        internal static bool PluginCheckDue(string last, DateTime nowUtc)
        {
            DateTime when;
            if (!DateTime.TryParse(last ?? "", CultureInfo.InvariantCulture, DateTimeStyles.AdjustToUniversal | DateTimeStyles.AssumeUniversal, out when)) return true;
            return when > nowUtc || nowUtc - when >= TimeSpan.FromHours(24);
        }

        // The last catalog that passed the signature check, from the cache. Nothing when the cache is empty or the seal fails.
        static string ReadStoreCache(string cache)
        {
            try {
                string index = Path.Combine(cache, "index.json"), seal = Path.Combine(cache, "index.json.sig");
                if (!System.IO.File.Exists(index) || !System.IO.File.Exists(seal)) return null;
                byte[] data = System.IO.File.ReadAllBytes(index);
                return VerifyIndex(data, System.IO.File.ReadAllBytes(seal), StoreKeyText()) ? Encoding.UTF8.GetString(data) : null;
            } catch (Exception) { return null; }
        }

        // Loads the saved catalog at once, then asks the Workshop for a new one.
        async Task<bool> RefreshStore(bool quiet = false)
        {
            if (storeBusy) return false;
            storeBusy = true;
            storeAsked = true;
            bool fresh = false;
            string cache = StoreCacheFolder(), problem = "", json = null;
            try {
                byte[] index = null, seal = null;
                try {
                    await Task.Run(() => { index = StoreRead("index.json", StoreIndexLimit); seal = StoreRead("index.json.sig", 4096); });
                } catch (Exception) { problem = "The Workshop is not reachable. The saved catalog shows."; index = null; }
                if (index != null) {
                    if (VerifyIndex(index, seal, StoreKeyText())) {
                        Directory.CreateDirectory(cache);
                        System.IO.File.WriteAllBytes(Path.Combine(cache, "index.json"), index);
                        System.IO.File.WriteAllBytes(Path.Combine(cache, "index.json.sig"), seal);
                        json = Encoding.UTF8.GetString(index);
                        fresh = true;
                        settings.Set("plugin_check", DateTime.UtcNow.ToString("o", CultureInfo.InvariantCulture));
                        settings.Save();
                    } else problem = "The signature of the Workshop catalog does not match. The launcher refuses it.";
                }
                if (json == null) json = ReadStoreCache(cache);
                storeEntries.Clear();
                if (json != null) storeEntries.AddRange(ParseStoreIndex(json, cache));
            } catch (Exception ex) {
                problem = "Cannot read the Workshop catalog: " + ex.Message;
            } finally { storeBusy = false; }
            storeProblem = problem;
            if (problem.Length > 0 && !quiet) message.Text = problem;
            if (catalogList != null && catalogList.Visible) { LoadCatalog(); RenderCatalog(); }
            var pictures = PrefetchStorePictures();
            UpdateBadge();
            return fresh;
        }

        // The icons that the list needs. They load once and stay in the cache.
        async Task PrefetchStorePictures()
        {
            var missing = storeEntries.Where(e => e.IconRemote.Length > 0 && !System.IO.File.Exists(e.Icon)).ToList();
            if (missing.Count == 0) return;
            await Task.Run(() => {
                foreach (CatalogEntry e in missing)
                    try { StoreDownload(e.IconRemote, e.Icon, StoreIconLimit); } catch (Exception) { }
            });
            if (!IsDisposed && catalogList != null && catalogList.Visible) { LoadCatalog(); RenderCatalog(); }
        }

        // The large pictures load when the user opens the page of a plugin.
        async Task FetchStoreImages(CatalogEntry c)
        {
            await Task.Run(() => {
                for (int i = 0; i < c.Images.Count; i++)
                    if (!System.IO.File.Exists(c.Images[i]))
                        try { StoreDownload(c.ImageRemote[i], c.Images[i], StoreImageLimit); } catch (Exception) { }
            });
            if (!IsDisposed && catalogDetail != null && catalogDetail.Visible) ShowCatalogDetail(c);
        }

        // ------------------------------------------------------------ install and update

        static bool NewerVersion(string candidate, string installed)
        {
            try { return ReleaseUpdate.CompareVersions(candidate, installed) > 0; } catch (Exception) { return false; }
        }

        async Task InstallFromStore(CatalogEntry c, StoreVersion chosen = null)
        {
            if (pluginBusy || updating || storeBusy || !c.Remote) return;
            StoreVersion v = chosen ?? BestVersion(c);
            if (v == null) { message.Text = "No version of " + c.Name + " runs on this server."; return; }
            string target = StoreFilePath(StoreCacheFolder(), v.PackageFile);
            storeBusy = true;
            message.Text = "Downloading " + c.Name + " " + v.Version + "...";
            try {
                await Task.Run(() => { StoreDownload(v.PackageFile, target, v.Size); VerifyStorePackage(target, v.Size, v.Sha256); });
            } catch (Exception ex) {
                message.Text = "Cannot download " + c.Name + ": " + ex.Message;
                return;
            } finally { storeBusy = false; }
            if (server != null) {
                pendingInstalls.RemoveAll(x => x[3] == c.Id);
                pendingInstalls.Add(new[] { target, v.Sha256, c.Source, c.Id });
                SavePendingInstalls();
                message.Text = c.Name + " " + v.Version + " installs at the next start of the server.";
                if (catalogList != null && catalogList.Visible) RenderCatalog();
                return;
            }
            await InstallPlugin(target, v.Sha256, c.Source);
        }

        // The versions of a plugin that this server can run, other than the one that is installed.
        int OtherVersions(CatalogEntry c, string have)
        {
            return UsableVersions(c, ServerNumber(), previewsBox != null && previewsBox.On).Count(v => v.Version != have);
        }

        static string VersionLabel(StoreVersion v, string have)
        {
            return v.Version + (v.Version == have ? " (installed)" : "") + (v.Channel != "stable" ? " (" + v.Channel + ")" : "");
        }

        // The text of the main button for a choice of version.
        internal static string ChangeVerb(string have, string chosen)
        {
            if (have.Length == 0) return "Install";
            return NewerVersion(chosen, have) ? "Update" : IsDowngrade(have, chosen) ? "Go back" : "Install";
        }

        // The window of an install, an update or a change of version. The user picks a version and reads what changed and what it can do.
        async Task ChooseAndInstall(CatalogEntry c)
        {
            if (pluginBusy || updating || storeBusy || !c.Remote) return;
            PluginMetadata installed = plugins.FirstOrDefault(p => p.Id == c.Id);
            string have = installed == null ? "" : installed.Version;
            List<StoreVersion> usable = UsableVersions(c, ServerNumber(), previewsBox != null && previewsBox.On);
            if (usable.Count == 0) { message.Text = "No version of " + c.Name + " runs on this server."; return; }
            int start = usable.FindIndex(v => v.Version != have);
            if (start < 0) { message.Text = c.Name + " has no other version that this server can run."; return; }
            List<string> labels = usable.Select(v => VersionLabel(v, have)).ToList();
            string title = (have.Length == 0 ? "Install " : "Change version of ") + c.Name;
            string subtitle = ByLine(c.Author, "") + " · " + (c.Source.Length > 0 ? c.Source : "Workshop") + " plugin";
            using (var dialog = new ChangeDialog(title, subtitle, labels, start, i => ChangeVerb(have, usable[i].Version),
                (box, i) => RenderVersionNotes(box, c, usable[i], have), i => usable[i].Version != have)) {
                if (dialog.ShowDialog(this) != DialogResult.OK) return;
                await InstallFromStore(c, usable[dialog.PickedIndex]);
            }
        }

        // ------------------------------------------------------------ what the window shows

        static void NoteHeading(RichTextBox box, string text)
        {
            box.SelectionFont = UI.F(9.5f, FontStyle.Bold);
            box.SelectionColor = UI.T.Fg;
            box.AppendText(text + "\r\n");
        }

        static void NoteText(RichTextBox box, string text, Color color)
        {
            box.SelectionFont = UI.F(9.5f, FontStyle.Regular);
            box.SelectionColor = color;
            box.AppendText(text + "\r\n");
        }

        // The changelog of one version, a warning for an older one, and what the plugin can do.
        internal static void RenderVersionNotes(RichTextBox box, CatalogEntry c, StoreVersion v, string have)
        {
            box.Clear();
            NoteHeading(box, "What changed in " + v.Version);
            NoteText(box, v.Changelog.Length > 0 ? v.Changelog : "The author wrote no changelog for this version.", UI.T.Fg);
            if (v.Breaking.Length > 0 && !(have.Length > 0 && IsDowngrade(have, v.Version))) {
                NoteText(box, "", UI.T.Fg);
                NoteHeading(box, "This version can break your setup");
                NoteText(box, v.Breaking, PluginDangerColor("high"));
            }
            if (have.Length > 0 && IsDowngrade(have, v.Version)) {
                NoteText(box, "", UI.T.Fg);
                NoteText(box, "This is an older version than " + have + ". The data of the plugin stays in user\\plugins.", PluginDangerColor("high"));
            }
            NoteText(box, "", UI.T.Fg);
            NoteHeading(box, "What it can do");
            List<string> caps = v.Capabilities.Count > 0 ? v.Capabilities : c.Capabilities;
            if (caps.Count == 0) NoteText(box, "The Workshop does not list it. The launcher shows it again after the download.", UI.T.Muted);
            foreach (string cap in caps) {
                string what;
                if (!CapText.TryGetValue(cap, out what)) what = cap;
                bool low = cap == "read" || cap == "storage" || cap == "config";
                NoteText(box, "•  " + what, low ? UI.T.Fg : PluginDangerColor("high"));
            }
            if (c.Runtime.Length == 0 || c.Runtime == "native")
                NoteText(box, "•  Runs inside the server with full access", PluginDangerColor("highest"));
            box.SelectionStart = 0;
            box.SelectionLength = 0;
        }

        // The notes of a release: the words before the download line, then the list of changes. The other lines are for the web page.
        internal static string ReleaseNotes(string body)
        {
            if (string.IsNullOrWhiteSpace(body)) return "";
            string text = body.Replace("\r\n", "\n").Replace('\r', '\n');
            int download = text.IndexOf("**Download:**", StringComparison.Ordinal);
            string intro = download >= 0 ? text.Substring(0, download) : text;
            string changes = "";
            Match m = Regex.Match(text, @"^#{2,3} Changes\s*$", RegexOptions.Multiline);
            if (m.Success) {
                changes = text.Substring(m.Index + m.Length);
                int log = changes.IndexOf("**Full Changelog**", StringComparison.Ordinal);
                if (log >= 0) changes = changes.Substring(0, log);
            }
            string notes = (intro.Trim() + "\n\n" + changes.Trim()).Trim();
            if (notes.Length > 3000) notes = notes.Substring(0, 3000).TrimEnd() + "...";
            return notes.Replace("\n", "\r\n");
        }

        static void RenderReleaseNotes(RichTextBox box, string notes)
        {
            box.Clear();
            NoteHeading(box, "What changed");
            NoteText(box, notes.Length > 0 ? notes : "This release has no notes.", UI.T.Fg);
            box.SelectionStart = 0;
            box.SelectionLength = 0;
        }

        // ------------------------------------------------------------ the update check

        // The plugins that are installed and that the index offers in a newer version that this server can run.
        List<CatalogEntry> UpdatableEntries()
        {
            string server = ServerNumber();
            bool betas = previewsBox != null && previewsBox.On;
            var list = new List<CatalogEntry>();
            foreach (CatalogEntry c in storeEntries) {
                PluginMetadata have = plugins.FirstOrDefault(p => p.Id == c.Id);
                StoreVersion best = BestVersion(c, server, betas);
                if (c.Remote && have != null && best != null && NewerVersion(best.Version, have.Version)) list.Add(c);
            }
            return list;
        }

        // The newer version of an installed plugin that this server can run, or nothing.
        StoreVersion UpdateFor(PluginMetadata p)
        {
            CatalogEntry c = storeEntries.FirstOrDefault(e => e.Id == p.Id && e.Remote);
            StoreVersion best = c == null ? null : BestVersion(c);
            return best != null && NewerVersion(best.Version, p.Version) ? best : null;
        }

        // The state of a plugin, and a note when the Workshop has a newer version.
        string StateLabel(PluginMetadata p)
        {
            return StateText(p.State) + (UpdateFor(p) != null ? " (New Update)" : "");
        }

        // A number on the Plugins tab: how many installed plugins have an update.
        void UpdateBadge()
        {
            if (IsDisposed || !tabLabels.ContainsKey("Plugins")) return;
            int count = UpdatableEntries().Count;
            tabLabels["Plugins"].SetText(count > 0 ? "Plugins (" + count + ")" : "Plugins");
        }

        // At the start, and then once a day, the launcher reads the Workshop index in the background. A failure is silent.
        async Task StartupPluginCheck()
        {
            try {
                await Task.Delay(2500);
                if (IsDisposed) return;
                if (plugins.Count == 0 && server == null && !pluginBusy && System.IO.File.Exists(Path.Combine(folder, "subvulpes_server.exe"))) {
                    string before = message.Text;
                    await RefreshPlugins();
                    if (message.Text.StartsWith("Cannot list plugins")) message.Text = before;
                }
                if (PluginCheckDue(settings.Get("plugin_check", ""), DateTime.UtcNow)) await RefreshStore(true);
                else if (storeEntries.Count == 0) {
                    string saved = ReadStoreCache(StoreCacheFolder());
                    if (saved != null) storeEntries.AddRange(ParseStoreIndex(saved, StoreCacheFolder()));
                }
                UpdateBadge();
                await AutoInstallOfficial();
            } catch (Exception) { }
        }

        // With the setting on, an official plugin installs its update by itself. A server that runs waits for its next start.
        async Task AutoInstallOfficial()
        {
            if (autoOfficialBox == null || !autoOfficialBox.On) return;
            var held = new List<string>();
            foreach (CatalogEntry c in UpdatableEntries().Where(e => e.Source == "Official").ToList()) {
                if (IsDisposed || pluginBusy || updating) break;
                // An update that can break an old setup waits for the user.
                if (BestVersion(c).Breaking.Length > 0) { held.Add(c.Name); continue; }
                await InstallFromStore(c);
            }
            if (held.Count > 0 && !IsDisposed)
                message.Text = string.Join(", ", held.ToArray()) + (held.Count == 1 ? " has an update" : " have updates") + " that can break your setup. Open My Plugins and read the notes before you update.";
        }

        // The installs that wait for a server that runs are in a small file, so a closed launcher keeps them.
        void LoadPendingInstalls()
        {
            pendingInstalls.Clear();
            try {
                string file = Path.Combine(StoreCacheFolder(), "pending.txt");
                if (!System.IO.File.Exists(file)) return;
                foreach (string line in System.IO.File.ReadAllLines(file, Encoding.UTF8)) {
                    string[] f = line.Split('\t');
                    if (f.Length == 4 && System.IO.File.Exists(f[0]) && Sha256Text.IsMatch(f[1]) && PluginId(f[3]) && (f[2] == "Official" || f[2] == "Community"))
                        pendingInstalls.Add(f);
                }
            } catch (Exception) { pendingInstalls.Clear(); }
        }

        void SavePendingInstalls()
        {
            try {
                Directory.CreateDirectory(StoreCacheFolder());
                System.IO.File.WriteAllLines(Path.Combine(StoreCacheFolder(), "pending.txt"),
                    pendingInstalls.Select(x => string.Join("\t", x)).ToArray(), Encoding.UTF8);
            } catch (Exception) { }
        }

        // The server never restarts by itself. The installs that waited run just before the next start that the user makes.
        async Task ApplyPendingInstalls()
        {
            var work = pendingInstalls.ToList();
            pendingInstalls.Clear();
            SavePendingInstalls();
            foreach (string[] item in work) await InstallPlugin(item[0], item[1], item[2]);
            StartServer();
        }

        // The short card for a plugin of the community: who made it, what it can do, and one box that allows it to run.
        bool ReviewCommunityCard(PluginMetadata metadata)
        {
            using (var card = new CommunityCard(metadata))
                return card.ShowDialog(this) == DialogResult.OK;
        }

        // One window for an install, an update, a change of version and an update of the launcher: a choice of version (when there
        // is one), the notes of the choice, and one main button that follows the choice.
        internal sealed class ChangeDialog : Form
        {
            internal readonly FlatButton Go;
            internal readonly Dropdown Picker;
            internal readonly RichTextBox Notes;
            readonly Func<int, string> verb;
            readonly Func<int, bool> allowed;
            readonly Action<RichTextBox, int> render;

            internal int PickedIndex { get { return Picker == null || Picker.SelectedIndex < 0 ? 0 : Picker.SelectedIndex; } }

            internal ChangeDialog(string title, string subtitle, List<string> choices, int selected, Func<int, string> verb,
                Action<RichTextBox, int> render, Func<int, bool> allowed)
            {
                this.verb = verb; this.render = render; this.allowed = allowed;
                Text = title;
                StartPosition = FormStartPosition.CenterParent;
                Font = UI.F(9.5f, FontStyle.Regular);
                ClientSize = new Size(520, 470);
                FormBorderStyle = FormBorderStyle.FixedDialog;
                MaximizeBox = false; MinimizeBox = false; ShowInTaskbar = false; KeyPreview = true;
                var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 5, Padding = new Padding(14) };
                table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
                for (int i = 0; i < 5; i++) table.RowStyles.Add(new RowStyle(i == 3 ? SizeType.Percent : SizeType.AutoSize, i == 3 ? 100 : 0));
                var heading = new Label { Text = title, AutoSize = true, UseMnemonic = false, Font = UI.F(12f, FontStyle.Bold), Margin = new Padding(3, 3, 3, 2) };
                var sub = new Label { Text = subtitle, AutoSize = true, MaximumSize = new Size(480, 0), UseMnemonic = false, Margin = new Padding(3, 0, 3, 10), Tag = "muted" };
                var pick = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, FlowDirection = FlowDirection.LeftToRight, WrapContents = false, Margin = new Padding(0, 0, 0, 8) };
                if (choices != null && choices.Count > 0) {
                    Picker = new Dropdown(260);
                    Picker.Items.AddRange(choices.Cast<object>().ToArray());
                    Picker.SelectedIndex = Math.Max(0, Math.Min(selected, choices.Count - 1));
                    Picker.AccessibleName = "Version";
                    var label = new Label { Text = "Version", AutoSize = true, UseMnemonic = false, Margin = new Padding(3, 8, 12, 0) };
                    pick.Controls.Add(label);
                    pick.Controls.Add(Picker);
                }
                Notes = new RichTextBox { ReadOnly = true, Dock = DockStyle.Fill, Tag = "doc", BorderStyle = BorderStyle.None, DetectUrls = false, TabStop = false, AccessibleName = "What changed and what the plugin can do" };
                var actions = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft, Margin = new Padding(0, 10, 0, 0) };
                Go = new FlatButton("Install") { Kind = "primary" };
                var cancel = new FlatButton("Cancel") { Margin = new Padding(0, 0, 8, 0) };
                cancel.Click += delegate { DialogResult = DialogResult.Cancel; Close(); };
                Go.Click += delegate { if (Go.Enabled) { DialogResult = DialogResult.OK; Close(); } };
                actions.Controls.Add(Go);
                actions.Controls.Add(cancel);
                table.Controls.Add(heading, 0, 0);
                table.Controls.Add(sub, 0, 1);
                table.Controls.Add(pick, 0, 2);
                table.Controls.Add(Notes, 0, 3);
                table.Controls.Add(actions, 0, 4);
                Controls.Add(table);
                Skin.Window(this, UI.T);
                HandleCreated += delegate { Skin.Window(this, UI.T); };
                KeyDown += (sender, e) => { if (e.KeyCode == Keys.Escape) { DialogResult = DialogResult.Cancel; Close(); } };
                if (Picker != null) Picker.SelectedIndexChanged += delegate { ShowChoice(); };
                ShowChoice();
            }

            void ShowChoice()
            {
                int i = PickedIndex;
                Go.Text = verb(i);
                Go.Enabled = allowed(i);
                Go.Invalidate();
                render(Notes, i);
            }
        }

        internal sealed class CommunityCard : Form
        {
            internal readonly CheckBox Allow;
            internal readonly FlatButton InstallButton;

            internal CommunityCard(PluginMetadata m)
            {
                Text = "Install " + m.DisplayName;
                StartPosition = FormStartPosition.CenterParent;
                Font = UI.F(9.5f, FontStyle.Regular);
                ClientSize = new Size(480, 330);
                FormBorderStyle = FormBorderStyle.FixedDialog;
                MaximizeBox = false; MinimizeBox = false; ShowInTaskbar = false; KeyPreview = true;
                var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 5, Padding = new Padding(14) };
                table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
                for (int i = 0; i < 5; i++) table.RowStyles.Add(new RowStyle(i == 2 ? SizeType.Percent : SizeType.AutoSize, i == 2 ? 100 : 0));
                var title = new Label { Text = m.DisplayName + " " + m.Version, AutoSize = true, UseMnemonic = false, Font = UI.F(12f, FontStyle.Bold), Margin = new Padding(3, 3, 3, 4) };
                string by = (m.Author.Length == 0 ? "Author not stated" : "By " + m.Author) + ". Community plugin.";
                var info = new Label { Text = by + (m.Description.Length > 0 ? "\r\n" + m.Description : ""), AutoSize = true, MaximumSize = new Size(440, 0), UseMnemonic = false, Margin = new Padding(3, 3, 3, 10) };
                var can = new RichTextBox { ReadOnly = true, Dock = DockStyle.Fill, Tag = "doc", BorderStyle = BorderStyle.None, DetectUrls = false, TabStop = false, AccessibleName = "What the plugin can do" };
                foreach (PluginDanger danger in m.Dangers.OrderByDescending(d => d.Level == "highest" ? 2 : d.Level == "high" ? 1 : 0)) {
                    can.SelectionColor = PluginDangerColor(danger.Level);
                    can.AppendText(danger.Capability + " (" + danger.Level + "): " + danger.Text + "\r\n");
                }
                can.SelectionStart = 0; can.SelectionLength = 0;
                Allow = new CheckBox { Text = "Allow this plugin to run on my server", AutoSize = true, Checked = false, Tag = "warn", Margin = new Padding(3, 12, 3, 8) };
                var actions = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft };
InstallButton = new FlatButton("Install") { Kind = "primary", Enabled = false };
                var cancel = new FlatButton("Cancel") { Margin = new Padding(0, 0, 8, 0) };
                cancel.Click += delegate { DialogResult = DialogResult.Cancel; Close(); };
                Allow.CheckedChanged += delegate { InstallButton.Enabled = CanApprovePlugin(m.Runtime, Allow.Checked); };
                InstallButton.Click += delegate { if (CanApprovePlugin(m.Runtime, Allow.Checked)) { DialogResult = DialogResult.OK; Close(); } };
                actions.Controls.Add(InstallButton);
                actions.Controls.Add(cancel);
                table.Controls.Add(title, 0, 0);
                table.Controls.Add(info, 0, 1);
                table.Controls.Add(can, 0, 2);
                table.Controls.Add(Allow, 0, 3);
                table.Controls.Add(actions, 0, 4);
                Controls.Add(table);
                Skin.Window(this, UI.T);
                HandleCreated += delegate { Skin.Window(this, UI.T); };
                KeyDown += (sender, e) => { if (e.KeyCode == Keys.Escape) { DialogResult = DialogResult.Cancel; Close(); } };
            }
        }
    }
}
