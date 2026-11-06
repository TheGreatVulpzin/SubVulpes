// The Server page: the buttons, the numbers, the tick rate graph and the console.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.Linq;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using SubVulpes.Ui;

namespace SubVulpes
{
    partial class LauncherForm
    {
        StatsList stats;
        AreaChart chart;
        RichTextBox console;
        Control statsRow;
        TableLayoutPanel serverLayout;
        FlatButton restartButton, killButton;
        Dropdown levelBox;
        SearchBox consoleSearch;
        TextField commandBox;
        string levelFilter = "all";
        bool consoleExpanded;
        int dashHeight = 196, dashWidth = 290, errorCount, warnCount;
        bool graphShown = true;
        static readonly string[] StatNames = { "Uptime", "Tick rate", "Players", "Lowest minute", "Peak players", "Started at", "Port", "Errors", "Warnings", "Last crash", "Server address" };
        const string DefaultHiddenStats = "Peak players|Started at|Port|Errors|Warnings|Last crash|Server address";
        const int MaxLogLines = 5000;
        // Each entry is the level (i, w or e) and the text.
        readonly List<string[]> logLines = new List<string[]>();

        Control ServerPage()
        {
            LoadDashboard();
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 5, Dock = DockStyle.Fill };
            serverLayout = page;
            page.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 46));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, dashHeight));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));
            page.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));
            page.Controls.Add(ServerToolbar(), 0, 0);
            page.Controls.Add(StatsPanel(), 0, 1);
            page.Controls.Add(ConsoleBar(), 0, 2);
            page.Controls.Add(ConsoleBox(), 0, 3);
            page.Controls.Add(CommandRow(), 0, 4);
            // The page has no frame of its own. It sits on the window, like the other pages.
            page.Padding = new Padding(16, 12, 16, 8);
            return page;
        }

        Control ServerToolbar()
        {
            var toolbar = new ToolbarLine { Dock = DockStyle.Fill };
            var right = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, WrapContents = false, FlowDirection = FlowDirection.RightToLeft, Padding = new Padding(0, 4, 0, 0) };
            killButton = new FlatButton("Kill") { Kind = "danger", Margin = new Padding(8, 0, 0, 0), Glyph = "bolt", Enabled = false };
            restartButton = new FlatButton("Restart") { Margin = new Padding(8, 0, 0, 0), Glyph = "restart", Enabled = false };
            stopButton = new FlatButton("Stop") { Margin = new Padding(8, 0, 0, 0), Glyph = "stop", Enabled = false };
            startStop = new FlatButton("Start") { Kind = "primary", Margin = new Padding(8, 0, 0, 0), Glyph = "play" };
            startStop.Click += delegate { StartServer(); };
            stopButton.Click += async delegate { await StopServer(); };
            restartButton.Click += async delegate { if (server != null && await StopServer()) StartServer(); };
            killButton.Click += delegate { KillServer(); };
            UI.Tips.SetToolTip(stopButton, "Asks the server to save and close.");
            UI.Tips.SetToolTip(restartButton, "Stops the server and starts it again.");
            UI.Tips.SetToolTip(killButton, "Ends the server at once. Use it only if Stop does not work.");
            right.Controls.AddRange(new Control[] { killButton, restartButton, stopButton, startStop });
            var left = new FlowLayoutPanel { Dock = DockStyle.Left, AutoSize = true, WrapContents = false, Padding = new Padding(0, 4, 0, 0) };
            // The dot starts at the same edge as the text of the numbers below it.
            stateLabel = new StateLabel { Margin = new Padding(12, 0, 14, 0) };
            toolbar.From = left; toolbar.To = right; toolbar.State = stateLabel;
            stateLabel.Changed += delegate { toolbar.Invalidate(); };
            left.SizeChanged += delegate { toolbar.Invalidate(); };
            right.SizeChanged += delegate { toolbar.Invalidate(); };
            missingText = new Label { AutoSize = true, Tag = "warn", Margin = new Padding(0, 9, 0, 0) };
            ShowMissing(configCreated);
            left.Controls.Add(stateLabel);
            left.Controls.Add(missingText);
            toolbar.Controls.Add(right);
            toolbar.Controls.Add(left);
            return toolbar;
        }

        void LoadDashboard()
        {
            int value;
            if (int.TryParse(settings.Get("dash_height", "196"), out value)) dashHeight = Math.Max(90, Math.Min(value, 400));
            if (int.TryParse(settings.Get("dash_width", "290"), out value)) dashWidth = Math.Max(170, Math.Min(value, 700));
            graphShown = settings.Get("dash_graph", "1") == "1";
        }

        // The choices of the user for the panel stay in launcher.ini.
        void SaveDashboard()
        {
            if (stats == null) return;
            settings.Set("dash_height", dashHeight.ToString());
            settings.Set("dash_width", dashWidth.ToString());
            settings.Set("dash_graph", graphShown ? "1" : "0");
            settings.Set("dash_hidden", string.Join("|", stats.Hidden.ToArray()));
            settings.Save();
        }

        // The list of numbers at the left and the graph at the right. The user drags the lines to resize them.
        Control StatsPanel()
        {
            var grid = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, Padding = new Padding(0, 4, 0, 8) };
            grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, dashWidth));
            grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 9));
            grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            var vgrip = new Grip(true);
            UI.Tips.SetToolTip(vgrip, "Drag to resize.");
            stats = new StatsList { Flat = true };
            chart = new AreaChart { Flat = true };
            UI.Tips.SetToolTip(chart, "Average tick rate for each of the last 20 minutes.");
            foreach (string name in StatNames) stats.Set(name, "-");
            foreach (string name in settings.Get("dash_hidden", DefaultHiddenStats).Split(new[] { '|' }, StringSplitOptions.RemoveEmptyEntries)) stats.Hidden.Add(name);
            // The gear sits inside the panel, in its top right corner.
            var gear = new GearButton();
            UI.Tips.SetToolTip(gear, "Customize this panel.");
            gear.Click += delegate { StatsMenu(gear, grid, vgrip); };
            vgrip.Dragged += delegate (int dx)
            {
                if (!chart.Visible) return;
                dashWidth = Math.Max(170, Math.Min(dashWidth + dx, grid.Width - 280));
                grid.ColumnStyles[0].Width = dashWidth;
            };
            grid.Controls.Add(stats, 0, 0);
            grid.Controls.Add(vgrip, 1, 0);
            grid.Controls.Add(chart, 2, 0);
            ShowGraph(graphShown, grid, vgrip);
            var dash = new Panel { Dock = DockStyle.Fill };
            statsRow = dash;
            var grip = new Grip(false);
            UI.Tips.SetToolTip(grip, "Drag to resize.");
            grip.Dragged += delegate (int dy)
            {
                if (consoleExpanded) return;
                dashHeight = Math.Max(90, Math.Min(dashHeight + dy, serverLayout.Height - 240));
                serverLayout.RowStyles[1].Height = dashHeight;
            };
            dash.Controls.Add(grid);
            dash.Controls.Add(grip);
            dash.Controls.Add(gear);
            gear.BringToFront();
            dash.Resize += delegate { gear.Location = new Point(dash.Width - gear.Width - 8, 10); };
            return dash;
        }

        void ShowGraph(bool show, TableLayoutPanel grid, Control vgrip)
        {
            graphShown = show;
            chart.Visible = show;
            vgrip.Visible = show;
            // Without the graph, the list gives room to the gear in its corner.
            stats.TopPad = show ? 0 : 26;
            stats.Invalidate();
            grid.ColumnStyles[0] = show ? new ColumnStyle(SizeType.Absolute, dashWidth) : new ColumnStyle(SizeType.Percent, 100);
            grid.ColumnStyles[1] = new ColumnStyle(SizeType.Absolute, show ? 9 : 0);
            grid.ColumnStyles[2] = show ? new ColumnStyle(SizeType.Percent, 100) : new ColumnStyle(SizeType.Absolute, 0);
        }

        void StatsMenu(Control gear, TableLayoutPanel grid, Control vgrip)
        {
            var menu = Dropdown.Menu();
            menu.ShowCheckMargin = true;
            var graphItem = new ToolStripMenuItem("Tick rate graph") { Checked = chart.Visible, ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) };
            graphItem.Click += delegate { ShowGraph(!chart.Visible, grid, vgrip); SaveDashboard(); };
            menu.Items.Add(graphItem);
            menu.Items.Add(new ToolStripSeparator());
            foreach (string[] row in stats.All)
            {
                string name = row[0];
                var item = new ToolStripMenuItem(name) { Checked = !stats.Hidden.Contains(name), ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) };
                item.Click += delegate
                {
                    // At least one row stays on.
                    if (stats.Hidden.Contains(name)) stats.Hidden.Remove(name);
                    else if (stats.All.Count - stats.Hidden.Count > 1) stats.Hidden.Add(name);
                    stats.Invalidate();
                    SaveDashboard();
                };
                menu.Items.Add(item);
            }
            menu.Show(gear, new Point(0, gear.Height));
        }

        Control ConsoleBar()
        {
            var bar = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, Padding = new Padding(0, 6, 0, 4) };
            bar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 150));
            bar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 260));
            bar.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            levelBox = new Dropdown(138) { Anchor = AnchorStyles.Left };
            levelBox.Items.AddRange(new object[] { "All levels", "Info", "Warnings", "Errors" });
            string[] keys = { "all", "i", "w", "e" };
            levelBox.SelectedIndex = 0;
            levelBox.SelectedIndexChanged += delegate { levelFilter = keys[levelBox.SelectedIndex]; RenderConsole(); };
            UI.Tips.SetToolTip(levelBox, "Filter by log level.");
            consoleSearch = new SearchBox(240);
            UI.Tips.SetToolTip(consoleSearch, "Filter the log by text.");
            consoleSearch.TextChanged += delegate { RenderConsole(); };
            bar.Controls.Add(levelBox, 0, 0);
            bar.Controls.Add(consoleSearch, 1, 0);
            return bar;
        }

        Control ConsoleBox()
        {
            console = new RichTextBox { Dock = DockStyle.Fill, ReadOnly = true, BorderStyle = BorderStyle.None, Font = new Font("Consolas", 9.5f), DetectUrls = false, WordWrap = false };
            var holder = new Panel { Dock = DockStyle.Fill, Margin = new Padding(3, 0, 3, 0), Tag = "consolebg" };
            holder.Controls.Add(console);
            var expand = new IconButton("expand");
            var copy = new IconButton("copy");
            var clear = new IconButton("clear");
            var save = new IconButton("save");
            expand.Caption = "Expand or collapse the console.";
            copy.Caption = "Copy all.";
            clear.Caption = "Clear the console.";
            save.Caption = "Save the log to a file.";
            clear.Click += delegate { ClearConsole(); };
            copy.Click += delegate { if (console.TextLength > 0) Clipboard.SetText(console.Text); };
            save.Click += async delegate { await SaveLog(); };
            expand.Click += delegate
            {
                consoleExpanded = !consoleExpanded;
                statsRow.Visible = !consoleExpanded;
                serverLayout.RowStyles[1].Height = consoleExpanded ? 0 : dashHeight;
                expand.Glyph = consoleExpanded ? "collapse" : "expand";
                expand.Invalidate();
            };
            IconButton[] icons = { expand, copy, clear, save };
            // The icons float in the top right corner, the same distance from the top and from the right edge.
            foreach (var icon in icons) { holder.Controls.Add(icon); icon.BringToFront(); }
            holder.Resize += delegate
            {
                for (int i = 0; i < icons.Length; i++)
                    icons[i].Location = new Point(holder.Width - 8 - (icons.Length - i) * 28 - (icons.Length - 1 - i) * 6, 8);
            };
            return holder;
        }

        Control CommandRow()
        {
            var row = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, Padding = new Padding(0, 8, 0, 0) };
            row.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            row.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 88));
            commandBox = new TextField(200, false) { Anchor = AnchorStyles.Left | AnchorStyles.Right, Margin = new Padding(0, 0, 8, 0) };
            commandBox.Box.MaxLength = 300;
            UI.Tips.SetToolTip(commandBox, "A server command, for example: plugins list");
            var send = new FlatButton("Send") { Anchor = AnchorStyles.Left, Margin = new Padding(0), Height = 28 };
            Action submit = delegate { SendPluginCommand(commandBox.Text); commandBox.Text = ""; };
            send.Click += delegate { submit(); };
            commandBox.Box.KeyDown += delegate (object s, KeyEventArgs e) { if (e.KeyCode == Keys.Enter) { submit(); e.SuppressKeyPress = true; } };
            row.Controls.Add(commandBox, 0, 0);
            row.Controls.Add(send, 1, 0);
            return row;
        }

        // ------------------------------------------------------------ console text

        static string LevelOf(string line)
        {
            if (Regex.IsMatch(line, @"\b(ERROR|FATAL|CRASH)\b|[Ff]ailed|[Cc]annot")) return "e";
            if (Regex.IsMatch(line, @"\bWARN(ING)?\b|[Ww]arning")) return "w";
            return "i";
        }

        bool ShowsLine(string[] line)
        {
            if (levelFilter != "all" && line[0] != levelFilter) return false;
            string term = consoleSearch == null ? "" : consoleSearch.Text.Trim();
            return term.Length == 0 || line[1].IndexOf(term, StringComparison.OrdinalIgnoreCase) >= 0;
        }

        void PutLine(string[] line)
        {
            Color color = line[0] == "w" ? UI.T.Warn : line[0] == "e" ? UI.T.Bad : UI.T.ConsoleFg;
            console.SelectionStart = console.TextLength;
            console.SelectionColor = color;
            console.AppendText(line[1] + "\n");
        }

        void RenderConsole()
        {
            if (console == null) return;
            console.SuspendLayout();
            console.Clear();
            foreach (string[] line in logLines) if (ShowsLine(line)) PutLine(line);
            console.SelectionStart = console.TextLength;
            console.ScrollToCaret();
            console.ResumeLayout();
        }

        void ClearConsole()
        {
            logLines.Clear();
            if (console != null) console.Clear();
        }

        void AddConsoleLine(string line)
        {
            // The plugin list lines are for the table, not for people.
            if (line.StartsWith("PLUGIN\t", StringComparison.Ordinal) || line.StartsWith("SETTING\t", StringComparison.Ordinal)) return;
            string shown = RedactLog(line, config.Get("serverpassword", ""));
            var entry = new[] { LevelOf(shown), shown };
            if (entry[0] == "e") { errorCount++; stats.Set("Errors", errorCount.ToString()); }
            else if (entry[0] == "w") { warnCount++; stats.Set("Warnings", warnCount.ToString()); }
            logLines.Add(entry);
            // A long run makes many lines. Keep the last part only: the log file has all.
            if (logLines.Count > MaxLogLines) { logLines.RemoveRange(0, 1000); RenderConsole(); return; }
            if (!ShowsLine(entry)) return;
            PutLine(entry);
            console.SelectionStart = console.TextLength;
            console.ScrollToCaret();
        }

        // ------------------------------------------------------------ numbers

        void ResetStats()
        {
            foreach (string name in new[] { "Uptime", "Tick rate", "Players", "Lowest minute", "Peak players", "Started at" }) stats.Set(name, "-");
            errorCount = warnCount = 0;
            stats.Set("Errors", "0");
            stats.Set("Warnings", "0");
            stats.Set("Last crash", settings.Get("last_crash", "None"));
            stats.Set("Port", config.Get("port", "27584"));
            stats.Set("Server address", LocalAddress() + ":" + config.Get("port", "27584"));
            chart.Values = new double[0];
            chart.Invalidate();
            speedStatus = NoSpeedText;
            playersStatus = NoPlayersText;
        }

        void ShowSpeed(double ticks, int players, string bots)
        {
            stats.Set("Tick rate", ticks.ToString("0.0", CultureInfo.InvariantCulture) + " ticks/s");
            stats.Set("Lowest minute", speeds.Min().ToString("0.0", CultureInfo.InvariantCulture));
            chart.Values = speeds.Skip(Math.Max(0, speeds.Count - AreaChart.Slots)).ToArray();
            chart.Invalidate();
            if (players < 0) return;
            stats.Set("Players", players.ToString() + (bots != "0" ? " (+" + bots + " bots)" : ""));
            stats.Set("Peak players", maxPlayers.ToString());
        }

        // The first IPv4 address of this computer. Players on the same network use it.
        static string LocalAddress()
        {
            try {
                foreach (var a in System.Net.Dns.GetHostEntry(System.Net.Dns.GetHostName()).AddressList)
                    if (a.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork) return a.ToString();
            } catch (Exception) { }
            return "unknown";
        }

        void KillServer()
        {
            if (server == null) return;
            if (MessageBox.Show(this, "Do you want to kill the server? This can lead to data loss.", "Kill", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;
            try {
                stopping = true;
                int id = server.Id;
                server.Kill();
                // If the process stays alive, taskkill ends it and its children.
                if (!server.WaitForExit(3000))
                    System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo("taskkill.exe", "/F /T /PID " + id) { UseShellExecute = false, CreateNoWindow = true });
            } catch (Exception ex) { message.Text = "Cannot end the server: " + ex.Message; }
        }
    }
}
