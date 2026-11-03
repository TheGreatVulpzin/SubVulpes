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
        int dashHeight = 214;
        const int MaxLogLines = 5000;
        // Each entry is the level (i, w or e) and the text.
        readonly List<string[]> logLines = new List<string[]>();

        Control ServerPage()
        {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 5, Dock = DockStyle.Fill, Tag = "panel" };
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
            var surface = new Surface { Padding = new Padding(16, 12, 16, 12) };
            surface.Controls.Add(page);
            return surface;
        }

        Control ServerToolbar()
        {
            var toolbar = new Panel { Dock = DockStyle.Fill, Tag = "panel" };
            var right = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, WrapContents = false, FlowDirection = FlowDirection.RightToLeft, Padding = new Padding(0, 4, 0, 0), Tag = "panel" };
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
            var left = new FlowLayoutPanel { Dock = DockStyle.Left, AutoSize = true, WrapContents = false, Padding = new Padding(0, 4, 0, 0), Tag = "panel" };
            stateLabel = new StateLabel { Margin = new Padding(0, 0, 14, 0) };
            missingText = new Label { AutoSize = true, Tag = "warn", Margin = new Padding(0, 9, 0, 0) };
            ShowMissing(configCreated);
            left.Controls.Add(stateLabel);
            left.Controls.Add(missingText);
            toolbar.Controls.Add(right);
            toolbar.Controls.Add(left);
            return toolbar;
        }

        // The list of numbers at the left and the graph at the right. The user drags the lines to resize them.
        Control StatsPanel()
        {
            var grid = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, Padding = new Padding(0, 4, 0, 8), Tag = "panel" };
            int listWidth = 290;
            grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, listWidth));
            grid.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 9));
            grid.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            var vgrip = new Grip(true);
            UI.Tips.SetToolTip(vgrip, "Drag to resize.");
            stats = new StatsList { Flat = true };
            chart = new AreaChart { Flat = true };
            UI.Tips.SetToolTip(stats, "Tick rate is how many updates the server runs each second. About 60 is normal. Players does not include bots.");
            UI.Tips.SetToolTip(chart, "Average tick rate for each of the last 20 minutes.");
            foreach (string name in new[] { "Uptime", "Tick rate", "Players", "Lowest minute", "Peak players", "Started at", "Port" }) stats.Set(name, "-");
            foreach (string name in new[] { "Peak players", "Started at", "Port" }) stats.Hidden.Add(name);
            var head = new Panel { Dock = DockStyle.Top, Height = 28, Tag = "panel" };
            var gear = new GearButton { Dock = DockStyle.Right };
            head.Controls.Add(gear);
            UI.Tips.SetToolTip(gear, "Customize this panel.");
            gear.Click += delegate { StatsMenu(gear, grid, vgrip, listWidth); };
            vgrip.Dragged += delegate (int dx)
            {
                if (!chart.Visible) return;
                listWidth = Math.Max(170, Math.Min(listWidth + dx, grid.Width - 280));
                grid.ColumnStyles[0].Width = listWidth;
            };
            grid.Controls.Add(stats, 0, 0);
            grid.Controls.Add(vgrip, 1, 0);
            grid.Controls.Add(chart, 2, 0);
            var dash = new Panel { Dock = DockStyle.Fill, Tag = "panel" };
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
            dash.Controls.Add(head);
            dash.Controls.Add(grip);
            return dash;
        }

        void StatsMenu(Control gear, TableLayoutPanel grid, Control vgrip, int listWidth)
        {
            var menu = Dropdown.Menu();
            menu.ShowCheckMargin = true;
            var graphItem = new ToolStripMenuItem("Tick rate graph") { Checked = chart.Visible, ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) };
            graphItem.Click += delegate
            {
                chart.Visible = !chart.Visible;
                vgrip.Visible = chart.Visible;
                grid.ColumnStyles[0] = chart.Visible ? new ColumnStyle(SizeType.Absolute, listWidth) : new ColumnStyle(SizeType.Percent, 100);
                grid.ColumnStyles[1] = new ColumnStyle(SizeType.Absolute, chart.Visible ? 9 : 0);
                grid.ColumnStyles[2] = chart.Visible ? new ColumnStyle(SizeType.Percent, 100) : new ColumnStyle(SizeType.Absolute, 0);
            };
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
                };
                menu.Items.Add(item);
            }
            menu.Show(gear, new Point(0, gear.Height));
        }

        Control ConsoleBar()
        {
            var bar = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, Padding = new Padding(0, 6, 0, 4), Tag = "panel" };
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
            UI.Tips.SetToolTip(expand, "Expand or collapse the console.");
            UI.Tips.SetToolTip(copy, "Copy all.");
            UI.Tips.SetToolTip(clear, "Clear the console.");
            UI.Tips.SetToolTip(save, "Save the log to a file.");
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
            var row = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, Padding = new Padding(0, 8, 0, 0), Tag = "panel" };
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
            stats.Set("Port", config.Get("port", "27584"));
            chart.Values = new double[0];
            chart.Invalidate();
            speedStatus = NoSpeedText;
            playersStatus = NoPlayersText;
        }

        void ShowSpeed(double ticks, int players, string bots)
        {
            stats.Set("Tick rate", ticks.ToString("0.0", CultureInfo.InvariantCulture) + " ticks/s");
            stats.Set("Lowest minute", speeds.Min().ToString("0.0", CultureInfo.InvariantCulture));
            chart.Values = speeds.Skip(Math.Max(0, speeds.Count - 20)).ToArray();
            chart.Invalidate();
            if (players < 0) return;
            stats.Set("Players", players.ToString() + (bots != "0" ? " (+" + bots + " bots)" : ""));
            stats.Set("Peak players", maxPlayers.ToString());
        }

        void KillServer()
        {
            if (server == null) return;
            if (MessageBox.Show(this, "End the server at once? It cannot save the game.", "Kill", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes) return;
            try {
                stopping = true;
                server.Kill();
            } catch (Exception ex) { message.Text = "Cannot end the server: " + ex.Message; }
        }
    }
}
