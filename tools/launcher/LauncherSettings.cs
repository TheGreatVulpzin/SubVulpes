// The Settings page: Server Settings, Launcher Settings and Advanced Settings, with the Save bar and the list of past logs.
using System;
using System.Collections.Generic;

using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using SubVulpes.Ui;

namespace SubVulpes
{
    partial class LauncherForm
    {
        TableLayoutPanel settingsHost;
        Control settingsBarPanel;
        readonly List<Control> subPages = new List<Control>();
        readonly List<TabLabel> subTabs = new List<TabLabel>();

        // Server settings
        TextField nameBox, passwordBox, welcomeBox;
        Dropdown modeBox;
        NumberField playersBox, roundBox, portBox, bytesBox;
        Label dirtyText;
        FlatButton saveButton, undoButton;
        bool loading, dirty;

        // Launcher settings
        TextField folderBox;
        FlatButton browseButton, checkUpdate;
        Switch previewsBox, debugModeBox;
        Dropdown themeBox, accentBox;

        // Advanced settings
        Dropdown presetBox;
        Label presetText;
        Switch profileBox, timersBox, packetsBox, freezeBox;
        Segmented dumpType;

        // Past logs
        DataGridView testsList;
        TextField testNotes;

        static readonly Color[][] Accents = {
            new[] { Color.FromArgb(217, 99, 30), Color.FromArgb(240, 138, 75) }, new[] { Color.FromArgb(37, 99, 235), Color.FromArgb(96, 165, 250) },
            new[] { Color.FromArgb(22, 163, 74), Color.FromArgb(74, 222, 128) }, new[] { Color.FromArgb(219, 39, 119), Color.FromArgb(244, 114, 182) },
            new[] { Color.FromArgb(112, 82, 196), Color.FromArgb(176, 158, 240) } };
        static readonly string[] AccentNames = { "Orange", "Blue", "Green", "Pink", "Violet" };

        // ------------------------------------------------------------ the page

        Control SettingsHost()
        {
            var host = new TableLayoutPanel { ColumnCount = 1, RowCount = 3, Dock = DockStyle.Fill };
            settingsHost = host;
            host.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            host.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
            host.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            host.RowStyles.Add(new RowStyle(SizeType.Absolute, 58));
            var strip = new Panel { Dock = DockStyle.Fill, Padding = new Padding(8, 0, 8, 0), Tag = "subbar", Margin = new Padding(0) };
            string[] names = { "Server Settings", "Launcher Settings", "Past Logs", "Advanced Settings" };
            settingsBarPanel = SettingsBar();
            var area = new Panel { Dock = DockStyle.Fill, Padding = new Padding(16, 8, 16, 10) };
            Control[] subs = { ServerSettingsPage(), LauncherSettingsPage(), TestsPage(), AdvancedSettingsPage() };
            for (int i = names.Length - 1; i >= 0; i--) {
                var tab = new TabLabel(names[i]) { Selected = i == 0, Sub = true };
                int index = i;
                tab.Click += delegate { ShowSub(index); };
                subTabs.Insert(0, tab);
                strip.Controls.Add(tab);
                subs[i].Dock = DockStyle.Fill;
                subs[i].Visible = i == 0;
                area.Controls.Add(subs[i]);
                subPages.Insert(0, subs[i]);
            }
            host.Controls.Add(strip, 0, 0);
            host.Controls.Add(area, 0, 1);
            host.Controls.Add(settingsBarPanel, 0, 2);
            return host;
        }

        void ShowSub(int index)
        {
            for (int k = 0; k < subPages.Count; k++) {
                subPages[k].Visible = k == index;
                subTabs[k].Selected = k == index;
                subTabs[k].Invalidate();
            }
            // Only the server settings wait for the Save button. The other pages apply each change at once.
            settingsBarPanel.Visible = index == 0;
            settingsHost.RowStyles[2].Height = index == 0 ? 58 : 0;
            if (index == 2) LoadTests();
        }

        void SetDebugMode(bool enabled)
        {
            subTabs[3].Visible = enabled;
            if (!enabled && subPages[3].Visible) ShowSub(1);
        }

        void SetDebug(bool enabled)
        {
            debugModeBox.On = enabled;
            SetDebugMode(enabled);
            settings.Set("developer", enabled ? "1" : "0");
            settings.Save();
        }

        // ------------------------------------------------------------ rows

        static Label L(string text, bool bold, bool muted)
        {
            return new Label { Text = text, AutoSize = true, Font = UI.F(bold ? 10f : 9.5f, bold ? FontStyle.Bold : FontStyle.Regular), Tag = muted ? "muted" : null, Margin = new Padding(0) };
        }

        static TableLayoutPanel RowTable()
        {
            var t = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 4 };
            t.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 260));
            t.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 16));
            t.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            t.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 20));
            return t;
        }

        static void Compact(Control c)
        {
            var button = c as FlatButton;
            if (button != null) { button.Height = 28; return; }
            foreach (Control k in c.Controls) Compact(k);
        }

        static FlowLayoutPanel Inline(params Control[] items)
        {
            var f = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0) };
            foreach (var c in items) f.Controls.Add(c);
            return f;
        }

        static TextField Field(int width)
        {
            return new TextField(width, false);
        }

        static NumberField Number(int width, decimal minimum, decimal maximum)
        {
            return new NumberField(width) { Minimum = minimum, Maximum = maximum };
        }

        static Dropdown Combo(int width, params object[] items)
        {
            var c = new Dropdown(width);
            c.Items.AddRange(items);
            return c;
        }

        void Section(TableLayoutPanel t, string title)
        {
            int row = t.RowCount++;
            t.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));
            var p = new Panel { Dock = DockStyle.Fill, Margin = new Padding(0) };
            var l = L(title, true, false);
            l.Font = UI.F(11f, FontStyle.Bold);
            l.Location = new Point(0, 14);
            var line = new Panel { Dock = DockStyle.Bottom, Height = 1, Tag = "line" };
            p.Controls.Add(l);
            p.Controls.Add(line);
            t.Controls.Add(p, 0, row);
            t.SetColumnSpan(p, 4);
        }

        // A label with its help line under it, the field, and a badge when the setting needs a restart.
        // The help is plain text and not a hover icon, so that a person finds it without searching for it.
        void AddRow(TableLayoutPanel t, string label, string help, Control input, string badge)
        {
            int row = t.RowCount++;
            t.RowStyles.Add(new RowStyle(SizeType.Absolute, Math.Max(help != null ? 58 : 48, input is TextField ? input.Height + 12 : 0)));
            var lab = new FlowLayoutPanel { FlowDirection = FlowDirection.TopDown, WrapContents = false, AutoSize = true, Anchor = AnchorStyles.Left, Margin = new Padding(0, 4, 0, 0) };
            var title = L(label, false, false);
            lab.Controls.Add(title);
            if (help != null)
            {
                var note = L(help, false, false);
                note.Tag = "hint";
                note.Font = UI.F(8.5f, FontStyle.Regular);
                note.MaximumSize = new Size(250, 0);
                note.Margin = new Padding(0, 1, 0, 0);
                lab.Controls.Add(note);
            }
            t.Controls.Add(lab, 0, row);
            input.Anchor = AnchorStyles.Left;
            input.Margin = new Padding(0);
            input.AccessibleName = label;
            var field = input as TextField;
            if (field != null) field.Box.AccessibleName = label;
            Compact(input);
            Control cell = input;
            if (badge != null) {
                // The badge shows only when the server runs with another value than the field.
                var mark = new Badge(badge) { Margin = new Padding(12, 2, 0, 0), Visible = false };
                restartBadges[label] = mark;
                cell = Inline(input, mark);
            }
            cell.Anchor = AnchorStyles.Left;
            t.Controls.Add(cell, 2, row);
        }

        static Control ScrollPage(Action<TableLayoutPanel> build)
        {
            var scroll = new Panel { Dock = DockStyle.Fill, AutoScroll = true };
            var t = RowTable();
            build(t);
            scroll.Controls.Add(t);
            return scroll;
        }

        // ------------------------------------------------------------ server settings

        Control ServerSettingsPage()
        {
            nameBox = Field(300);
            nameBox.Box.MaxLength = 49;
            passwordBox = Field(220);
            passwordBox.Box.MaxLength = 49;
            passwordBox.Box.UseSystemPasswordChar = true;
            var show = new FlatButton("Show") { Margin = new Padding(8, 0, 0, 0) };
            show.Click += delegate
            {
                passwordBox.Box.UseSystemPasswordChar = !passwordBox.Box.UseSystemPasswordChar;
                show.Text = passwordBox.Box.UseSystemPasswordChar ? "Show" : "Hide";
                show.Invalidate();
            };
            welcomeBox = new TextField(380, true);
            modeBox = Combo(160);
            playersBox = Number(80, 2, 128);
            roundBox = Number(80, 1, 30);
            portBox = Number(100, 1, 65535);
            bytesBox = Number(120, 0, 100000000);
            modeBox.SelectedIndexChanged += delegate { var m = modeBox.SelectedItem as Mode; roundBox.Enabled = m != null && m.Value == "3"; };
            return ScrollPage(delegate (TableLayoutPanel t)
            {
                Section(t, "General");
                AddRow(t, "Server name", null, nameBox, null);
                AddRow(t, "Admin password", "In the game, type /admin and this password.", Inline(passwordBox, show), null);
                AddRow(t, "Welcome text", "Shown to players in the in-game newspaper (readme.txt).", welcomeBox, null);
                Section(t, "Network");
                AddRow(t, "Port", "Forward this UDP port on your router. The default is 27584.", portBox, "Restart required");
                AddRow(t, "Upload limit", "Shared by all players, in bytes per second.", bytesBox, null);
                Section(t, "Game");
                AddRow(t, "Game mode", null, modeBox, "Restart required");
                AddRow(t, "Max players", null, playersBox, "Restart required");
                AddRow(t, "Round time", "Minutes in Round mode.", roundBox, null);
                WatchSettings();
            });
        }

        readonly Dictionary<string, Badge> restartBadges = new Dictionary<string, Badge>();
        string[] runValues;   // port, game mode and max players that the running server started with

        string[] CurrentRestartValues()
        {
            var mode = modeBox.SelectedItem as Mode;
            return new[] { portBox.Value.ToString(CultureInfo.InvariantCulture), mode == null ? "" : mode.Value, playersBox.Value.ToString(CultureInfo.InvariantCulture) };
        }

        void UpdateRestartBadges()
        {
            if (portBox == null || modeBox == null || playersBox == null) return;
            string[] now = CurrentRestartValues();
            string[] labels = { "Port", "Game mode", "Max players" };
            for (int i = 0; i < labels.Length; i++) {
                Badge badge;
                if (restartBadges.TryGetValue(labels[i], out badge)) badge.Visible = server != null && runValues != null && runValues[i] != now[i];
            }
        }

        void WatchSettings()
        {
            EventHandler changed = delegate { SetDirty(true); UpdateRestartBadges(); };
            foreach (TextField f in new[] { nameBox, passwordBox, welcomeBox, playersBox, roundBox, portBox, bytesBox }) f.TextChanged += changed;
            modeBox.SelectedIndexChanged += changed;
        }

        // One bar for the server settings. Reset to defaults sits apart on the left, because it is a last resort.
        Control SettingsBar()
        {
            var bar = new Panel { Dock = DockStyle.Fill, Padding = new Padding(0, 8, 16, 8) };
            var line = new Panel { Dock = DockStyle.Top, Height = 1, Tag = "line" };
            var reset = new FlatButton("Reset to defaults…") { Kind = "link", Dock = DockStyle.Left };
            reset.Click += delegate { ResetSettings(); };
            var flow = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, WrapContents = false, FlowDirection = FlowDirection.RightToLeft };
            saveButton = new FlatButton("Save") { Kind = "primary", Enabled = false, Margin = new Padding(8, 0, 0, 0) };
            undoButton = new FlatButton("Revert changes") { Enabled = false, Margin = new Padding(8, 0, 0, 0) };
            dirtyText = L("", false, true);
            dirtyText.Margin = new Padding(0, 8, 16, 0);
            saveButton.Click += delegate { SaveSettings(); };
            undoButton.Click += delegate { config.Load(); LoadSettings(); };
            flow.Controls.Add(saveButton);
            flow.Controls.Add(undoButton);
            flow.Controls.Add(dirtyText);
            bar.Controls.Add(flow);
            bar.Controls.Add(reset);
            bar.Controls.Add(line);
            return bar;
        }

        void ResetSettings()
        {
            if (MessageBox.Show(this, "Set the port, the upload limit, the game mode, the player limit and the round time back to their default values?",
                "Reset to defaults", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;
            portBox.Value = 27584;
            bytesBox.Value = 100000;
            playersBox.Value = 16;
            roundBox.Value = 7;
            foreach (Mode m in modeBox.Items) if (m.Value == "3") modeBox.SelectedItem = m;
            SetDirty(true);
        }

        static decimal Clamp(string text, decimal fallback, NumberField box)
        {
            decimal v;
            if (!decimal.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out v)) v = fallback;
            return Math.Max(box.Minimum, Math.Min(box.Maximum, Math.Round(v)));
        }

        void LoadSettings()
        {
            loading = true;
            nameBox.Text = config.Get("servername", "");
            passwordBox.Text = config.Get("serverpassword", "");
            if (MissingAdminPassword(passwordBox.Text)) {
                passwordBox.Text = NewAdminPassword();
                config.Set("serverpassword", passwordBox.Text);
                config.Save();
            }
            string readme = Path.Combine(folder, "readme.txt");
            welcomeBox.Text = File.Exists(readme) ? File.ReadAllText(readme, Encoding.Default).TrimEnd() : "";

            string type = config.Get("gametype", "3");
            modeBox.Items.Clear();
            modeBox.Items.Add(new Mode("3", "Round"));
            modeBox.Items.Add(new Mode("4", "World"));
            if (type != "3" && type != "4")
                modeBox.Items.Add(new Mode(type, "Gamemode " + type));
            modeBox.SelectedIndex = -1;
            foreach (Mode m in modeBox.Items)
                if (m.Value == type)
                    modeBox.SelectedItem = m;

            playersBox.Value = Clamp(config.Get("maxnumofplayers", "16"), 16, playersBox);
            roundBox.Value = Clamp(config.Get("roundtime", "7"), 7, roundBox);
            portBox.Value = Clamp(config.Get("port", "27584"), 27584, portBox);
            bytesBox.Value = Clamp(config.Get("servermaxbytespersecond", "100000"), 100000, bytesBox);
            loading = false;
            SetDirty(false);
            UpdateRestartBadges();
        }

        void SaveSettings()
        {
            // Only the server keys change. The client keys (voice and others) stay as they are.
            config.Set("servername", nameBox.Text);
            config.Set("serverpassword", passwordBox.Text);
            config.Set("gametype", ((Mode)modeBox.SelectedItem).Value);
            config.Set("maxnumofplayers", playersBox.Value.ToString(CultureInfo.InvariantCulture));
            config.Set("roundtime", roundBox.Value.ToString(CultureInfo.InvariantCulture));
            config.Set("port", portBox.Value.ToString(CultureInfo.InvariantCulture));
            config.Set("servermaxbytespersecond", bytesBox.Value.ToString(CultureInfo.InvariantCulture));
            config.Save();
            File.WriteAllText(Path.Combine(folder, "readme.txt"), welcomeBox.Text, Encoding.Default);
            SetDirty(false);
            dirtyText.Text = server == null ? "Saved ✓" : "Saved ✓ Restart the server to apply them.";
            dirtyText.ForeColor = UI.T.Ok;
            ShowServerName();
            if (stats != null) stats.Set("Port", portBox.Value.ToString(CultureInfo.InvariantCulture));
        }

        void SetDirty(bool value)
        {
            if (loading)
                return;
            dirty = value;
            saveButton.Enabled = value;
            undoButton.Enabled = value;
            saveButton.Invalidate();
            undoButton.Invalidate();
            tabLabels[SettingsPageName].SetText(value ? SettingsPageName + " *" : SettingsPageName);
            dirtyText.Text = value ? "Unsaved changes" : "";
            dirtyText.ForeColor = UI.T.Warn;
        }

        // ------------------------------------------------------------ launcher settings

        Control LauncherSettingsPage()
        {
            folderBox = Field(240);
            folderBox.Box.ReadOnly = true;
            folderBox.Text = folder;
            browseButton = new FlatButton("Browse…") { Margin = new Padding(8, 0, 0, 0) };
            browseButton.Click += delegate { ChooseFolder(); };
            var open = new FlatButton("Open Folder") { Margin = new Padding(8, 0, 0, 0) };
            open.Click += delegate { System.Diagnostics.Process.Start("explorer.exe", "\"" + folder + "\""); };
            checkUpdate = new FlatButton("Check for Updates");
            checkUpdate.Click += async delegate { await CheckForUpdates(); };
            var update = new FlatButton("Update from .ZIP") { Margin = new Padding(0) };
            update.Click += async delegate { await ChooseUpdate(); };
            previewsBox = new Switch { On = settings.Get("pre_releases", "0") == "1", AccessibleName = "Include Beta Versions" };
            previewsBox.Changed += delegate { settings.Set("pre_releases", previewsBox.On ? "1" : "0"); settings.Save(); };
            themeBox = Combo(160, "Light", "Dark");
            accentBox = Combo(160, AccentNames);
            themeBox.SelectedIndex = settings.Get("theme", "light") == "dark" ? 1 : 0;
            int accent = 0;
            int.TryParse(settings.Get("accent", "0"), out accent);
            accentBox.SelectedIndex = Math.Max(0, Math.Min(accent, AccentNames.Length - 1));
            EventHandler look = delegate
            {
                settings.Set("theme", themeBox.SelectedIndex == 1 ? "dark" : "light");
                settings.Set("accent", accentBox.SelectedIndex.ToString());
                settings.Save();
                ApplyLook();
            };
            themeBox.SelectedIndexChanged += look;
            accentBox.SelectedIndexChanged += look;
            debugModeBox = new Switch { On = settings.Get("developer", "0") == "1", AccessibleName = "Debug Mode" };
            debugModeBox.Changed += delegate { SetDebug(debugModeBox.On); };
            return ScrollPage(delegate (TableLayoutPanel t)
            {
                Section(t, "Server");
                AddRow(t, "Server folder", null, Inline(folderBox, browseButton, open), null);
                Section(t, "Updates");
                AddRow(t, "", null, Inline(checkUpdate, update), null);
                AddRow(t, "Include Beta Versions", null, previewsBox, null);
                Section(t, "Appearance");
                AddRow(t, "Theme", null, themeBox, null);
                AddRow(t, "Accent color", null, accentBox, null);
                Section(t, "About");
                AddRow(t, "Version", null, L(AboutVersion(), false, false), null);
                Section(t, "Debug");
                AddRow(t, "Debug Mode", "Shows the Advanced Settings tab. Each run then saves its own log folder.", debugModeBox, null);
            });
        }

        // Applies the saved theme and accent color to the whole window.
        void ApplyLook()
        {
            int accent = Math.Max(0, Math.Min(accentBox.SelectedIndex, Accents.Length - 1));
            Theme.Light.SetAccent(Accents[accent][0]);
            Theme.Dark.SetAccent(Accents[accent][1]);
            Skin.Window(this, themeBox.SelectedIndex == 1 ? Theme.Dark : Theme.Light);
            tabBar.BackColor = UI.T.Bar;
            statusBar.BackColor = UI.T.SubBar;
            message.ForeColor = UI.T.Muted;
            foreach (Control k in statusBar.Controls) if ((k.Tag as string) == "line") k.BackColor = UI.T.Line;
            RenderConsole();
            RenderPluginDetail();
            RenderCatalog();
            RenderHelp();
            if (!Visible) return;
            // A change of the window size makes the title bar follow the theme.
            Size = new Size(Width + 1, Height);
            Size = new Size(Width - 1, Height);
            Invalidate(true);
        }

        // ------------------------------------------------------------ past logs

        Control TestsPage()
        {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 3 };
            page.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 80));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));

            testsList = Table();
            foreach (var c in new[] { "Start", "Preset", "Minutes", "Speed (min/avg)", "Players", "Result", "Size" }) testsList.Columns.Add(c, c);
            testsList.SelectionChanged += delegate { ShowTestNotes(); };
            page.Controls.Add(testsList, 0, 0);

            testNotes = new TextField(200, true) { Dock = DockStyle.Fill, Margin = new Padding(0, 8, 0, 8) };
            UI.Tips.SetToolTip(testNotes, "Notes of the selected log.");
            page.Controls.Add(testNotes, 0, 1);
            var bar = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false };
            var pack = new FlatButton("Pack the Selected Log") { Height = 28 };
            var open = new FlatButton("Open Folder") { Height = 28 };
            var save = new FlatButton("Save Notes") { Height = 28, Margin = new Padding(0) };
            pack.Click += delegate { string d = SelectedTest(); if (d != null) { SaveNotes(d, testNotes.Text); PackTest(d); } };
            open.Click += delegate { string d = SelectedTest(); if (d != null) System.Diagnostics.Process.Start("explorer.exe", "\"" + d + "\""); };
            save.Click += delegate { string d = SelectedTest(); if (d != null) SaveNotes(d, testNotes.Text); };
            bar.Controls.AddRange(new Control[] { pack, open, save });
            page.Controls.Add(bar, 0, 2);
            return page;
        }

        // A table in the style of the launcher: no border, a line between the rows, and a gray header.
        static DataGridView Table()
        {
            var grid = new DataGridView {
                Dock = DockStyle.Fill, ReadOnly = true, AllowUserToAddRows = false, RowHeadersVisible = false, MultiSelect = false,
                SelectionMode = DataGridViewSelectionMode.FullRowSelect, BorderStyle = BorderStyle.None, CellBorderStyle = DataGridViewCellBorderStyle.SingleHorizontal,
                EnableHeadersVisualStyles = false, ColumnHeadersBorderStyle = DataGridViewHeaderBorderStyle.None, AllowUserToResizeRows = false, ColumnHeadersHeight = 36,
                ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.DisableResizing, AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill,
                Font = UI.F(9.5f, FontStyle.Regular)
            };
            grid.RowTemplate.Height = 36;
            grid.DefaultCellStyle.Padding = new Padding(2, 0, 0, 0);
            return grid;
        }

        void LoadTests()
        {
            if (testsList == null) return;
            testsList.Rows.Clear();
            if (!Directory.Exists(logsRoot))
                return;
            foreach (string dir in Directory.GetDirectories(logsRoot).OrderByDescending(d => d)) {
                var s = new KeyFile(Path.Combine(dir, "session.ini"));
                if (!s.Exists)
                    continue;
                int row = testsList.Rows.Add(
                    s.Get("start", Path.GetFileName(dir)), s.Get("preset", "?"), s.Get("minutes", "running"),
                    s.Get("speed_min", "-") + " / " + s.Get("speed_avg", "-"), s.Get("max_players", "-"),
                    s.Get("result", server != null && dir == testDir ? "running" : "?"), FolderSize(dir));
                testsList.Rows[row].Tag = dir;
            }
            if (testsList.Rows.Count > 0)
                testsList.Rows[0].Selected = true;
            ShowTestNotes();
        }

        string SelectedTest()
        {
            return testsList.SelectedRows.Count == 0 ? null : (string)testsList.SelectedRows[0].Tag;
        }

        void ShowTestNotes()
        {
            string d = SelectedTest();
            string notes = d == null ? null : Path.Combine(d, "notes.txt");
            testNotes.Text = notes != null && File.Exists(notes) ? File.ReadAllText(notes) : "";
        }

        static void SaveNotes(string dir, string text)
        {
            if (dir != null && text.Trim() != "")
                File.WriteAllText(Path.Combine(dir, "notes.txt"), text);
        }

        // ------------------------------------------------------------ advanced settings

        Control AdvancedSettingsPage()
        {
            presetBox = Combo(180);
            presetBox.Items.AddRange(Presets);
            presetBox.SelectedIndexChanged += delegate { OnPreset(); };
            presetText = L("", false, true);
            profileBox = Option("Samples the function that the server runs, every millisecond, into profile.txt.");
            timersBox = Option("Writes the CPU time of each game section in the last tick, once each minute, in the log.");
            packetsBox = Option("Records network traffic, including player names and chat.");
            freezeBox = Option("Automatically dumps the memory when the server freezes for " + FreezeSeconds + " s.");
            dumpType = new Segmented("None", "Small (about 1 MB)", "Full (about 600 MB)");
            dumpType.Changed += delegate { OnCaptureChanged(); };
            var dump = new FlatButton("Save Memory Dump Now") { Margin = new Padding(0) };
            dump.Click += delegate { DumpNow("dump"); };
            return ScrollPage(delegate (TableLayoutPanel t)
            {
                Section(t, "Diagnostics");
                AddRow(t, "Preset", "A set of captures for a run. A change in the options below makes the preset Custom.", presetBox, null);
                Section(t, "Tools");
                AddRow(t, "Function Profiler", "Samples the running function every millisecond into profile.txt. Finds slow code.", profileBox, null);
                AddRow(t, "Section Timers", "Logs the CPU time of each game section in the last tick, once a minute.", timersBox, null);
                AddRow(t, "Network packet capture", "Records network traffic, including player names and chat.", packetsBox, null);
                Section(t, "Crash reports");
                AddRow(t, "Crash dump", "Saved when the server crashes. Send it to the developers.", dumpType, null);
                AddRow(t, "Freeze dump", null, freezeBox, null);
                AddRow(t, "Memory dump", "A copy of the server memory. Send it to the developers to help find a problem.", dump, null);
            });
        }

        Switch Option(string hint)
        {
            var box = new Switch();
            box.Changed += delegate { OnCaptureChanged(); };
            UI.Tips.SetToolTip(box, hint);
            return box;
        }
    }
}
