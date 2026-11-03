// The Workshop client: the signed index, the cache, the downloads and the installs that wait for the next start.
using System;
using System.Collections.Generic;
using System.Drawing;
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

namespace SubVulpes
{
    partial class LauncherForm
    {
        const string StoreUrl = "https://raw.githubusercontent.com/TheGreatVulpzin/SubVulpes-Workshop/dist/";
        const long StoreIndexLimit = 2 * 1024 * 1024, StorePackageLimit = 70L * 1024 * 1024, StoreIconLimit = 131072, StoreImageLimit = 2 * 1024 * 1024;
        static readonly Regex StorePath = new Regex(@"\A(icons|thumbs|images|packages)/[A-Za-z0-9][A-Za-z0-9._+-]{0,119}\z");
        static readonly Regex Sha256Text = new Regex(@"\A[0-9a-f]{64}\z");

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
        async Task RefreshStore()
        {
            if (storeBusy) return;
            storeBusy = true;
            storeAsked = true;
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
                    } else problem = "The signature of the Workshop catalog does not match. The launcher refuses it.";
                }
                if (json == null) json = ReadStoreCache(cache);
                storeEntries.Clear();
                if (json != null) storeEntries.AddRange(ParseStoreIndex(json, cache));
            } catch (Exception ex) {
                problem = "Cannot read the Workshop catalog: " + ex.Message;
            } finally { storeBusy = false; }
            storeProblem = problem;
            if (problem.Length > 0) message.Text = problem;
            if (catalogList != null && catalogList.Visible) { LoadCatalog(); RenderCatalog(); }
            var pictures = PrefetchStorePictures();
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

        async Task InstallFromStore(CatalogEntry c)
        {
            if (pluginBusy || updating || storeBusy || !c.Remote) return;
            string target = StoreFilePath(StoreCacheFolder(), c.PackageFile);
            storeBusy = true;
            message.Text = "Downloading " + c.Name + "...";
            try {
                await Task.Run(() => { StoreDownload(c.PackageFile, target, c.Size); VerifyStorePackage(target, c.Size, c.Sha256); });
            } catch (Exception ex) {
                message.Text = "Cannot download " + c.Name + ": " + ex.Message;
                return;
            } finally { storeBusy = false; }
            if (server != null) {
                pendingInstalls.RemoveAll(x => x[3] == c.Id);
                pendingInstalls.Add(new[] { target, c.Sha256, c.Source, c.Id });
                SavePendingInstalls();
                message.Text = c.Name + " installs at the next start of the server.";
                if (catalogList != null && catalogList.Visible) RenderCatalog();
                return;
            }
            await InstallPlugin(target, c.Sha256, c.Source);
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

        internal sealed class CommunityCard : Form
        {
            internal readonly CheckBox Allow;
            internal readonly Button InstallButton;

            internal CommunityCard(PluginMetadata m)
            {
                Text = "Install " + m.DisplayName;
                StartPosition = FormStartPosition.CenterParent;
                Font = SystemFonts.MessageBoxFont;
                ClientSize = new Size(480, 330);
                FormBorderStyle = FormBorderStyle.FixedDialog;
                MaximizeBox = false; MinimizeBox = false; ShowInTaskbar = false; KeyPreview = true;
                var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 5, Padding = new Padding(14) };
                table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
                for (int i = 0; i < 5; i++) table.RowStyles.Add(new RowStyle(i == 2 ? SizeType.Percent : SizeType.AutoSize, i == 2 ? 100 : 0));
                var title = new Label { Text = m.DisplayName + " " + m.Version, AutoSize = true, UseMnemonic = false, Font = new Font(Font.FontFamily, Font.Size + 2f, FontStyle.Bold), Margin = new Padding(3, 3, 3, 4) };
                string by = (m.Author.Length == 0 ? "Author not stated" : "By " + m.Author) + ". Community plugin.";
                var info = new Label { Text = by + (m.Description.Length > 0 ? "\r\n" + m.Description : ""), AutoSize = true, MaximumSize = new Size(440, 0), UseMnemonic = false, Margin = new Padding(3, 3, 3, 10) };
                var can = new RichTextBox { ReadOnly = true, Dock = DockStyle.Fill, BackColor = SystemColors.Window, BorderStyle = BorderStyle.FixedSingle, DetectUrls = false, TabStop = false };
                foreach (PluginDanger danger in m.Dangers.OrderByDescending(d => d.Level == "highest" ? 2 : d.Level == "high" ? 1 : 0)) {
                    can.SelectionColor = PluginDangerColor(danger.Level);
                    can.AppendText(danger.Capability + " (" + danger.Level + "): " + danger.Text + "\r\n");
                }
                can.SelectionStart = 0; can.SelectionLength = 0;
                Allow = new CheckBox { Text = "Allow this plugin to run on my server", AutoSize = true, Checked = false, ForeColor = Color.DarkRed, Margin = new Padding(3, 12, 3, 8) };
                var actions = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft };
                InstallButton = new Button { Text = "Install", AutoSize = true, Enabled = false };
                var cancel = new LinkLabel { Text = "Cancel", AutoSize = true, Margin = new Padding(12, 8, 12, 3) };
                cancel.LinkClicked += delegate { DialogResult = DialogResult.Cancel; Close(); };
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
                AcceptButton = InstallButton;
                KeyDown += (sender, e) => { if (e.KeyCode == Keys.Escape) { DialogResult = DialogResult.Cancel; Close(); } };
            }
        }
    }
}
