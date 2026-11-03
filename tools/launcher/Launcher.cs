// Server panel with separate developer captures and verified release updates.
//
// Build: tools\launcher\build.bat. It uses the C# compiler of .NET Framework 4, which is
// part of Windows, so the testers do not install anything. C# 5 only (no $"" strings).
// The exe is 32-bit, so that its dumps of the 32-bit server have the x86 view.
// Put the exe in the server folder, next to subvulpes_server.exe.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using SubVulpes.Ui;

namespace SubVulpes
{
    class ReleaseAsset
    {
        public string name { get; set; }
        public string browser_download_url { get; set; }
    }

    class ServerRelease
    {
        public string name { get; set; }
        public string tag_name { get; set; }
        public bool draft { get; set; }
        public bool prerelease { get; set; }
        public ReleaseAsset[] assets { get; set; }
        public string DisplayName { get { return string.IsNullOrWhiteSpace(name) ? tag_name : name; } }
    }

    static class ReleaseUpdate
    {
        const string Endpoint = "https://api.github.com/repos/TheGreatVulpzin/SubVulpes/releases";
        static readonly Regex Tag = new Regex(@"^v?(\d+)\.(\d+)\.(\d+)(?:-([0-9A-Za-z.-]+))?$");

        public static int CompareVersions(string left, string right)
        {
            Match a = Tag.Match(left), b = Tag.Match(right);
            if (!a.Success || !b.Success)
                throw new InvalidDataException("Version tag is unreadable.");
            for (int i = 1; i <= 3; i++) {
                int c = long.Parse(a.Groups[i].Value).CompareTo(long.Parse(b.Groups[i].Value));
                if (c != 0) return c;
            }
            string x = a.Groups[4].Value, y = b.Groups[4].Value;
            if (x == y) return 0;
            if (x == "") return 1;
            if (y == "") return -1;
            string[] xs = x.Split('.'), ys = y.Split('.');
            for (int i = 0; i < Math.Min(xs.Length, ys.Length); i++) {
                long xn, yn;
                bool nx = long.TryParse(xs[i], out xn), ny = long.TryParse(ys[i], out yn);
                int c = nx && ny ? xn.CompareTo(yn) : nx != ny ? (nx ? -1 : 1) : string.CompareOrdinal(xs[i], ys[i]);
                if (c != 0) return c;
            }
            return xs.Length.CompareTo(ys.Length);
        }

        public static ServerRelease Select(IEnumerable<ServerRelease> releases, string current, bool previews)
        {
            ServerRelease best = null;
            foreach (var release in releases) {
                if (release.draft || (!previews && release.prerelease) || !Tag.IsMatch(release.tag_name ?? ""))
                    continue;
                if (!previews && Tag.Match(release.tag_name).Groups[4].Success)
                    continue;
                if (CompareVersions(release.tag_name, current) > 0 &&
                    (best == null || CompareVersions(release.tag_name, best.tag_name) > 0))
                    best = release;
            }
            return best;
        }

        static HttpWebRequest Request(string url)
        {
            if (new Uri(url).Scheme != Uri.UriSchemeHttps)
                throw new InvalidDataException("Download URL is not HTTPS");
            ServicePointManager.SecurityProtocol = (SecurityProtocolType)3072; // TLS 1.2.
            var request = (HttpWebRequest)WebRequest.Create(url);
            request.UserAgent = "SubVulpes-launcher";
            request.Accept = "application/vnd.github+json";
            request.Timeout = 15000;
            request.ReadWriteTimeout = 15000;
            return request;
        }

        static string Read(string url)
        {
            using (var response = Request(url).GetResponse())
            using (var reader = new StreamReader(response.GetResponseStream()))
                return reader.ReadToEnd();
        }

        public static ServerRelease Find(string current, bool previews)
        {
            var releases = new List<ServerRelease>();
            var json = new JavaScriptSerializer { MaxJsonLength = 8 * 1024 * 1024 };
            for (int page = 1; ; page++) {
                var batch = json.Deserialize<ServerRelease[]>(Read(Endpoint + "?per_page=100&page=" + page));
                releases.AddRange(batch);
                if (batch.Length < 100) break;
            }
            return Select(releases, current, previews);
        }

        public static void Verify(string file, string checksum)
        {
            Match hash = Regex.Match(checksum.Trim(), @"\A([0-9a-fA-F]{64})(?:[ \t]+[^\r\n]+)?\s*\z");
            if (!hash.Success)
                throw new InvalidDataException("File hash (SHA-256) is invalid.");
            using (var algorithm = SHA256.Create())
            using (var stream = File.OpenRead(file)) {
                string actual = BitConverter.ToString(algorithm.ComputeHash(stream)).Replace("-", "");
                if (!actual.Equals(hash.Groups[1].Value, StringComparison.OrdinalIgnoreCase))
                    throw new InvalidDataException("File hash (SHA-256) mismatch");
            }
        }

        public static string Download(ServerRelease release, string directory)
        {
            string name = "SubVulpes-server-" + release.tag_name + ".zip";
            var assets = release.assets ?? new ReleaseAsset[0];
            var zip = assets.FirstOrDefault(a => a.name == name);
            var hash = assets.FirstOrDefault(a => a.name == name + ".sha256");
            if (zip == null || hash == null)
                throw new InvalidDataException("Release has no server zip or hash (SHA-256) file.");
            string checksum = Read(hash.browser_download_url);
            Directory.CreateDirectory(directory);
            string path = Path.Combine(directory, name);
            try {
                using (var response = Request(zip.browser_download_url).GetResponse())
                using (var source = response.GetResponseStream())
                using (var destination = new FileStream(path, FileMode.CreateNew))
                    source.CopyTo(destination);
                Verify(path, checksum);
                return path;
            } catch {
                if (File.Exists(path)) File.Delete(path);
                throw;
            }
        }
    }

    // A key=value file (config.txt, launcher.ini, session.ini). Save keeps the order of the
    // lines and the lines without a key. The server reads its files in the ANSI code page.
    class KeyFile
    {
        public readonly string FilePath;
        readonly List<string> lines = new List<string>();

        public KeyFile(string path)
        {
            FilePath = path;
            Load();
        }

        public bool Exists { get { return File.Exists(FilePath); } }

        public void Load()
        {
            lines.Clear();
            if (File.Exists(FilePath))
                lines.AddRange(File.ReadAllText(FilePath, Encoding.Default).Replace("\r\n", "\n").Split('\n'));
        }

        int Find(string key)
        {
            for (int i = 0; i < lines.Count; i++)
                if (lines[i].StartsWith(key + "="))
                    return i;
            return -1;
        }

        public string Get(string key, string fallback)
        {
            int i = Find(key);
            return i < 0 ? fallback : lines[i].Substring(key.Length + 1);
        }

        public void Set(string key, string value)
        {
            int i = Find(key);
            if (i < 0)
                lines.Add(key + "=" + value);
            else
                lines[i] = key + "=" + value;
        }

        public void Save()
        {
            File.WriteAllText(FilePath, string.Join("\r\n", lines.ToArray()), Encoding.Default);
        }
    }

    // The captures of one test. A preset is a named set of captures.
    class Captures
    {
        public bool Profile, Timers, Packets, FreezeDump;
        public string CrashDump = "small";  // none, small or full

        public Captures Copy() { return (Captures)MemberwiseClone(); }

        public bool Same(Captures o)
        {
            return Profile == o.Profile && Timers == o.Timers && Packets == o.Packets &&
                   FreezeDump == o.FreezeDump && CrashDump == o.CrashDump;
        }

        public string Summary()
        {
            var parts = new List<string>();
            if (Profile) parts.Add("speed profile");
            if (Timers) parts.Add("game timers");
            if (Packets) parts.Add("packets");
            if (CrashDump != "none") parts.Add(CrashDump + " crash dump");
            if (FreezeDump) parts.Add("Freeze Dump");
            return parts.Count == 0 ? "No Captures" : string.Join(", ", parts.ToArray());
        }
    }

    class Preset
    {
        public readonly string Name, Text;
        public readonly Captures Captures;
        public Preset(string name, string text, Captures c) { Name = name; Text = text; Captures = c; }
        public override string ToString() { return Name; }
    }

    // One item of the game mode list: the gametype value and its name.
    class Mode
    {
        public readonly string Value, Name;
        public Mode(string value, string name) { Value = value; Name = name; }
        public override string ToString() { return Name; }
    }

    partial class LauncherForm : Form
    {
        static readonly Regex TicksLine = new Regex(@"^Server status: ([0-9.]+) ticks/s, (\d+) players, (\d+) bots$");
        const int FreezeSeconds = 150;
        const string AppTitle = "SubVulpes Server Launcher";
        const string SettingsPageName = "Settings";
        const string IncompleteReviewText = "The plugin review is incomplete. Please try again.";
        const string InvalidReviewText = "The saved review is not valid. Review the plugin again.";
        const string StoppedUnexpectedly = "Stopped unexpectedly";
        const string UpdateInstalledText = "Update Installed!";
        const string NoSpeedText = "- ticks/s";
        const string NoPlayersText = "- players";
        static readonly string[] UpdateFiles = {
            "subvulpes_server.exe", "subvulpes_launcher.exe", "LEIA-ME.txt", "version.txt",
            "plugins/plugins.ini", "plugins/native-approvals.ini", "plugins/sv.status/plugin.ini", "plugins/sv.status/status.dll",
            "plugins/sv.permissions/plugin.ini", "plugins/sv.permissions/permissions.dll",
            "plugins/sv.admcmd/plugin.ini", "plugins/sv.admcmd/admcmd.dll",
            "install_plugin.ps1"
        };

        internal static string UpdateKey(string path)
        {
            if (string.IsNullOrEmpty(path) || path.IndexOfAny(new[] { '\\', ':', '\r', '\n', '\0' }) >= 0) return null;
            string[] parts = path.Split('/');
            if (parts.Any(p => p == "." || p == ".." || p.Length == 0)) return null;
            foreach (string allowed in UpdateFiles) {
                int count = allowed.Split('/').Length;
                if (parts.Length != count && parts.Length != count + 1) continue;
                if (path.Equals(allowed, StringComparison.OrdinalIgnoreCase) ||
                    path.EndsWith("/" + allowed, StringComparison.OrdinalIgnoreCase)) return allowed;
            }
            return null;
        }

        static void AssertUpdatePath(string path)
        {
            FileSystemInfo item = File.Exists(path) ? (FileSystemInfo)new FileInfo(path) : new DirectoryInfo(path);
            while (item != null) {
                if (item.Exists && (item.Attributes & FileAttributes.ReparsePoint) != 0)
                    throw new IOException("The update folder cannot contain symbolic links.");
                var directory = item as DirectoryInfo;
                item = directory != null ? directory.Parent : ((FileInfo)item).Directory;
            }
        }

        internal static void CreateUpdateDirectory(string path)
        {
            AssertUpdatePath(path);
            Directory.CreateDirectory(path);
        }

        static readonly Preset[] Presets = {
            new Preset("Normal",
                "Records the server speed. Saves a small report if the server crashes or freezes.",
                new Captures { Profile = true, FreezeDump = true }),
            new Preset("Speed Check",
                "Records the server speed. Also, saves a small report if the server crashes or freezes.",
                new Captures { Profile = true, Timers = true, FreezeDump = true }),
            new Preset("Crashhunt",
                "Records the network packets. Saves a full report if the server crashes or freezes.",
                new Captures { Packets = true, FreezeDump = true, CrashDump = "full" }),
            new Preset("Custom", "Records only what you select in the capture options.", null),
        };

        string folder, logsRoot, logFile;
        KeyFile config;
        readonly KeyFile settings;
        readonly string launcherFolder;
        System.Threading.EventWaitHandle stopEvent;
        bool developerRun, closeAfterStop, updating, configCreated;
        TaskCompletionSource<bool> serverStopped;
        Captures activeCaptures;
        long logOffset;
        string pendingLog = "";
        readonly ToolTip tips = new ToolTip();
        Captures captures = Presets[0].Captures.Copy();
        bool settingCaptures;

        // Frame
        TabControl mainTabs;
        readonly Dictionary<string, TabPage> pages = new Dictionary<string, TabPage>();
        readonly Dictionary<string, TabLabel> tabLabels = new Dictionary<string, TabLabel>();
        Panel tabBar, statusBar;
        ServerName serverName;
        StateLabel stateLabel;
        string serverState = "Stopped", speedStatus = NoSpeedText, playersStatus = NoPlayersText;
        Label message, missingText;
        FlatButton startStop, stopButton;
        bool pluginBusy;

        // The running test
        Process server;
        string testDir;
        DateTime started, lastSpeedLine;
        bool stopping, crashed, frozen;
        int maxPlayers;
        readonly List<double> speeds = new List<double>();
        readonly Timer clock = new Timer { Interval = 1000 };

        public LauncherForm(string folder)
        {
            launcherFolder = folder;
            settings = new KeyFile(Path.Combine(folder, "launcher.ini"));
            this.folder = settings.Get("server_folder", folder);
            if (!Directory.Exists(this.folder)) this.folder = folder;
            SetFolder(this.folder);
            Text = AppTitle + Version();
            Font = SystemFonts.MessageBoxFont;
            ForeColor = SystemColors.ControlText;
            AutoScaleMode = AutoScaleMode.Dpi;
            ClientSize = new Size(1000, 680);
            MinimumSize = new Size(820, 560);
            AllowDrop = true;
            DragEnter += (s, e) => { if (ZipOf(e) != null) e.Effect = DragDropEffects.Copy; };
            DragDrop += async (s, e) => { string zip = ZipOf(e); if (zip != null) await InstallUpdate(zip); };

            BackColor = UI.T.Win;
            // The tab headers of the control stay hidden. The bar at the top of the window draws the tabs.
            mainTabs = new TabControl { Dock = DockStyle.Fill, Appearance = TabAppearance.FlatButtons, SizeMode = TabSizeMode.Fixed, ItemSize = new Size(0, 1), Padding = new Point(0, 0) };
            // The status bar sits at the bottom, in a tone a little stronger than the window. A message fades after a few seconds.
            message = new Label { AutoSize = false, Dock = DockStyle.Fill, TextAlign = ContentAlignment.MiddleLeft, Margin = Padding.Empty, BackColor = Color.Transparent };
            statusBar = new Panel { Dock = DockStyle.Bottom, Height = 28, Padding = new Padding(14, 0, 14, 0), Tag = "subbar", BackColor = UI.T.SubBar };
            message.ForeColor = UI.T.Muted;
            statusBar.Controls.Add(message);
            statusBar.Controls.Add(new Panel { Dock = DockStyle.Top, Height = 1, Tag = "line" });
            var fade = new Timer { Interval = 9000 };
            fade.Tick += delegate { fade.Stop(); message.Text = ""; };
            message.TextChanged += delegate { fade.Stop(); if (message.Text.Length > 0) fade.Start(); };
            var layout = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 1, Padding = new Padding(4) };
            layout.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            layout.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            // The tab control draws a frame of its own. It sits a little outside its holder, so that the holder clips the frame.
            var tabHolder = new Panel { Dock = DockStyle.Fill, Margin = Padding.Empty };
            mainTabs.Dock = DockStyle.None;
            tabHolder.Controls.Add(mainTabs);
            tabHolder.Resize += delegate { mainTabs.SetBounds(-4, -8, tabHolder.Width + 8, tabHolder.Height + 12); };
            layout.Controls.Add(tabHolder, 0, 0);
            AddPage("Server", ServerPage());
            AddPage("Plugins", PluginsHost());
            AddPage(SettingsPageName, SettingsHost());
            AddPage("Help", HelpPage());
            Controls.Add(layout);
            Controls.Add(statusBar);
            tabBar = new Panel { Dock = DockStyle.Top, Height = 46, Padding = new Padding(8, 0, 8, 0), BackColor = UI.T.Bar, Tag = "bar" };
            serverName = new ServerName("SubVulpes");
            tabBar.Controls.Add(serverName);
            string[] tabOrder = { "Server", "Plugins", SettingsPageName, "Help" };
            for (int i = tabOrder.Length - 1; i >= 0; i--) {
                string tabName = tabOrder[i];
                var label = new TabLabel(tabName);
                label.Click += delegate { ShowPage(tabName); };
                tabLabels[tabName] = label;
                tabBar.Controls.Add(label);
            }
            mainTabs.SelectedIndexChanged += delegate {
                SyncTabs();
                // The list reads again each time the page opens. A server that runs answers with its own list.
                if (mainTabs.SelectedTab == pages["Plugins"] && !pluginBusy) { var read = RefreshPlugins(); }
            };
            Controls.Add(tabBar);
            SetDebugMode(debugModeBox.On);

            LoadSettings();
            LoadPendingInstalls();
            ShowServerName();
            string saved = settings.Get("preset", Presets[0].Name);
            presetBox.SelectedItem = Presets.FirstOrDefault(p => p.Name == saved) ?? Presets[0];
            if (presetBox.SelectedItem == Presets[Presets.Length - 1])
                captures = ReadCustomCaptures();
            ShowCaptures();
            SetState("Stopped");
            ResetStats();
            ShowPage("Server");
            ApplyLook();
            HandleCreated += delegate { Skin.Window(this, UI.T); };
            clock.Tick += delegate { OnClock(); };
            clock.Start();
            FormClosing += OnClosing;
            Load += delegate { CleanOldLauncher(); };
        }

        // The name of a version, such as Kit for 0.2.0. The table is tools/release/version-names.csv, inside the program.
        internal static string VersionName(string version)
        {
            Match m = Regex.Match(version ?? "", @"^v?(\d+)\.(\d+)\.\d+");
            if (!m.Success) return "";
            string exact = m.Groups[1].Value + "." + m.Groups[2].Value, major = m.Groups[1].Value, found = "";
            string table;
            using (var stream = System.Reflection.Assembly.GetExecutingAssembly().GetManifestResourceStream("SubVulpes.VersionNames"))
            using (var reader = new StreamReader(stream, Encoding.UTF8))
                table = reader.ReadToEnd();
            foreach (string line in table.Replace("\r", "").Split('\n')) {
                if (line.Length == 0 || line[0] == '#') continue;
                string[] f = line.Split(new[] { ',' }, 4);
                if (f.Length < 2) continue;
                if (f[0] == exact) return f[1];
                if (f[0] == major) found = f[1];
            }
            return found;
        }

        string AboutVersion()
        {
            string v = Path.Combine(folder, "version.txt");
            string number = File.Exists(v) ? (File.ReadLines(v).FirstOrDefault() ?? "").Trim() : "";
            if (number.Length == 0) return "Unknown";
            string name = VersionName(number);
            return name.Length > 0 ? number + " " + name : number;
        }

        string Version()
        {
            string v = Path.Combine(folder, "version.txt");
            return File.Exists(v) ? " - " + File.ReadLines(v).FirstOrDefault() : "";
        }

        void SetFolder(string path)
        {
            string selected = Path.GetFullPath(path);
            var nextConfig = new KeyFile(Path.Combine(selected, "config.txt"));
            bool absent = !nextConfig.Exists;
            if (absent) {
                using (var source = typeof(LauncherForm).Assembly.GetManifestResourceStream("SubVulpes.DefaultConfig"))
                using (var destination = File.Create(nextConfig.FilePath))
                    source.CopyTo(destination);
                nextConfig.Load();
            }
            folder = selected;
            logsRoot = Path.Combine(folder, "logs");
            logFile = Path.Combine(logsRoot, "server.log");
            config = nextConfig;
            configCreated = absent;
            if (missingText != null) ShowMissing(absent);
        }

        void OpenServerFolder(string path)
        {
            if (server != null || updating || pluginBusy) return;
            SetFolder(path);
            ClearPlugins();
            folderBox.Text = folder;
            settings.Set("server_folder", folder);
            settings.Save();
            LoadSettings();
            logOffset = 0;
            pendingLog = "";
            ClearConsole();
            Text = AppTitle + Version();
        }

        void ShowMissing(bool configCreated)
        {
            var missing = new List<string>();
            if (!Directory.Exists(Path.Combine(folder, "data"))) missing.Add("data/");
            if (configCreated) missing.Add("config.txt has been created.");
            missingText.Text = missing.Count == 0 ? "" : "Missing: " + string.Join(", ", missing.ToArray());
        }

        void ChooseFolder()
        {
            using (var dialog = new FolderBrowserDialog { SelectedPath = folder }) {
                if (dialog.ShowDialog(this) != DialogResult.OK) return;
                if (dirty && MessageBox.Show(this, "Do you want to save the changed settings?", Text, MessageBoxButtons.YesNo) == DialogResult.Yes)
                    SaveSettings();
                try {
                    OpenServerFolder(dialog.SelectedPath);
                } catch (Exception ex) {
                    MessageBox.Show(this, "Cannot open the server folder: " + ex.Message, Text);
                }
            }
        }

        void AddPage(string name, Control view)
        {
            var page = new TabPage(name) { Padding = Padding.Empty, UseVisualStyleBackColor = false, BackColor = UI.T.Win };
            view.Dock = DockStyle.Fill;
            page.Controls.Add(view);
            pages[name] = page;
            mainTabs.TabPages.Add(page);
        }

        void ShowPage(string name)
        {
            if (mainTabs.TabPages.Contains(pages[name])) mainTabs.SelectedTab = pages[name];
            SyncTabs();
        }

        // For a picture of one page: --page Plugins, --page Settings:2 (the number is the sub-tab).
        internal void OpenPage(string spec)
        {
            string[] part = spec.Split(':');
            ShowPage(part[0]);
            int sub = part.Length > 1 ? int.Parse(part[1]) : 0;
            if (part[0] == SettingsPageName) { if (sub == 3) SetDebug(true); ShowSub(sub); }
            if (part[0] == "Plugins") ShowPluginSub(sub);
        }

        // The bar shows which page is open.
        void SyncTabs()
        {
            foreach (var pair in tabLabels) {
                pair.Value.Selected = mainTabs.SelectedTab == pages[pair.Key];
                pair.Value.Invalidate();
            }
        }

        void SetState(string text)
        {
            serverState = text;
            startStop.Enabled = server == null && !updating && !pluginBusy;
            stopButton.Enabled = server != null && !stopping;
            restartButton.Enabled = server != null && !stopping && !updating && !pluginBusy;
            killButton.Enabled = server != null;
            browseButton.Enabled = server == null && !updating && !pluginBusy;
            presetBox.Enabled = server == null && !updating && !pluginBusy;
            nativeBypass.Enabled = server == null && !updating && !pluginBusy;
            UpdateRestartBadges();
            RefreshStatus();
        }

        void RefreshStatus()
        {
            string tone = serverState == "Running" ? "ok" : serverState == "Stopped" ? "muted" : (serverState == "Crashed" || serverState == StoppedUnexpectedly) ? "bad" : "warn";
            stateLabel.Set(serverState, tone);
            ShowServerName();
        }

        // The tab bar shows the name of the server, and the player count while the server runs.
        void ShowServerName()
        {
            if (serverName == null || nameBox == null) return;
            string name = nameBox.Text.Length == 0 ? "SubVulpes" : nameBox.Text;
            if (server != null && playersStatus != NoPlayersText) name += "  ·  " + playersStatus;
            serverName.SetText(name);
        }

        // ------------------------------------------------------------ captures

        void ShowCaptures()
        {
            settingCaptures = true;
            profileBox.On = captures.Profile;
            timersBox.On = captures.Timers;
            packetsBox.On = captures.Packets;
            freezeBox.On = captures.FreezeDump;
            dumpType.SelectedIndex = captures.CrashDump == "full" ? 2 : captures.CrashDump == "small" ? 1 : 0;
            settingCaptures = false;
            var preset = (Preset)presetBox.SelectedItem;
            presetText.Text = captures.Summary();
            tips.SetToolTip(presetBox, preset.Text);
        }

        void OnPreset()
        {
            var preset = (Preset)presetBox.SelectedItem;
            if (settingCaptures) return;
            captures = preset.Captures != null ? preset.Captures.Copy() : ReadCustomCaptures();
            ShowCaptures();
        }

        // A change on the Captures page makes the preset Custom, unless it gives a preset again.
        void OnCaptureChanged()
        {
            if (settingCaptures)
                return;
            captures = new Captures {
                Profile = profileBox.On, Timers = timersBox.On, Packets = packetsBox.On, FreezeDump = freezeBox.On,
                CrashDump = dumpType.SelectedIndex == 2 ? "full" : dumpType.SelectedIndex == 1 ? "small" : "none"
            };
            var match = Presets.FirstOrDefault(p => p.Captures != null && p.Captures.Same(captures)) ?? Presets[Presets.Length - 1];
            settingCaptures = true;
            presetBox.SelectedItem = match;
            settingCaptures = false;
            var preset = match;
            presetText.Text = captures.Summary();
            tips.SetToolTip(presetBox, preset.Text);
        }

        Captures ReadCustomCaptures()
        {
            return new Captures {
                Profile = settings.Get("custom.profile", "1") == "1", Timers = settings.Get("custom.timers", "0") == "1",
                Packets = settings.Get("custom.packets", "0") == "1", FreezeDump = settings.Get("custom.freeze", "1") == "1",
                CrashDump = settings.Get("custom.crashdump", "small")
            };
        }

        void SaveCaptureChoice()
        {
            var preset = (Preset)presetBox.SelectedItem;
            settings.Set("preset", preset.Name);
            if (preset.Captures == null) {
                settings.Set("custom.profile", captures.Profile ? "1" : "0");
                settings.Set("custom.timers", captures.Timers ? "1" : "0");
                settings.Set("custom.packets", captures.Packets ? "1" : "0");
                settings.Set("custom.freeze", captures.FreezeDump ? "1" : "0");
                settings.Set("custom.crashdump", captures.CrashDump);
            }
            settings.Save();
        }

        internal sealed class PluginMetadata
        {
            internal string Id, Version, Hash, PackageHash, State, Required, Reason, ManifestHash;
            internal readonly List<PluginDanger> Dangers = new List<PluginDanger>();
            internal string Runtime = "native", Capabilities = "", Name = "", Description = "", Author = "", Homepage = "";
            internal string DisplayName { get { return Name.Length == 0 ? Id : Name; } }
        }

        static bool PluginText(string text, int maximum)
        {
            if (text.Length > maximum) return false;
            foreach (char character in text) if (character < ' ' || character > '~') return false;
            return true;
        }

        static bool PluginId(string id)
        {
            return Regex.IsMatch(id, @"\A[a-z0-9][a-z0-9_.-]{0,46}\z") && !id.Contains("..") &&
                !id.EndsWith(".", StringComparison.Ordinal) &&
                !Regex.IsMatch(id, @"\A(con|prn|aux|nul|com[1-9]|lpt[1-9])(?:\.|\z)");
        }

        internal static PluginMetadata ParsePluginRow(string line, bool package)
        {
            if (line == null) return null;
            string[] fields = line.Split('\t');
            int count = package ? 5 : 7;
            if ((fields.Length != count && fields.Length != count + 6) ||
                fields[0] != (package ? "MODULE" : "PLUGIN") || !PluginId(fields[1]) ||
                !PluginText(fields[2], 23)) return null;
            int hash = package ? 3 : 5;
            if ((fields[hash].Length > 0 || package) && !Regex.IsMatch(fields[hash], @"\A[0-9a-f]{64}\z")) return null;
            if (package && !Regex.IsMatch(fields[4], @"\A[0-9a-f]{64}\z")) return null;
            var metadata = new PluginMetadata { Id = fields[1], Version = fields[2], Hash = fields[hash] };
            if (package) metadata.PackageHash = fields[4];
            else {
                if (!PluginText(fields[3], 16) || (fields[4] != "0" && fields[4] != "1") || !PluginText(fields[6], 127)) return null;
                metadata.State = fields[3]; metadata.Required = fields[4]; metadata.Reason = fields[6];
            }
            if (fields.Length == count) return metadata;
            metadata.Runtime = fields[count]; metadata.Capabilities = fields[count + 1]; metadata.Name = fields[count + 2];
            metadata.Description = fields[count + 3]; metadata.Author = fields[count + 4]; metadata.Homepage = fields[count + 5];
            if ((metadata.Runtime != "native" && metadata.Runtime != "lua" && metadata.Runtime != "wasm") ||
                !PluginText(metadata.Capabilities, 255) || !PluginText(metadata.Name, 48) ||
                !PluginText(metadata.Description, 160) || !PluginText(metadata.Author, 48) || !PluginText(metadata.Homepage, 96)) return null;
            var capabilities = new HashSet<string>(StringComparer.Ordinal);
            if (metadata.Capabilities.Length > 0) foreach (string capability in metadata.Capabilities.Split(',')) {
                if (!Regex.IsMatch(capability, @"\A[a-z][a-z_]{0,31}\z") || !capabilities.Add(capability)) return null;
            }
            if (metadata.Homepage.Length > 0) {
                Uri homepage;
                if (!Uri.TryCreate(metadata.Homepage, UriKind.Absolute, out homepage) ||
                    (homepage.Scheme != "http" && homepage.Scheme != "https") || metadata.Homepage.Any(char.IsWhiteSpace)) return null;
            }
            return metadata;
        }

        internal static string PluginReviewDetails(PluginMetadata metadata, string question, bool nativeApproval)
        {
            var text = new StringBuilder();
            if (metadata.Description.Length > 0) text.AppendLine(metadata.Description).AppendLine();
            if (metadata.Author.Length > 0) text.AppendLine("Author: " + metadata.Author);
            if (metadata.Homepage.Length > 0) text.AppendLine("Homepage: " + metadata.Homepage);
            text.AppendLine("Version: " + metadata.Version).AppendLine("Runtime: " + metadata.Runtime);
            text.AppendLine("Capabilities: " + (metadata.Capabilities.Length == 0 ? "None has been declared" : metadata.Capabilities));
            text.AppendLine("File Hash (SHA-256): " + metadata.Hash).AppendLine();
            if (nativeApproval || metadata.Runtime == "native") text.AppendLine("Runs inside the server with full access to it.").AppendLine();
            text.Append(question);
            return text.ToString();
        }

        internal sealed class PluginDanger
        {
            internal string Capability, Level, Text;
        }

        internal static Color PluginDangerColor(string level)
        {
            return level == "low" ? Color.DarkGreen : level == "high" ? Color.SaddleBrown : Color.DarkRed;
        }

        internal static bool CanApprovePlugin(string runtime, bool nativeConfirmed)
        {
            return runtime == "lua" || runtime == "wasm" || (runtime == "native" && nativeConfirmed);
        }

        internal static bool NeedsFullPluginReview(PluginMetadata next, PluginMetadata previous)
        {
            return previous == null || next.Id != previous.Id || next.Runtime != previous.Runtime ||
                !new HashSet<string>(next.Capabilities.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries),
                    StringComparer.Ordinal).SetEquals(previous.Capabilities.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries));
        }

        internal static PluginMetadata ParsePluginInspection(string output, bool package)
        {
            if (output == null || output.Length > 65536) return null;
            string[] lines = output.Replace("\r", "").Split(new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries);
            if (lines.Length < 3) return null;
            string[] manifest = lines[0].Split('\t'), identity = lines[1].Split('\t'), bundle = lines[lines.Length - 1].Split('\t');
            if (manifest.Length != 10 || manifest[0] != "MANIFEST" || identity.Length != 5 ||
                identity[0] != "INSPECT" || identity[1] != "1" || manifest[1] != identity[2] ||
                !Regex.IsMatch(identity[3], @"\A[0-9a-f]{64}\z") || !Regex.IsMatch(identity[4], @"\A[0-9a-f]{64}\z") ||
                bundle.Length != 2 || bundle[0] != "PACKAGE" ||
                (package ? !Regex.IsMatch(bundle[1], @"\A[0-9a-f]{64}\z") : bundle[1] != "")) return null;
            string suffix = string.Join("\t", manifest.Skip(4).ToArray());
            PluginMetadata metadata = ParsePluginRow("MODULE\t" + manifest[1] + "\t" + manifest[2] + "\t" +
                identity[3] + "\t" + (package ? bundle[1] : identity[3]) + "\t" + suffix, true);
            if (metadata == null) return null;
            metadata.PackageHash = bundle[1];
            metadata.ManifestHash = identity[4];
            var expected = new HashSet<string>(metadata.Capabilities.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries),
                StringComparer.Ordinal);
            if (metadata.Runtime == "native") expected.Add("native");
            var seen = new HashSet<string>(StringComparer.Ordinal);
            for (int i = 2; i < lines.Length - 1; i++) {
                string[] danger = lines[i].Split('\t');
                if (danger.Length != 4 || danger[0] != "DANGER" || !expected.Contains(danger[1]) ||
                    !seen.Add(danger[1]) || danger[3].Length == 0 || !PluginText(danger[3], 256)) return null;
                string level = danger[1] == "native" ? "highest" :
                    danger[1] == "read" || danger[1] == "storage" || danger[1] == "config" ? "low" : "high";
                if (danger[2] != level) return null;
                metadata.Dangers.Add(new PluginDanger { Capability = danger[1], Level = level, Text = danger[3] });
            }
            return expected.SetEquals(seen) ? metadata : null;
        }

        internal static PluginMetadata ParsePluginTrust(string output, string id)
        {
            string[] lines = output.Replace("\r", "").Split(new[] { '\n' }, StringSplitOptions.RemoveEmptyEntries);
            if (lines.Length != 1) throw new IOException(InvalidReviewText);
            string[] fields = lines[0].Split('\t');
            if (fields.Length == 2 && fields[0] == "TRUST_NONE" && fields[1] == id) return null;
            if (fields.Length != 10 || fields[0] != "TRUST" || fields[1] != "1" || fields[2] != id ||
                !Regex.IsMatch(fields[7], @"\A[0-9a-f]{64}\z") ||
                (fields[8].Length != 0 && !Regex.IsMatch(fields[8], @"\A[0-9a-f]{64}\z")) ||
                !Regex.IsMatch(fields[9], @"\A[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}Z\z"))
                throw new IOException(InvalidReviewText);
            PluginMetadata result = ParsePluginRow("MODULE\t" + id + "\t" + fields[3] + "\t" + fields[6] +
                "\t" + (fields[8].Length == 0 ? fields[6] : fields[8]) + "\t" + fields[4] + "\t" + fields[5] + "\t\t\t\t", true);
            if (result == null) throw new IOException(InvalidReviewText);
            result.ManifestHash = fields[7]; result.PackageHash = fields[8];
            return result;
        }

        internal sealed class PluginApprovalDialog : Form
        {
            internal readonly CheckBox NativeConfirmation;
            internal readonly Button ApproveButton;
            internal readonly bool FullReview;
            readonly PluginMetadata metadata;

            internal PluginApprovalDialog(PluginMetadata next, PluginMetadata previous, bool install)
            {
                metadata = next;
                FullReview = NeedsFullPluginReview(next, previous);
                Text = "Plugin Installation";
                StartPosition = FormStartPosition.CenterParent;
                Font = SystemFonts.MessageBoxFont;
                ClientSize = new Size(700, 580); MinimumSize = new Size(600, 480);
                MaximizeBox = false; MinimizeBox = false; ShowInTaskbar = false; KeyPreview = true;
                var table = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 1, RowCount = 6, Padding = new Padding(12) };
                table.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
                for (int i = 0; i < 6; i++) table.RowStyles.Add(new RowStyle(i == 3 ? SizeType.Percent : SizeType.AutoSize, i == 3 ? 100 : 0));
                table.Controls.Add(new Label { Text = next.DisplayName + " (" + next.Id + ")", AutoSize = true,
                    MaximumSize = new Size(650, 0), UseMnemonic = false, Margin = new Padding(3, 3, 3, 8) }, 0, 0);
                string details = (next.Description.Length == 0 ? "" : next.Description + "\r\n") + "Author: " + (next.Author.Length == 0 ? "Not Stated" : next.Author) +
                    "\r\nRuntime: " + next.Runtime;
                table.Controls.Add(new Label { Text = details, AutoSize = true, MaximumSize = new Size(650, 0), UseMnemonic = false, Margin = new Padding(3, 3, 3, 8) }, 0, 1);
                string version = previous == null ? "Version " + next.Version :
                    "Version " + previous.Version + " to " + next.Version;
                table.Controls.Add(new Label { Text = version + (FullReview ? ". The plugin has new capabilities." :
                    ". The plugin has no new capabilities."), AutoSize = true, MaximumSize = new Size(650, 0), UseMnemonic = false,
                    Margin = new Padding(3, 3, 3, 8) }, 0, 2);
                var review = new RichTextBox { ReadOnly = true, Dock = DockStyle.Fill, BackColor = SystemColors.Window,
                    BorderStyle = BorderStyle.FixedSingle, DetectUrls = false, TabIndex = 0, Name = "capabilityReview" };
                table.Controls.Add(review, 0, 3);
                NativeConfirmation = new CheckBox { Text = "Allow this plugin to run on my server",
                    AutoSize = true, Visible = next.Runtime == "native", Checked = false, ForeColor = Color.DarkRed,
                    Name = "nativeConfirmation", TabIndex = 1, Margin = new Padding(3, 12, 3, 12) };
                table.Controls.Add(NativeConfirmation, 0, 4);
                var actions = new FlowLayoutPanel { AutoSize = true, Dock = DockStyle.Fill, FlowDirection = FlowDirection.RightToLeft, TabIndex = 2 };
                ApproveButton = new Button { Text = "Install", AutoSize = true, TabIndex = 0, Name = "installAndApprove" };
                var cancel = new LinkLabel { Text = "Cancel", AutoSize = true, Margin = new Padding(12, 8, 12, 3), TabIndex = 1 };
                cancel.LinkClicked += delegate { DialogResult = DialogResult.Cancel; Close(); };
                ApproveButton.Click += delegate { TryApprove(); };
                NativeConfirmation.CheckedChanged += delegate { UpdateApprovalState(); };
                actions.Controls.Add(ApproveButton); actions.Controls.Add(cancel);
                table.Controls.Add(actions, 0, 5); Controls.Add(table);
                var oldCaps = new HashSet<string>(previous == null ? new string[0] : previous.Capabilities.Split(','));
                foreach (PluginDanger danger in next.Dangers.OrderByDescending(d => d.Level == "highest" ? 2 : d.Level == "high" ? 1 : 0)) {
                    review.SelectionColor = PluginDangerColor(danger.Level);
                    review.AppendText((previous != null && danger.Capability != "native" && !oldCaps.Contains(danger.Capability) ? "NEW: " : "") +
                        danger.Capability + " (" + danger.Level + "): " + danger.Text + "\r\n\r\n");
                }
                review.SelectionColor = SystemColors.ControlText;
                review.AppendText(
                    "SHA-256: " + (next.PackageHash.Length == 0 ? next.Hash : next.PackageHash));
                review.SelectionStart = 0; review.SelectionLength = 0;
                AcceptButton = ApproveButton;
                KeyDown += (sender, e) => { if (e.KeyCode == Keys.Escape) { DialogResult = DialogResult.Cancel; Close(); } };
                UpdateApprovalState();
            }

            void UpdateApprovalState()
            {
                ApproveButton.Enabled = CanApprovePlugin(metadata.Runtime, NativeConfirmation.Checked);
            }

            internal bool TryApprove()
            {
                if (!CanApprovePlugin(metadata.Runtime, NativeConfirmation.Checked)) return false;
                DialogResult = DialogResult.OK; Close(); return true;
            }
        }

        PluginMetadata ReadPreviousPluginReview(string serverFolder, string id)
        {
            try { return ParsePluginTrust(RunPluginTool(serverFolder, "--plugin-trust-status " + id), id); }
            catch (IOException) { return null; }
        }

        bool ReviewPluginApproval(PluginMetadata metadata, PluginMetadata previous, bool install)
        {
            using (var dialog = new PluginApprovalDialog(metadata, previous, install))
                return dialog.ShowDialog(this) == DialogResult.OK;
        }

        static string RunPluginTool(string serverFolder, string arguments, int timeout = 10000)
        {
            string executable = Path.Combine(serverFolder, "subvulpes_server.exe");
            var info = new FileInfo(executable);
            if (!info.Exists || info.Length > 64 * 1024 * 1024) throw new IOException("Server Executable is invalid.");
            string content = Encoding.ASCII.GetString(File.ReadAllBytes(executable));
            if (!content.Contains("SubVulpes-plugin-cli-v1"))
                throw new IOException("This server does not support plugin metadata requests. Update the server and launcher together.");
            var start = new ProcessStartInfo(executable, arguments) {
                WorkingDirectory = serverFolder, UseShellExecute = false, CreateNoWindow = true,
                RedirectStandardOutput = true, RedirectStandardError = true
            };
            using (var process = Process.Start(start)) {
                Task<string> output = process.StandardOutput.ReadToEndAsync();
                Task<string> error = process.StandardError.ReadToEndAsync();
                if (!process.WaitForExit(timeout)) {
                    process.Kill();
                    process.WaitForExit();
                    throw new IOException("Plugin metadata request timed out.");
                }
                Task.WaitAll(output, error);
                string text = output.Result + error.Result;
                if (process.ExitCode != 0) throw new IOException(text.Trim());
                return text;
            }
        }

        async Task RefreshPlugins()
        {
            if (pluginBusy || updating) return;
            if (server != null) {
                // The rows that come back replace the rows that exist. The list does not empty first.
                SendPluginCommand("plugins list");
                return;
            }
            pluginBusy = true;
            SetState(serverState);
            string selectedFolder = folder;
            try {
                string output = await Task.Run(() => RunPluginTool(selectedFolder, "--plugins-list"));
                if (selectedFolder != folder || server != null) return;
                var fresh = new List<PluginMetadata>();
                foreach (string line in output.Replace("\r", "").Split('\n')) {
                    PluginMetadata row = ParsePluginRow(line, false);
                    if (row == null) continue;
                    int same = fresh.FindIndex(p => p.Id == row.Id);
                    if (same < 0) fresh.Add(row); else fresh[same] = row;
                }
                ReplacePlugins(fresh);
            } catch (Exception ex) {
                message.Text = "Cannot list plugins: " + ex.Message;
            } finally { pluginBusy = false; SetState(serverState); }
        }

        void SendPluginCommand(string command)
        {
            if (updating) return;
            if (server == null || stopping) { message.Text = "Cannot send a command while the server is closed."; return; }
            if (command.Length == 0 || command.Length > 300 || command.IndexOfAny(new[] { '\r', '\n' }) >= 0) return;
            try {
                server.StandardInput.WriteLine(command);
                server.StandardInput.Flush();
                message.Text = "Command sent.";
            } catch (Exception ex) { message.Text = "Cannot send the command: " + ex.Message; }
        }

        async Task ChangePlugin(string id, bool enabled)
        {
            if (pluginBusy || updating) return;
            id = PluginActionId(id);
            if (id == null) return;
            if (server != null) {
                SendPluginCommand("plugins " + (enabled ? "enable " : "disable ") + id);
                return;
            }
            pluginBusy = true;
            SetState(serverState);
            string selectedFolder = folder;
            try {
                string output = await Task.Run(() => RunPluginTool(selectedFolder,
                    (enabled ? "--plugin-enable " : "--plugin-disable ") + id));
                message.Text = output.Trim();
            } catch (Exception ex) { message.Text = "Cannot change plugin state: " + ex.Message; }
            finally { pluginBusy = false; SetState(serverState); }
            await RefreshPlugins();
        }

        async Task ApprovePlugin(string id)
        {
            if (pluginBusy || updating) return;
            id = PluginActionId(id);
            if (id == null) return;
            if (server != null) { message.Text = "Stop the server before reviewing installed content."; return; }
            pluginBusy = true; SetState(serverState);
            string selectedFolder = folder;
            string outcome = null;
            try {
                message.Text = "Reading installed plugin content...";
                string output = await Task.Run(() => RunPluginTool(selectedFolder, "--plugin-inspect " + id));
                PluginMetadata metadata = ParsePluginInspection(output, false);
                if (metadata == null) throw new IOException(IncompleteReviewText);
                PluginMetadata previous = await Task.Run(() => ReadPreviousPluginReview(selectedFolder, id));
                if (server != null || folder != selectedFolder || !ReviewPluginApproval(metadata, previous, false)) return;
                string arguments = "--plugin-trust " + id + " " + metadata.Hash + " " + metadata.ManifestHash + " " +
                    metadata.Runtime + " " + (metadata.Capabilities.Length == 0 ? "-" : metadata.Capabilities) + " - --native-confirmed";
                await Task.Run(() => RunPluginTool(selectedFolder, arguments));
                message.Text = outcome = metadata.DisplayName + " " + metadata.Version + " has been approved.";
            } catch (Exception ex) { message.Text = outcome = "Plugin approval failed: " + ex.Message; }
            finally { pluginBusy = false; SetState(serverState); }
            await RefreshPlugins();
            if (outcome != null) message.Text = outcome;
        }

        internal static string QuoteProcessArgument(string value)
        {
            var result = new StringBuilder("\"");
            int slashes = 0;
            foreach (char character in value) {
                if (character == '\\') { slashes++; continue; }
                if (character == '"') result.Append('\\', slashes * 2 + 1);
                else result.Append('\\', slashes);
                result.Append(character);
                slashes = 0;
            }
            return result.Append('\\', slashes * 2).Append('"').ToString();
        }

        internal static string PluginApprovalArguments(PluginMetadata metadata, bool nativeConfirmed)
        {
            if (metadata == null || metadata.Runtime != "native" || !CanApprovePlugin(metadata.Runtime, nativeConfirmed) ||
                !Regex.IsMatch(metadata.Hash ?? "", @"\A[0-9a-f]{64}\z") ||
                !Regex.IsMatch(metadata.ManifestHash ?? "", @"\A[0-9a-f]{64}\z") ||
                !Regex.IsMatch(metadata.PackageHash ?? "", @"\A[0-9a-f]{64}\z"))
                throw new IOException("The review is incomplete.");
            return " -Approve -ExpectedPayloadHash " + metadata.Hash + " -ExpectedManifestHash " + metadata.ManifestHash +
                " -ExpectedRuntime " + metadata.Runtime + " -ExpectedCapabilities " +
                QuoteProcessArgument(metadata.Capabilities.Length == 0 ? "-" : metadata.Capabilities) + " -NativeConfirmed";
        }

        static string RunPluginInstaller(string serverFolder, string script, string zip, bool inspect, string packageHash = null, PluginMetadata approval = null)
        {
            string arguments = "-NoProfile -ExecutionPolicy Bypass -File " + QuoteProcessArgument(script) +
                " -ServerDir " + QuoteProcessArgument(serverFolder) + " -Package " + QuoteProcessArgument(zip) +
                (inspect ? " -Inspect" : "") + (packageHash == null ? "" : " -ExpectedPackageHash " + packageHash);
            if (approval != null) arguments += PluginApprovalArguments(approval, true);
            var start = new ProcessStartInfo("powershell.exe", arguments) {
                WorkingDirectory = serverFolder, UseShellExecute = false, CreateNoWindow = true,
                RedirectStandardOutput = true, RedirectStandardError = true
            };
            using (var process = Process.Start(start)) {
                Task<string> output = process.StandardOutput.ReadToEndAsync();
                Task<string> error = process.StandardError.ReadToEndAsync();
                // An automatic kill during file replacement would bypass installer rollback.
                process.WaitForExit();
                Task.WaitAll(output, error);
                string text = output.Result + error.Result;
                if (process.ExitCode != 0) throw new IOException(text.Trim());
                return text;
            }
        }

        // The package comes from the catalog, or from a file that the user chooses.
        // A package from the Workshop has the hash of the signed catalog (expectedHash). It must match what the server reads.
        // The source (Official or Community) tells what the user sees. An official package has no review window.
        // A community package shows a short card, and the full window when an update brings new capabilities.
        async Task InstallPlugin(string given = null, string expectedHash = null, string source = null)
        {
            if (pluginBusy || updating) return;
            string zip = given;
            if (zip == null)
                using (var dialog = new OpenFileDialog { Filter = "Plugin package (*.svplugin;*.zip)|*.svplugin;*.zip" }) {
                    if (dialog.ShowDialog(this) != DialogResult.OK) return;
                    zip = dialog.FileName;
                }
            string selectedFolder = folder;
            string outcome = null;
            string script = Path.Combine(selectedFolder, "install_plugin.ps1");
            if (!File.Exists(script)) { message.Text = "The server package has no plugin installer."; return; }
            pluginBusy = true; SetState(serverState);
            try {
                message.Text = "Reading plugin package...";
                string inspection = await Task.Run(() => RunPluginTool(selectedFolder, "--plugin-inspect " + QuoteProcessArgument(zip), 45000));
                PluginMetadata metadata = ParsePluginInspection(inspection, true);
                if (metadata == null) throw new IOException(IncompleteReviewText);
                if (expectedHash != null && metadata.PackageHash != expectedHash) throw new IOException("The package does not match the signed catalog.");
                PluginMetadata previous = await Task.Run(() => ReadPreviousPluginReview(selectedFolder, metadata.Id));
                if (selectedFolder != folder) return;
                bool fromWorkshop = expectedHash != null;
                if (!(fromWorkshop && source == "Official")) {
                    bool brief = fromWorkshop && source == "Community" && (previous == null || !NeedsFullPluginReview(metadata, previous));
                    if (!(brief ? ReviewCommunityCard(metadata) : ReviewPluginApproval(metadata, previous, true))) return;
                }
                if (!await StopServer() || selectedFolder != folder) return;
                message.Text = "Installing " + metadata.DisplayName + "...";
                await Task.Run(() => RunPluginInstaller(selectedFolder, script, zip, false, metadata.PackageHash, metadata));
                message.Text = outcome = metadata.DisplayName + " " + metadata.Version + " is installed and approved.";
            } catch (Exception ex) { message.Text = outcome = "Plugin installation failed: " + ex.Message; }
            finally { pluginBusy = false; SetState(serverState); }
            await RefreshPlugins();
            if (outcome != null) message.Text = outcome;
        }

        // ------------------------------------------------------------ the server

        internal void StartServer()
        {
            if (server != null || updating || pluginBusy) return;
            // An install that waited for the next start runs first. Then the server starts.
            if (pendingInstalls.Count > 0) { var apply = ApplyPendingInstalls(); return; }
            developerRun = debugModeBox.On;
            activeCaptures = developerRun ? captures.Copy() : new Captures { CrashDump = "none" };
            string exe = Path.Combine(folder, "subvulpes_server.exe");
            if (!Directory.Exists(Path.Combine(folder, "data"))) {
                ShowMissing(false);
                return;
            }
            if (!File.Exists(exe)) {
                message.Text = Path.GetFileName(exe) + " is not in this folder.";
                return;
            }
            if (MissingAdminPassword(passwordBox.Text)) {
                message.Text = "Set an admin password in Server Settings before you start the server.";
                dirtyText.Text = message.Text;
                dirtyText.ForeColor = UI.T.Bad;
                ShowPage(SettingsPageName);
                ShowSub(0);
                passwordBox.Box.Focus();
                return;
            }
            try {
                if (dirty) SaveSettings();
                if (developerRun) SaveCaptureChoice();
                string stamp = DateTime.Now.ToString("yyyyMMdd-HHmmss-fff");
                testDir = developerRun ? Path.Combine(logsRoot, stamp) : logsRoot;
                Directory.CreateDirectory(testDir);
                logFile = Path.Combine(testDir, "server.log");
                string eventName = @"Local\SubVulpes-stop-" + Guid.NewGuid().ToString("N");
                stopEvent = new System.Threading.EventWaitHandle(false, System.Threading.EventResetMode.ManualReset, eventName);
                var start = new ProcessStartInfo(exe) {
                    WorkingDirectory = folder, UseShellExecute = false, CreateNoWindow = true,
                    RedirectStandardOutput = true, RedirectStandardError = true, RedirectStandardInput = true,
                    Arguments = nativeBypass.On ? "--allow-unapproved-native" : ""
                };
                var env = start.EnvironmentVariables;
                foreach (string key in new[] { "SV_SAMPLE", "SV_TIMERS", "SV_PACKETS", "SV_DUMP", "SV_DUMP_TYPE", "SV_TEST_CRASH", "SV_RECORD_DIR", "SV_SNAPSHOT_AFTER" })
                    env.Remove(key);
                env["SV_LOG"] = logFile;
                env["SV_STOP_EVENT"] = eventName;
                if (activeCaptures.Profile) env["SV_SAMPLE"] = Path.Combine(testDir, "profile.txt");
                if (activeCaptures.Timers) env["SV_TIMERS"] = "1";
                if (activeCaptures.Packets) env["SV_PACKETS"] = Path.Combine(testDir, "packets.bin");
                if (activeCaptures.CrashDump != "none") {
                    env["SV_DUMP"] = Path.Combine(testDir, "crash.dmp");
                    env["SV_DUMP_TYPE"] = activeCaptures.CrashDump;
                }
                string testCrash = developerRun ? settings.Get("test_crash", "") : "";
                if (testCrash != "") env["SV_TEST_CRASH"] = testCrash;
                if (developerRun) {
                    var session = new KeyFile(Path.Combine(testDir, "session.ini"));
                    session.Set("preset", ((Preset)presetBox.SelectedItem).Name);
                    session.Set("captures", activeCaptures.Summary());
                    session.Set("start", DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
                    session.Set("server", ServerVersion());
                    session.Save();
                }
                server = new Process { StartInfo = start, EnableRaisingEvents = true };
                serverStopped = new TaskCompletionSource<bool>();
                server.ErrorDataReceived += OnOutput;
                server.Exited += delegate { if (!IsDisposed) BeginInvoke(new Action(OnExited)); };
                stopping = crashed = frozen = false;
                maxPlayers = 0;
                speeds.Clear();
                ClearConsole();
                ClearPlugins();
                ResetStats();
                logOffset = 0;
                pendingLog = "";
                // Remove the previous session before the server opens SV_LOG.
                if (File.Exists(logFile)) File.Delete(logFile);
                server.Start();
                runValues = CurrentRestartValues();
                UpdateRestartBadges();
                server.BeginOutputReadLine();
                server.BeginErrorReadLine();
                started = lastSpeedLine = DateTime.Now;
                stats.Set("Started at", started.ToString("HH:mm"));
                SetState("Starting");
                message.Text = developerRun ? "This test: logs\\" + stamp : "";
            } catch (Exception ex) {
                if (server != null) { server.Dispose(); server = null; }
                if (stopEvent != null) { stopEvent.Dispose(); stopEvent = null; }
                SetState("Stopped");
                message.Text = "Cannot start the server: " + ex.Message;
            }
        }

        string ServerVersion()
        {
            var info = new FileInfo(Path.Combine(folder, "subvulpes_server.exe"));
            return info.Exists ? info.LastWriteTime.ToString("yyyy-MM-dd HH:mm") + ", " + info.Length + " bytes" : "?";
        }

        async Task<bool> StopServer()
        {
            if (server == null) return true;
            if (!stopping) {
                stopping = true;
                stopEvent.Set();
                SetState("Stopping");
            }
            await Task.WhenAny(serverStopped.Task, Task.Delay(15000));
            if (server == null) return true;
            stopping = false;
            SetState("Stop has timed out");
            message.Text = "The server did not stop within 15 seconds. Close it from Task Manager if it stays open.";
            return false;
        }

        void OnOutput(object sender, DataReceivedEventArgs e)
        {
            if (e.Data != null && !IsDisposed)
                BeginInvoke(new Action<string>(AddLine), e.Data);
        }

        void ReadLog()
        {
            if (server == null || !File.Exists(logFile)) return;
            try {
                using (var stream = new FileStream(logFile, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete)) {
                    if (stream.Length < logOffset) { logOffset = 0; pendingLog = ""; }
                    stream.Position = logOffset;
                    using (var reader = new StreamReader(stream, Encoding.Default)) {
                        pendingLog += reader.ReadToEnd();
                        logOffset = stream.Position;
                    }
                }
                int end;
                while ((end = pendingLog.IndexOf('\n')) >= 0) {
                    AddLine(pendingLog.Substring(0, end).TrimEnd('\r'));
                    pendingLog = pendingLog.Substring(end + 1);
                }
            } catch (IOException ex) {
                message.Text = "Cannot read server.log: " + ex.Message;
            }
        }


        void AddLine(string line)
        {
            ReadPluginLine(line);
            AddConsoleLine(line);

            Match m = TicksLine.Match(line);
            if (m.Success) {
                double ticks = double.Parse(m.Groups[1].Value, CultureInfo.InvariantCulture);
                speeds.Add(ticks);
                lastSpeedLine = DateTime.Now;
                frozen = false;
                speedStatus = m.Groups[1].Value + " ticks/s";
                if (m.Groups[2].Success) {
                    int players = int.Parse(m.Groups[2].Value);
                    maxPlayers = Math.Max(maxPlayers, players);
                    playersStatus = players + (players == 1 ? " player" : " players") +
                                         (m.Groups[3].Value != "0" ? ", " + m.Groups[3].Value + " bots" : "");
                }
                ShowSpeed(ticks, m.Groups[2].Success ? int.Parse(m.Groups[2].Value) : -1, m.Groups[3].Value);
                if (serverState == "Running" || serverState == "Frozen?")
                    SetState("Running");
            } else if (Regex.IsMatch(line, @"^Server started on port \d+\.$")) {
                lastSpeedLine = DateTime.Now;
                SetState("Running");

            } else if (line.Contains("CRASH")) {
                crashed = true;
                settings.Set("last_crash", DateTime.Now.ToString("yyyy-MM-dd HH:mm"));
                settings.Save();
                stats.Set("Last crash", DateTime.Now.ToString("yyyy-MM-dd HH:mm"));
            }
            RefreshStatus();
        }

        void OnClock()
        {
            if (server == null) return;
            ReadLog();
            TimeSpan t = DateTime.Now - started;
            stats.Set("Uptime", ((int)t.TotalHours).ToString("00") + ":" + t.Minutes.ToString("00") + ":" + t.Seconds.ToString("00"));
            if (server == null || frozen || (DateTime.Now - lastSpeedLine).TotalSeconds < FreezeSeconds)
                return;
            // The status line comes each minute. No line for a long time: the main loop stops.
            frozen = true;
            SetState("Frozen?");
            AddLine("[launcher] No speed status for " + FreezeSeconds + " s. The server is probably frozen.");
            if (activeCaptures.FreezeDump)
                DumpNow("freeze");
        }

        void OnExited()
        {
            if (server == null) return;
            ReadLog();
            int code = server.ExitCode;
            server.Dispose();
            server = null;
            if (stopEvent != null) { stopEvent.Dispose(); stopEvent = null; }
            serverStopped.TrySetResult(true);
            string result;
            if (stopping) {
                AddLine("[launcher] Server stopped by the user.");
                SetState("Stopped");
                result = frozen ? "frozen" : "ok";
            } else {
                AddLine("[launcher] Server stopped unexpectedly. Exit Code: " + code + ".");
                SetState(crashed ? "Crashed" : StoppedUnexpectedly);
                result = crashed ? "crash" : StoppedUnexpectedly;
            }
            if (!developerRun) return;
            TimeSpan t = DateTime.Now - started;
            var session = new KeyFile(Path.Combine(testDir, "session.ini"));
            session.Set("end", DateTime.Now.ToString("yyyy-MM-dd HH:mm:ss"));
            session.Set("minutes", ((int)t.TotalMinutes).ToString());
            session.Set("result", result);
            session.Set("max_players", maxPlayers.ToString());
            if (speeds.Count > 0) {
                session.Set("speed_min", speeds.Min().ToString("0.0", CultureInfo.InvariantCulture));
                session.Set("speed_avg", speeds.Average().ToString("0.0", CultureInfo.InvariantCulture));
            }
            session.Save();

            AddLine("[launcher] Log saved: " + (int)t.TotalMinutes + " min, " + result + ". It is in Launcher Settings, Past Logs.");
            LoadTests();
        }

        // ------------------------------------------------------------ memory dumps

        [DllImport("dbghelp.dll", SetLastError = true)]
        static extern bool MiniDumpWriteDump(IntPtr process, int processId, Microsoft.Win32.SafeHandles.SafeFileHandle file,
                                             int type, IntPtr exception, IntPtr userStream, IntPtr callback);

        // MiniDumpWithDataSegs | IndirectlyReferencedMemory | UnloadedModules | ThreadInfo, or
        // WithFullMemory | HandleData | UnloadedModules | ThreadInfo (dbghelp.h).
        const int SmallDump = 0x1 | 0x40 | 0x20 | 0x1000, FullDump = 0x2 | 0x4 | 0x20 | 0x1000;

        void DumpNow(string kind)
        {
            if (server == null)
                return;
            bool full = activeCaptures.CrashDump == "full";
            string file = Path.Combine(testDir, kind + "-" + DateTime.Now.ToString("HHmmss") + ".dmp");
            IntPtr handle = server.Handle;
            int pid = server.Id;
            AddLine("[launcher] Writing a " + (full ? "full" : "small") + " dump: " + Path.GetFileName(file));
            Task.Run(() => {
                bool ok;
                using (var f = new FileStream(file, FileMode.Create, FileAccess.Write))
                    ok = MiniDumpWriteDump(handle, pid, f.SafeFileHandle, full ? FullDump : SmallDump, IntPtr.Zero, IntPtr.Zero, IntPtr.Zero);
                string size = (new FileInfo(file).Length / 1048576.0).ToString("0.0", CultureInfo.InvariantCulture) + " MB";
                BeginInvoke(new Action<string>(AddLine), ok ? "[launcher] Memory Dump ready (" + size + ")." : "[launcher] The Memory Dump failed.");
            });
        }

        // ------------------------------------------------------------ past tests

        static List<string> Files(string dir)
        {
            return Directory.Exists(dir) ? Directory.GetFiles(dir).ToList() : new List<string>();
        }

        static string FolderSize(string dir)
        {
            long bytes = Files(dir).Sum(f => new FileInfo(f).Length);
            return (bytes / 1048576.0).ToString("0.0", CultureInfo.InvariantCulture) + " MB";
        }

        // One zip for each test, next to the test folder. The server can still write its files:
        // read each one with shared access.
        void PackTest(string dir)
        {
            if (dir == null || Files(dir).Count == 0) {
                MessageBox.Show(this, "This log is empty.", Text);
                return;
            }
            string zip = Path.Combine(logsRoot, "test-" + Path.GetFileName(dir) + ".zip");
            message.Text = "Packing " + Path.GetFileName(zip) + "...";
            Task.Run(() => {
                string error = null;
                try {
                    if (File.Exists(zip))
                        File.Delete(zip);
                    using (var archive = ZipFile.Open(zip, ZipArchiveMode.Create))
                        foreach (string file in Files(dir)) {
                            var entry = archive.CreateEntry(Path.GetFileName(dir) + "/" + Path.GetFileName(file), CompressionLevel.Optimal);
                            using (var src = new FileStream(file, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
                            using (var dst = entry.Open())
                                src.CopyTo(dst);
                        }
                } catch (IOException ex) {
                    error = ex.Message;
                }
                BeginInvoke(new Action(() => {
                    if (error != null) {
                        message.Text = "";
                        MessageBox.Show(this, "Cannot make the zip: " + error, Text);
                        return;
                    }
                    message.Text = "Saved: logs\\" + Path.GetFileName(zip) + " (" +
                                  (new FileInfo(zip).Length / 1048576.0).ToString("0.0", CultureInfo.InvariantCulture) + " MB)";
                    Process.Start("explorer.exe", "/select,\"" + zip + "\"");
                }));
            });
        }

        // ------------------------------------------------------------ update

        // A password of 16 letters and digits. The set has no 0, O, 1, l or I.
        public static string NewAdminPassword()
        {
            const string set = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789";
            var chars = new char[16];
            var bytes = new byte[1];
            using (var random = new RNGCryptoServiceProvider()) {
                for (int i = 0; i < chars.Length; ) {
                    random.GetBytes(bytes);
                    // Skip the top bytes so that every character has the same chance.
                    if (bytes[0] < 256 - 256 % set.Length)
                        chars[i++] = set[bytes[0] % set.Length];
                }
            }
            return new string(chars);
        }

        public static bool MissingAdminPassword(string password)
        {
            return string.IsNullOrWhiteSpace(password);
        }

        public static string RedactLog(string text, string password)
        {
            text = Regex.Replace(text, @"/admin [^\r\n]*", "/admin [redacted]");
            return password.Length > 0 ? text.Replace(password, "[redacted]") : text;
        }

        public static void CopyLog(string source, string destination, string password)
        {
            if (Path.GetFullPath(source).Equals(Path.GetFullPath(destination), StringComparison.OrdinalIgnoreCase))
                throw new IOException("Choose another file for the saved log.");
            using (var input = new FileStream(source, FileMode.Open, FileAccess.Read, FileShare.ReadWrite))
            using (var reader = new StreamReader(input, Encoding.Default))
            using (var output = new FileStream(destination, FileMode.Create, FileAccess.Write))
            using (var writer = new StreamWriter(output, new UTF8Encoding(false)))
                writer.Write(RedactLog(reader.ReadToEnd(), password));
        }

        async Task SaveLogTo(string destination)
        {
            string source = logFile;
            try {
                string password = new KeyFile(Path.Combine(folder, "config.txt")).Get("serverpassword", "");
                await Task.Run(() => CopyLog(source, destination, password));
                message.Text = "Saved: " + destination;
            } catch (Exception ex) {
                message.Text = "Cannot save the log: " + ex.Message;
            }
        }

        async Task SaveLog()
        {
            if (!File.Exists(logFile)) { message.Text = "server.log is missing."; return; }
            using (var dialog = new SaveFileDialog { Filter = "Log files (*.log)|*.log|All files (*.*)|*.*", FileName = "server.log", DefaultExt = "log" })
                if (dialog.ShowDialog(this) == DialogResult.OK)
                    await SaveLogTo(dialog.FileName);
        }

        async Task CheckForUpdates()
        {
            if (updating || pluginBusy) return;
            updating = true;
            checkUpdate.Enabled = false;
            SetState(serverState);
            string zip = null, releaseName = null;
            try {
                string file = Path.Combine(folder, "version.txt");
                if (!File.Exists(file)) throw new InvalidDataException("version.txt is missing.");
                string version = File.ReadLines(file).FirstOrDefault();
                bool previews = previewsBox.On;
                var release = await Task.Run(() => ReleaseUpdate.Find((version ?? "").Trim(), previews));
                if (release == null) {
                    message.Text = "No newer release.";
                    return;
                }
                releaseName = release.DisplayName;
                message.Text = releaseName;
                string directory = Path.Combine(logsRoot, "updates", Guid.NewGuid().ToString("N"));
                zip = await Task.Run(() => ReleaseUpdate.Download(release, directory));
            } catch (WebException) {
                message.Text = "Cannot check for updates. GitHub is unreachable or your internet is down.";
            } catch (Exception ex) {
                message.Text = "Cannot update: " + ex.Message;
            } finally {
                updating = false;
                checkUpdate.Enabled = true;
                SetState(serverState);
            }
            if (zip != null) await InstallUpdate(zip, releaseName);
        }

        static string ZipOf(DragEventArgs e)
        {
            var files = e.Data.GetData(DataFormats.FileDrop) as string[];
            return files != null && files.Length == 1 && files[0].EndsWith(".zip", StringComparison.OrdinalIgnoreCase) ? files[0] : null;
        }

        async Task ChooseUpdate()
        {
            using (var dialog = new OpenFileDialog { Filter = "Update zip (*.zip)|*.zip", Title = "Pick the update zip file!" })
                if (dialog.ShowDialog(this) == DialogResult.OK)
                    await InstallUpdate(dialog.FileName);
        }

        // Only fixed public files from a flat zip or one package root can enter an update. The old
        // files go to backup\<time>. config.txt, data and user never change. The running
        // launcher cannot be overwritten, but Windows lets it be renamed: it becomes
        // subvulpes_launcher.old, and the next start deletes it.
        public async Task InstallUpdate(string zip, string releaseName = null)
        {
            if (updating || pluginBusy) return;
            var found = new Dictionary<string, ZipArchiveEntry>(StringComparer.OrdinalIgnoreCase);
            ZipArchive archive;
            try {
                archive = ZipFile.OpenRead(zip);
            } catch (Exception ex) {
                MessageBox.Show(this, "Cannot read the zip: " + ex.Message, Text);
                return;
            }
            using (archive) {
                string archiveRoot = null;
                foreach (var entry in archive.Entries) {
                    string key = UpdateKey(entry.FullName);
                    if (key == null) continue;
                    string root = entry.FullName.Substring(0, entry.FullName.Length - key.Length);
                    if (archiveRoot != null && !root.Equals(archiveRoot, StringComparison.OrdinalIgnoreCase)) {
                        message.Text = "The zip file has more than one update folder. Use a zip from a SubVulpes release.";
                        return;
                    }
                    archiveRoot = root;
                    if (found.ContainsKey(key) || entry.Length > 64 * 1024 * 1024) {
                        message.Text = "The zip contains duplicate files.";
                        return;
                    }
                    found[key] = entry;
                }
                if (found.Count == 0) {
                    MessageBox.Show(this, "Hey, this is probably not an update .zip ???(" + string.Join(", ", UpdateFiles) + ").", Text);
                    return;
                }
                string version = "";
                if (found.ContainsKey("version.txt"))
                    using (var r = new StreamReader(found["version.txt"].Open()))
                        version = " " + r.ReadLine();
                string question = "Install " + (releaseName ?? version.Trim()) + "?";
                if (MessageBox.Show(this, question, Text, MessageBoxButtons.YesNo) != DialogResult.Yes)
                    return;
                updating = true;
                SetState(serverState);
                string backup = null;
                bool newLauncher = false;
                try {
                    if (!await StopServer()) return;
                    backup = Path.Combine(folder, "backup", DateTime.Now.ToString("yyyyMMdd-HHmmss"));
                    CreateUpdateDirectory(backup);
                    foreach (var f in found) {
                        string target = f.Key.Equals("subvulpes_launcher.exe", StringComparison.OrdinalIgnoreCase)
                            ? Application.ExecutablePath : Path.Combine(folder, f.Key);
                        if (f.Key == "plugins/plugins.ini" && File.Exists(target)) continue;
                        AssertUpdatePath(target);
                        string backupFile = Path.Combine(backup, f.Key);
                        AssertUpdatePath(backupFile);
                        Directory.CreateDirectory(Path.GetDirectoryName(target));
                        Directory.CreateDirectory(Path.GetDirectoryName(backupFile));
                        if (File.Exists(target)) File.Copy(target, backupFile, true);
                        if (f.Key.Equals("subvulpes_launcher.exe", StringComparison.OrdinalIgnoreCase)) {
                            string old = Path.Combine(launcherFolder, "subvulpes_launcher.old");
                            if (File.Exists(old))
                                File.Delete(old);
                            File.Move(Application.ExecutablePath, old);
                            newLauncher = true;
                        }
                        f.Value.ExtractToFile(target, true);
                    }
                    Text = AppTitle + Version();
                    message.Text = UpdateInstalledText + version + ". Old files: backup\\" + Path.GetFileName(backup);
                } catch (Exception ex) {
                    MessageBox.Show(this, "Update stopped: " + ex.Message +
                        (backup == null ? "" : "\r\n\r\nBackup path: " + backup + "."), Text);
                    return;
                } finally {
                    updating = false;
                    SetState(serverState);
                }
                if (newLauncher) {
                    MessageBox.Show(this, UpdateInstalledText + version + ". The launcher will now restart.", Text);
                    try {
                        Process.Start(Application.ExecutablePath);
                        Close();
                    } catch (Exception ex) { message.Text = "Cannot restart the launcher: " + ex.Message; }
                    return;
                }
            }
        }

        void CleanOldLauncher()
        {
            string old = Path.Combine(launcherFolder, "subvulpes_launcher.old");
            try {
                if (File.Exists(old))
                    File.Delete(old);
            } catch (IOException) {
                // The old launcher still closes. The next start deletes it.
            } catch (UnauthorizedAccessException) {
            }
        }

        // ------------------------------------------------------------ settings

        // ------------------------------------------------------------ close

        async void OnClosing(object sender, FormClosingEventArgs e)
        {
            if (updating || pluginBusy) { e.Cancel = true; return; }
            SaveDashboard();
            if (closeAfterStop) return;
            if (dirty) {
                var answer = MessageBox.Show(this, "Hey, there are unsaved settings. Do you want to save them before closing?", Text, MessageBoxButtons.YesNoCancel);
                if (answer == DialogResult.Cancel) { e.Cancel = true; return; }
                if (answer == DialogResult.Yes) SaveSettings();
            }
            if (server == null) return;
            e.Cancel = true;
            if (await StopServer()) {
                closeAfterStop = true;
                Close();
            }
        }

        protected override bool ProcessCmdKey(ref Message message, Keys keyData)
        {
            if (keyData == (Keys.Control | Keys.Shift | Keys.D)) {
                SetDebug(!debugModeBox.On);
                ShowPage(SettingsPageName);
                ShowSub(debugModeBox.On ? 3 : 1);

                return true;
            }
            if (keyData == Keys.F5) { var read = RefreshPlugins(); return true; }
            if (keyData == (Keys.Control | Keys.L)) { ClearConsole(); return true; }
            if (keyData == (Keys.Control | Keys.F)) { ShowPage("Server"); consoleSearch.Box.Focus(); return true; }
            if (keyData == (Keys.Control | Keys.S) && dirty) { SaveSettings(); return true; }
            return base.ProcessCmdKey(ref message, keyData);
        }

        [DllImport("user32.dll")]
        static extern bool SetProcessDPIAware();

        [STAThread]
        static void Main(string[] args)
        {
            SetProcessDPIAware();
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault(false);
            var form = new LauncherForm(AppDomain.CurrentDomain.BaseDirectory.TrimEnd('\\'));
            // A zip dropped on the exe icon: install it when the window shows.
            if (args.Length == 1 && args[0].EndsWith(".zip", StringComparison.OrdinalIgnoreCase))
                form.Shown += async delegate { await form.InstallUpdate(args[0]); };
            // For a test of the real server: start it when the window shows.
            if (Array.IndexOf(args, "--start") >= 0) form.Shown += delegate { form.StartServer(); };
            int at = Array.IndexOf(args, "--page");
            if (at >= 0 && at + 1 < args.Length) form.Shown += delegate { form.OpenPage(args[at + 1]); };
            Application.Run(form);
        }
    }
}
