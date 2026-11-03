// A dummy launcher in Windows Forms. It shows the new look with fake data and talks to no server.
// Build: tools\launcher-mockup\build.bat. Run with --shot DIR to save a picture of each page.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using System.Windows.Forms;

namespace SubVulpesMockup
{
    class Theme
    {
        public Color Bg, Win, Panel, Line, Fg, Muted, Accent, Ok, Warn, Bad, Chip, Console, ConsoleFg;
        public static readonly Theme Light = new Theme {
            Bg = Color.FromArgb(238, 240, 243), Win = Color.FromArgb(251, 251, 252), Panel = Color.White, Line = Color.FromArgb(217, 220, 225),
            Fg = Color.FromArgb(27, 29, 33), Muted = Color.FromArgb(106, 112, 122), Accent = Color.FromArgb(217, 99, 30),
            Ok = Color.FromArgb(42, 138, 74), Warn = Color.FromArgb(183, 121, 31), Bad = Color.FromArgb(194, 54, 43),
            Chip = Color.FromArgb(236, 239, 243), Console = Color.Black, ConsoleFg = Color.FromArgb(212, 215, 220) };
        public static readonly Theme Dark = new Theme {
            Bg = Color.FromArgb(15, 17, 20), Win = Color.FromArgb(26, 29, 34), Panel = Color.FromArgb(33, 37, 43), Line = Color.FromArgb(50, 56, 65),
            Fg = Color.FromArgb(232, 234, 238), Muted = Color.FromArgb(154, 161, 172), Accent = Color.FromArgb(240, 138, 75),
            Ok = Color.FromArgb(94, 194, 125), Warn = Color.FromArgb(224, 176, 80), Bad = Color.FromArgb(239, 111, 100),
            Chip = Color.FromArgb(43, 48, 56), Console = Color.Black, ConsoleFg = Color.FromArgb(201, 204, 210) };
    }

    static class UI
    {
        public static Theme T = Theme.Light;
        public static readonly ToolTip Tips = new ToolTip { InitialDelay = 250, ShowAlways = true, AutoPopDelay = 12000 };
        public static Font F(float size, FontStyle style)
        {
            return new Font("Segoe UI", size, style, GraphicsUnit.Point);
        }
        public static GraphicsPath Round(Rectangle r, int rad)
        {
            var p = new GraphicsPath(); int d = rad * 2;
            p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
            p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90); p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
            p.CloseFigure(); return p;
        }
    }

    class FlatButton : Control
    {
        public string Kind = "normal";
        bool hover;
        public FlatButton(string text)
        {
            Text = text; Cursor = Cursors.Hand; Font = UI.F(9.5f, FontStyle.Regular); Height = 34;
            Margin = new Padding(0, 0, 8, 0);
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
            Width = TextRenderer.MeasureText(text, Font).Width + 32;
        }
        protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hover = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias;
            g.Clear(Parent != null ? Parent.BackColor : UI.T.Win);
            var r = new Rectangle(0, 0, Width - 1, Height - 1);
            Color fill = UI.T.Panel, border = UI.T.Line, fg = UI.T.Fg;
            if (Kind == "primary") { fill = UI.T.Ok; border = UI.T.Ok; fg = Color.White; }
            else if (Kind == "danger") { border = UI.T.Bad; fg = UI.T.Bad; }
            if (!Enabled) { fill = UI.T.Chip; border = UI.T.Line; fg = UI.T.Muted; }
            else if (hover) border = UI.T.Accent;
            using (var path = UI.Round(r, 6)) using (var b = new SolidBrush(fill)) using (var p = new Pen(border))
            { g.FillPath(b, path); g.DrawPath(p, path); }
            TextRenderer.DrawText(g, Text, Font, r, fg, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
        }
    }

    class Switch : Control
    {
        bool on;
        public event EventHandler Changed;
        public Switch() { Size = new Size(40, 22); Cursor = Cursors.Hand; SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true); }
        public bool On { get { return on; } set { on = value; Invalidate(); } }
        protected override void OnClick(EventArgs e) { on = !on; Invalidate(); if (Changed != null) Changed(this, EventArgs.Empty); base.OnClick(e); }
        public static void Draw(Graphics g, Rectangle r, bool isOn)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            using (var path = UI.Round(r, r.Height / 2)) using (var b = new SolidBrush(isOn ? UI.T.Ok : UI.T.Line)) g.FillPath(b, path);
            int k = r.Height - 6;
            using (var b = new SolidBrush(Color.White)) g.FillEllipse(b, isOn ? r.Right - k - 3 : r.X + 3, r.Y + 3, k, k);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.Clear(Parent != null ? Parent.BackColor : UI.T.Win);
            Draw(e.Graphics, new Rectangle(0, 0, Width - 1, Height - 1), on);
        }
    }

    class Card : Control
    {
        public string Title = "", Value = "", Sub = "";
        public Card(string title, string value, string sub)
        {
            Title = title; Value = value; Sub = sub; Dock = DockStyle.Fill; Margin = new Padding(0, 0, 10, 0);
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            var r = new Rectangle(0, 0, Width - 1, Height - 1);
            using (var path = UI.Round(r, 8)) using (var b = new SolidBrush(UI.T.Panel)) using (var p = new Pen(UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
            TextRenderer.DrawText(g, Title, UI.F(8.5f, FontStyle.Regular), new Point(12, 9), UI.T.Muted);
            var vf = UI.F(18f, FontStyle.Bold); int vw = TextRenderer.MeasureText(g, Value, vf, new Size(999, 40), TextFormatFlags.NoPadding).Width;
            TextRenderer.DrawText(g, Value, vf, new Point(12, 32), UI.T.Fg, TextFormatFlags.NoPadding);
            if (Sub.Length > 0) TextRenderer.DrawText(g, Sub, UI.F(9f, FontStyle.Regular), new Point(12 + vw + 5, 42), UI.T.Muted, TextFormatFlags.NoPadding);
        }
    }

    class BarChart : Card
    {
        public double[] Values = new double[] { 61.8, 60.2, 59.7, 61.0, 59.9, 60.4, 59.8, 61.2, 59.6, 59.4 };
        public BarChart() : base("Speed per minute", "", "") { }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            var r = new Rectangle(0, 0, Width - 1, Height - 1);
            using (var path = UI.Round(r, 8)) using (var b = new SolidBrush(UI.T.Panel)) using (var p = new Pen(UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
            TextRenderer.DrawText(g, Title, UI.F(8.5f, FontStyle.Regular), new Point(12, 9), UI.T.Muted);
            int x0 = 14, x1 = Width - 14, y0 = 32, y1 = Height - 12, n = Values.Length;
            double lo = 55, hi = 63; int gap = 5; int bw = (x1 - x0 - gap * (n - 1)) / n;
            int low = 0; for (int i = 1; i < n; i++) if (Values[i] < Values[low]) low = i;
            for (int i = 0; i < n; i++)
            {
                int h = Math.Max(4, (int)((Values[i] - lo) / (hi - lo) * (y1 - y0)));
                var br = new Rectangle(x0 + i * (bw + gap), y1 - h, bw, h);
                Color bc = i == n - 1 ? UI.T.Accent : (i == low ? UI.T.Warn : Color.FromArgb(110, UI.T.Accent));
                using (var path = UI.Round(br, 2)) using (var b = new SolidBrush(bc)) g.FillPath(b, path);
            }
        }
    }

    class HelpIcon : Control
    {
        public HelpIcon(string tip)
        {
            Size = new Size(18, 18); Cursor = Cursors.Help; Margin = new Padding(0, 8, 8, 0);
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true);
            UI.Tips.SetToolTip(this, tip);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            using (var p = new Pen(UI.T.Muted)) g.DrawEllipse(p, 1, 1, Width - 3, Height - 3);
            TextRenderer.DrawText(g, "?", UI.F(8.5f, FontStyle.Bold), new Rectangle(0, 0, Width, Height), UI.T.Muted, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
        }
    }

    class TabLabel : Control
    {
        public bool Selected;
        public TabLabel(string text)
        {
            Text = text; Font = UI.F(10f, FontStyle.Regular); Cursor = Cursors.Hand; Dock = DockStyle.Left;
            Width = TextRenderer.MeasureText(text, Font).Width + 36;
            SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw, true);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.Clear(Parent.BackColor);
            TextRenderer.DrawText(e.Graphics, Text, Selected ? UI.F(10f, FontStyle.Bold) : Font, new Rectangle(0, 0, Width, Height - 3),
                Selected ? UI.T.Fg : UI.T.Muted, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
            if (Selected) using (var b = new SolidBrush(UI.T.Accent)) e.Graphics.FillRectangle(b, 10, Height - 3, Width - 20, 3);
        }
    }

    class StateLabel : Control
    {
        public string Word = "Running";
        public StateLabel() { Height = 34; Width = 128; SetStyle(ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer, true); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            var r = new Rectangle(0, 2, Width - 2, Height - 5);
            using (var path = UI.Round(r, r.Height / 2)) using (var b = new SolidBrush(Color.FromArgb(38, UI.T.Ok))) g.FillPath(b, path);
            int cy = r.Y + r.Height / 2;
            using (var b = new SolidBrush(UI.T.Ok)) g.FillEllipse(b, r.X + 14, cy - 5, 10, 10);
            TextRenderer.DrawText(g, Word, UI.F(10.5f, FontStyle.Bold), new Rectangle(r.X + 30, r.Y, r.Width - 30, r.Height), UI.T.Ok, TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
        }
    }

    class Plugin
    {
        public string Id, Name, Version, State, Risk, Source, Desc, By, Needs;
        public bool On; public string[] Caps; public string[][] Settings;
    }

    class MainForm : Form
    {
        readonly List<Panel> pages = new List<Panel>();
        readonly List<TabLabel> tabs = new List<TabLabel>();
        readonly List<Plugin> plugins = new List<Plugin>();
        Panel tabBar; RichTextBox console; DataGridView grid; Panel detail; int sel = 2;

        public MainForm()
        {
            Text = "SubVulpes Server Launcher - v0.1.0"; Size = new Size(1040, 700); MinimumSize = new Size(900, 600);
            StartPosition = FormStartPosition.CenterScreen; Font = UI.F(9.5f, FontStyle.Regular);
            LoadData();
            tabBar = new Panel { Dock = DockStyle.Top, Height = 40, Padding = new Padding(8, 0, 12, 0), Tag = "panel" };
            KeyPreview = true; KeyDown += delegate (object o, KeyEventArgs ke) { if (ke.Control && ke.KeyCode == Keys.T) SetTheme(UI.T == Theme.Light ? Theme.Dark : Theme.Light); };
            string[] names = { "Server", "Settings", "Plugins", "Launcher", "Advanced" };
            for (int i = names.Length - 1; i >= 0; i--)
            {
                var t = new TabLabel(names[i]); int index = i;
                t.Click += delegate { ShowPage(index); };
                tabs.Insert(0, t); tabBar.Controls.Add(t);
            }
            pages.Add(ServerPage()); pages.Add(SettingsPage()); pages.Add(PluginsPage()); pages.Add(LauncherPage()); pages.Add(AdvancedPage());
            foreach (var p in pages) { p.Dock = DockStyle.Fill; p.Padding = new Padding(16, 12, 16, 8); Controls.Add(p); }
            Controls.Add(tabBar);
            tabBar.SendToBack(); foreach (var p in pages) p.BringToFront();
            ShowPage(0); SetTheme(Theme.Light);
        }

        public void ShowPage(int i)
        {
            for (int k = 0; k < pages.Count; k++) { pages[k].Visible = k == i; tabs[k].Selected = k == i; tabs[k].Invalidate(); }
        }

        [DllImport("dwmapi.dll")] static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);

        public void SetTheme(Theme t)
        {
            UI.T = t;
            BackColor = t.Win;
            if (IsHandleCreated) { int dark = t == Theme.Dark ? 1 : 0; if (DwmSetWindowAttribute(Handle, 20, ref dark, 4) != 0) DwmSetWindowAttribute(Handle, 19, ref dark, 4); Size = new Size(Width + 1, Height); Size = new Size(Width - 1, Height); }
            Apply(this); FillConsole(); RenderDetail(); Invalidate(true);
        }

        void Apply(Control c)
        {
            if (c is TextBox) { c.BackColor = UI.T.Panel; c.ForeColor = UI.T.Fg; }
            else if (c is RichTextBox) { c.BackColor = UI.T.Console; c.ForeColor = UI.T.ConsoleFg; }
            else if (c is ComboBox) { c.BackColor = UI.T.Panel; c.ForeColor = UI.T.Fg; }
            else if (c is DataGridView) StyleGrid((DataGridView)c);
            else if (c is Label && (c.Tag as string) == "chip") { c.BackColor = UI.T.Chip; c.ForeColor = UI.T.Fg; }
            else if (c is Label) { c.BackColor = Color.Transparent; c.ForeColor = (c.Tag as string) == "muted" ? UI.T.Muted : (c.Tag as string) == "ok" ? UI.T.Ok : UI.T.Fg; }
            else if (c is FlowLayoutPanel || c is Panel || c is TableLayoutPanel) c.BackColor = (c.Tag as string) == "panel" ? UI.T.Panel : UI.T.Win;
            else if (c is RadioButton) { c.BackColor = Color.Transparent; c.ForeColor = UI.T.Fg; }
            foreach (Control k in c.Controls) Apply(k);
            c.Invalidate();
        }

        void StyleGrid(DataGridView g)
        {
            g.BackgroundColor = UI.T.Panel; g.GridColor = UI.T.Line; g.ForeColor = UI.T.Fg;
            g.DefaultCellStyle.BackColor = UI.T.Panel; g.DefaultCellStyle.ForeColor = UI.T.Fg;
            g.DefaultCellStyle.SelectionBackColor = UI.T.Chip; g.DefaultCellStyle.SelectionForeColor = UI.T.Fg;
            g.ColumnHeadersDefaultCellStyle.BackColor = UI.T.Panel; g.ColumnHeadersDefaultCellStyle.ForeColor = UI.T.Muted;
            g.ColumnHeadersDefaultCellStyle.SelectionBackColor = UI.T.Panel;
        }

        // ---- data
        void LoadData()
        {
            plugins.Add(new Plugin { Id = "sv.status", Name = "Status", Version = "0.1.0", State = "Active", Risk = "low", Source = "Official", On = true, By = "SubVulpes", Desc = "Small example plugin.", Caps = new[] { "read", "write" }, Settings = new string[0][] });
            plugins.Add(new Plugin { Id = "sv.permissions", Name = "Permission System", Version = "0.1.0", State = "Active", Risk = "high", Source = "Official", On = true, By = "SubVulpes", Desc = "Groups, permission nodes and password login policy.", Caps = new[] { "read", "write", "storage", "props", "config", "plugins" }, Settings = new[] { new[] { "Default group", "guest" }, new[] { "Login group", "admin" } } });
            plugins.Add(new Plugin { Id = "sv.admcmd", Name = "Admin Commands", Version = "0.1.0", State = "Active", Risk = "high", Source = "Official", On = true, By = "SubVulpes", Needs = "sv.permissions", Desc = "Player tools and server commands.", Caps = new[] { "read", "write", "props", "config", "plugins" }, Settings = new[] { new[] { "Admin group", "admin" } } });
            plugins.Add(new Plugin { Id = "sv.jukebox", Name = "MIDI Jukebox", Version = "0.2.0", State = "Active", Risk = "low", Source = "Installed", On = true, By = "SubVulpes", Desc = "Play MIDI music with any game sound.", Caps = new[] { "read", "write", "config" }, Settings = new[] { new[] { "Who can use it", "Admins" } } });
            plugins.Add(new Plugin { Id = "sv.testroom", Name = "Test Room", Version = "0.2.18", State = "Restart needed", Risk = "highest", Source = "Installed", On = true, By = "SubVulpes", Desc = "Tests of the plugin API inside the game.", Caps = new[] { "read", "write", "ai", "economy", "mission", "settings", "storage", "engine", "shop", "props", "config", "plugins", "network" }, Settings = new string[0][] });
        }

        // ---- helpers
        static Label L(string text, bool bold, bool muted)
        {
            return new Label { Text = text, AutoSize = true, Font = UI.F(bold ? 10f : 9.5f, bold ? FontStyle.Bold : FontStyle.Regular), Tag = muted ? "muted" : null, Margin = new Padding(0, 6, 8, 6) };
        }

        TableLayoutPanel RowTable()
        {
            var t = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 3 };
            t.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 250)); t.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); t.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 260));
            return t;
        }

        void AddRow(TableLayoutPanel t, string label, string help, Control input, string hint, bool autosave)
        {
            var left = new FlowLayoutPanel { AutoSize = true, FlowDirection = FlowDirection.LeftToRight, WrapContents = false, Margin = new Padding(0, 4, 0, 4) };
            left.Controls.Add(L(label, true, false));
            if (help != null) left.Controls.Add(new HelpIcon(help));
            var right = new FlowLayoutPanel { AutoSize = true, FlowDirection = FlowDirection.LeftToRight, Margin = new Padding(0, 6, 0, 4) };
            var saved = L("Saved ✓", false, false); saved.Tag = "ok"; saved.Visible = false; saved.Margin = new Padding(0, 6, 8, 6);
            if (hint != null) { var h2 = L(hint, false, true); right.Controls.Add(h2); }
            right.Controls.Add(saved);
            int row = t.RowCount++;
            t.Controls.Add(left, 0, row); t.Controls.Add(input, 1, row); t.Controls.Add(right, 2, row);
            if (autosave)
            {
                var timer = new System.Windows.Forms.Timer { Interval = 1600 };
                timer.Tick += delegate { saved.Visible = false; timer.Stop(); };
                EventHandler show = delegate { saved.Visible = true; timer.Stop(); timer.Start(); };
                if (input is TextBox) ((TextBox)input).TextChanged += show;
                else if (input is ComboBox) ((ComboBox)input).SelectedIndexChanged += show;
                else if (input is Switch) ((Switch)input).Changed += show;
            }
        }

        static TextBox TB(string text, int width)
        {
            return new TextBox { Text = text, Width = width, BorderStyle = BorderStyle.FixedSingle, Margin = new Padding(0, 6, 0, 6) };
        }

        // ---- pages
        Panel ServerPage()
        {
            var page = new Panel();
            var bottom = new FlowLayoutPanel { Dock = DockStyle.Bottom, Height = 42, Padding = new Padding(0, 6, 0, 0) };
            var cmd = new TextBox { Width = 560, BorderStyle = BorderStyle.FixedSingle, Margin = new Padding(0, 4, 8, 0) };
            bottom.Controls.Add(cmd); bottom.Controls.Add(new FlatButton("Send")); bottom.Controls.Add(new FlatButton("Save Log"));
            console = new RichTextBox { Dock = DockStyle.Fill, ReadOnly = true, BorderStyle = BorderStyle.None, Font = new Font("Consolas", 9.5f), Padding = new Padding(8) };
            var consoleHolder = new Panel { Dock = DockStyle.Fill, Padding = new Padding(0, 12, 0, 0) };
            consoleHolder.Controls.Add(console);

            var stats = new TableLayoutPanel { Dock = DockStyle.Top, Height = 84, ColumnCount = 6, RowCount = 1 };
            for (int i = 0; i < 5; i++) stats.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 13));
            stats.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 35));
            string[][] cards = {
                new[] { "Uptime", "00:05:39", "", "Time since the server started." },
                new[] { "Speed", "59.6", "ticks/s", "How many game steps (ticks) the server runs each second. About 60 is normal." },
                new[] { "Slowest minute", "59.4", "ticks/s", "The lowest speed in one minute since the server started." },
                new[] { "Players", "1", "", "People who play now. Bots are not counted." },
                new[] { "Bots", "32", "", "Computer players that the game adds." } };
            for (int i = 0; i < cards.Length; i++)
            {
                var c = new Card(cards[i][0], cards[i][1], cards[i][2]); UI.Tips.SetToolTip(c, cards[i][3]); stats.Controls.Add(c, i, 0);
            }
            var chart = new BarChart(); chart.Margin = new Padding(0); UI.Tips.SetToolTip(chart, "The speed of the server in each minute. The orange bar is the last minute. The yellow bar is the slowest."); stats.Controls.Add(chart, 5, 0);

            var top = new Panel { Dock = DockStyle.Top, Height = 42 };
            var buttons = new FlowLayoutPanel { Dock = DockStyle.Right, Width = 380, FlowDirection = FlowDirection.LeftToRight, Padding = new Padding(0, 4, 0, 0) };
            var start = new FlatButton("Start") { Kind = "primary", Enabled = false };
            var stop = new FlatButton("Stop");
            var restart = new FlatButton("Restart");
            var kill = new FlatButton("Kill") { Kind = "danger" };
            buttons.Controls.AddRange(new Control[] { start, stop, restart, kill });
            var state = new StateLabel { Dock = DockStyle.Left, Width = 128 };
            top.Controls.Add(buttons); top.Controls.Add(state);

            page.Controls.Add(consoleHolder); page.Controls.Add(bottom); page.Controls.Add(stats);
            var gap = new Panel { Dock = DockStyle.Top, Height = 8 };
            page.Controls.Add(gap); page.Controls.Add(top);
            gap.BringToFront(); stats.BringToFront(); top.BringToFront();
            // order: top, gap, stats from the top
            top.SendToBack(); gap.SendToBack(); stats.SendToBack();
            top.BringToFront();
            page.Controls.SetChildIndex(top, page.Controls.Count - 1);
            page.Controls.SetChildIndex(gap, page.Controls.Count - 2);
            page.Controls.SetChildIndex(stats, page.Controls.Count - 3);
            consoleHolder.BringToFront();
            return page;
        }

        void FillConsole()
        {
            if (console == null) return;
            console.Clear();
            string[] lines = {
                "i|[20:50:04 INFO]: Server started on port 27584.", "i|[20:50:04 INFO]: Plugin active: sv.permissions 0.1.0", "i|[20:50:04 INFO]: Plugin active: sv.admcmd 0.1.0",
                "w|[20:50:05 WARN]: sv.testroom has full access to the server and can crash it.", "i|[20:51:09 INFO]: Server status: 59.6 ticks/s, 1 players, 32 bots",
                "i|[20:52:09 INFO]: Server status: 59.4 ticks/s, 1 players, 32 bots", "e|[20:53:01 ERROR]: Cannot open the packet file capture.bin", "i|[20:53:09 INFO]: Server status: 60.1 ticks/s, 1 players, 32 bots" };
            foreach (var l in lines)
            {
                Color c = l[0] == 'w' ? UI.T.Warn : l[0] == 'e' ? UI.T.Bad : UI.T.ConsoleFg;
                console.SelectionStart = console.TextLength; console.SelectionColor = c; console.AppendText(l.Substring(2) + "\n");
            }
        }

        Panel SettingsPage()
        {
            var page = new Panel { AutoScroll = true };
            var t = RowTable();
            var note = L("Changes save by themselves.", false, true);
            page.Controls.Add(t); t.BringToFront();
            AddRow(t, "Server name", null, TB("SubVulpes test", 300), null, true);
            var pw = TB("secret123", 220); pw.UseSystemPasswordChar = true;
            AddRow(t, "Admin password", "In the game, type /admin and this password.", pw, null, true);
            AddRow(t, "Port", "Open this UDP port in the router.", TB("27584", 100), "Applies at the next start", true);
            AddRow(t, "Max players", null, TB("16", 80), "Applies at the next start", true);
            var mode = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 160, Margin = new Padding(0, 6, 0, 6) }; mode.Items.AddRange(new object[] { "Round", "World" }); mode.SelectedIndex = 0;
            AddRow(t, "Game mode", null, mode, "Applies at the next start", true);
            AddRow(t, "Round time", "Minutes in Round mode.", TB("7", 80), null, true);
            AddRow(t, "Upload limit", "Bytes per second for all the players.", TB("100000", 120), null, true);
            var welcome = new TextBox { Text = "Welcome to the test server.", Multiline = true, Height = 54, Width = 380, BorderStyle = BorderStyle.FixedSingle, Margin = new Padding(0, 6, 0, 6) };
            AddRow(t, "Welcome text", "The newspaper (readme.txt).", welcome, null, true);
            note.Dock = DockStyle.Top; page.Controls.Add(note); note.SendToBack();
            return page;
        }

        Panel PluginsPage()
        {
            var page = new Panel();
            var bar = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 44, FlowDirection = FlowDirection.RightToLeft, Padding = new Padding(0, 4, 0, 0) };
            bar.Controls.Add(new FlatButton("Refresh")); bar.Controls.Add(new FlatButton("Install plugin…"));
            detail = new Panel { Dock = DockStyle.Right, Width = 380, Padding = new Padding(14), Tag = "panel", AutoScroll = true };
            var sp = new Panel { Dock = DockStyle.Right, Width = 12 };
            var drop = new Label { Dock = DockStyle.Bottom, Height = 54, Text = "Drop a .svplugin file here to install it", TextAlign = ContentAlignment.MiddleCenter, Tag = "muted" };
            grid = new DataGridView
            {
                Dock = DockStyle.Fill, ReadOnly = true, AllowUserToAddRows = false, RowHeadersVisible = false, MultiSelect = false,
                SelectionMode = DataGridViewSelectionMode.FullRowSelect, BorderStyle = BorderStyle.None, CellBorderStyle = DataGridViewCellBorderStyle.SingleHorizontal,
                EnableHeadersVisualStyles = false, ColumnHeadersBorderStyle = DataGridViewHeaderBorderStyle.None, AllowUserToResizeRows = false, ColumnHeadersHeight = 34, ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.DisableResizing,
                AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill
            };
            grid.RowTemplate.Height = 36;
            grid.Columns.Add("n", "Name"); grid.Columns.Add("v", "Version"); grid.Columns.Add("s", "State"); grid.Columns.Add("r", "Risk"); grid.Columns.Add("o", "Source"); grid.Columns.Add("on", "On");
            grid.Columns[0].FillWeight = 160; grid.Columns[1].FillWeight = 70; grid.Columns[2].FillWeight = 110; grid.Columns[3].FillWeight = 70; grid.Columns[4].FillWeight = 80; grid.Columns[5].FillWeight = 50;
            foreach (var p in plugins) grid.Rows.Add(p.Name, p.Version, p.State, p.Risk, p.Source, "");
            grid.CellFormatting += delegate (object s, DataGridViewCellFormattingEventArgs e)
            {
                if (e.RowIndex < 0) return; var p = plugins[e.RowIndex];
                if (e.ColumnIndex == 2) e.CellStyle.ForeColor = p.State == "Active" ? UI.T.Ok : p.State == "Restart needed" ? UI.T.Warn : UI.T.Muted;
                if (e.ColumnIndex == 3) e.CellStyle.ForeColor = p.Risk == "low" ? UI.T.Ok : p.Risk == "high" ? UI.T.Warn : UI.T.Bad;
            };
            grid.CellPainting += delegate (object s, DataGridViewCellPaintingEventArgs e)
            {
                if (e.RowIndex < 0 || e.ColumnIndex != 5) return;
                e.Paint(e.ClipBounds, DataGridViewPaintParts.Background | DataGridViewPaintParts.Border);
                Switch.Draw(e.Graphics, new Rectangle(e.CellBounds.X + 6, e.CellBounds.Y + (e.CellBounds.Height - 20) / 2, 36, 20), plugins[e.RowIndex].On);
                e.Handled = true;
            };
            grid.CellClick += delegate (object s, DataGridViewCellEventArgs e)
            {
                if (e.RowIndex < 0) return;
                sel = e.RowIndex;
                if (e.ColumnIndex == 5) { var p = plugins[e.RowIndex]; p.On = !p.On; p.State = p.On ? "Active" : "Disabled"; grid.Rows[e.RowIndex].Cells[2].Value = p.State; grid.InvalidateRow(e.RowIndex); }
                RenderDetail();
            };
            grid.SelectionChanged += delegate { if (grid.CurrentRow != null) { sel = grid.CurrentRow.Index; RenderDetail(); } };
            grid.CurrentCell = grid.Rows[sel].Cells[0];
            page.Controls.Add(grid); page.Controls.Add(drop); page.Controls.Add(sp); page.Controls.Add(detail); page.Controls.Add(bar);
            bar.SendToBack(); grid.BringToFront();
            return page;
        }

        void RenderDetail()
        {
            if (detail == null || sel < 0 || sel >= plugins.Count) return;
            var p = plugins[sel];
            detail.SuspendLayout(); detail.Controls.Clear();
            var flow = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false, Width = 340, Tag = "panel" };
            var title = L(p.Name, true, false); title.Font = UI.F(13f, FontStyle.Bold);
            flow.Controls.Add(title);
            flow.Controls.Add(L(p.Version + " · " + p.By, false, true));
            var d = L(p.Desc, false, false); d.MaximumSize = new Size(330, 0); flow.Controls.Add(d);
            if (p.Needs != null) { flow.Controls.Add(L("Needs", true, true)); flow.Controls.Add(Chip(p.Needs)); }
            flow.Controls.Add(L("Capabilities", true, true));
            var caps = new FlowLayoutPanel { AutoSize = true, MaximumSize = new Size(330, 0), Tag = "panel" };
            foreach (var c in p.Caps) caps.Controls.Add(Chip(c));
            flow.Controls.Add(caps);
            if (p.Risk == "highest")
            {
                var n = L("This plugin has full access to the server. It can read private data, freeze it or crash it.", false, false);
                n.MaximumSize = new Size(330, 0); n.ForeColor = UI.T.Warn; n.Tag = "warn"; flow.Controls.Add(n);
            }
            if (p.Settings.Length > 0)
            {
                flow.Controls.Add(L("Settings", true, true));
                foreach (var s in p.Settings)
                {
                    var row = new FlowLayoutPanel { AutoSize = true, Tag = "panel" };
                    var lab = L(s[0], false, false); lab.Width = 130; lab.AutoSize = false; lab.Height = 28;
                    row.Controls.Add(lab); row.Controls.Add(TB(s[1], 170));
                    flow.Controls.Add(row);
                }
            }
            var acts = new FlowLayoutPanel { AutoSize = true, Tag = "panel", Margin = new Padding(0, 12, 0, 0) };
            acts.Controls.Add(new FlatButton("Update…")); acts.Controls.Add(new FlatButton("Remove") { Kind = "danger" });
            flow.Controls.Add(acts);
            detail.Controls.Add(flow);
            Apply(detail); foreach (Control c in flow.Controls) if ((c.Tag as string) == "warn") c.ForeColor = UI.T.Warn;
            detail.ResumeLayout();
        }

        static Label Chip(string text)
        {
            var l = new Label { Text = text, AutoSize = true, Padding = new Padding(8, 2, 8, 2), Margin = new Padding(0, 2, 6, 2), Font = UI.F(9f, FontStyle.Regular), Tag = "chip" };
            l.BackColor = UI.T.Chip; l.ForeColor = UI.T.Fg; return l;
        }

        Panel LauncherPage()
        {
            var page = new Panel { AutoScroll = true }; var t = RowTable(); page.Controls.Add(t);
            var folder = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 4, 0, 4) };
            folder.Controls.Add(TB("C:\\SubVulpes-server", 320)); folder.Controls.Add(new FlatButton("Browse…") { Margin = new Padding(8, 3, 0, 0) });
            AddRow(t, "Server folder", null, folder, null, false);
            var upd = new FlowLayoutPanel { AutoSize = true }; upd.Controls.Add(new FlatButton("Check for Updates")); upd.Controls.Add(new FlatButton("Update from .ZIP"));
            AddRow(t, "Updates", "Looks for a newer version on GitHub.", upd, null, false);
            AddRow(t, "Include Beta Versions", "Includes beta versions. They can still contain bugs.", new Switch { Margin = new Padding(0, 8, 0, 8) }, null, true);
            var theme = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 160, Margin = new Padding(0, 6, 0, 6) }; theme.Items.AddRange(new object[] { "Light", "Dark" }); theme.SelectedIndex = 0; theme.SelectedIndexChanged += delegate { SetTheme(theme.SelectedIndex == 1 ? Theme.Dark : Theme.Light); };
            AddRow(t, "Theme", null, theme, null, true);
            return page;
        }

        Panel AdvancedPage()
        {
            var page = new Panel { AutoScroll = true }; var t = RowTable(); page.Controls.Add(t);
            var mode = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = 180, Margin = new Padding(0, 6, 0, 6) }; mode.Items.AddRange(new object[] { "Normal", "Speed Check", "Network Check", "Custom" }); mode.SelectedIndex = 0;
            AddRow(t, "Capture mode:", "Records the server speed. Saves a small memory dump if the server crashes or freezes.", mode, null, true);
            var dump = new FlowLayoutPanel { AutoSize = true, Margin = new Padding(0, 4, 0, 4) };
            dump.Controls.Add(L("Save a memory dump:", false, true)); dump.Controls.Add(new RadioButton { Text = "None", AutoSize = true }); dump.Controls.Add(new RadioButton { Text = "Small (about 1 MB)", AutoSize = true, Checked = true }); dump.Controls.Add(new RadioButton { Text = "Full (about 600 MB)", AutoSize = true });
            AddRow(t, "If the server crashes or freezes", null, dump, null, true);
            AddRow(t, "Speed Profile", "Finds slow functions.", new Switch { On = true, Margin = new Padding(0, 8, 0, 8) }, null, true);
            AddRow(t, "Game Timers", "Writes the time of each part of the game, once each minute.", new Switch { Margin = new Padding(0, 8, 0, 8) }, null, true);
            AddRow(t, "Network Packet Capture", "Records network traffic, including player names and chat.", new Switch { Margin = new Padding(0, 8, 0, 8) }, null, true);
            AddRow(t, "Memory dump", null, new FlatButton("Save Memory Dump Now"), null, false);
            return page;
        }
    }

    static class Program
    {
        [DllImport("user32.dll")] static extern bool SetProcessDPIAware();

        [STAThread]
        static void Main(string[] args)
        {
            SetProcessDPIAware();
            Application.EnableVisualStyles();
            var form = new MainForm();
            if (args.Length == 2 && args[0] == "--shot")
            {
                string dir = args[1]; Directory.CreateDirectory(dir);
                form.TopMost = true;
                form.Shown += delegate
                {
                    var timer = new System.Windows.Forms.Timer { Interval = 400 }; timer.Tick += delegate
                    {
                        timer.Stop();
                        string[] pages = { "server", "settings", "plugins", "launcher", "advanced" };
                        Theme[] themes = { Theme.Light, Theme.Dark }; string[] tn = { "light", "dark" };
                        for (int t = 0; t < 2; t++) for (int i = 0; i < 5; i++)
                        {
                            form.SetTheme(themes[t]); form.ShowPage(i); Application.DoEvents(); Thread.Sleep(350); Application.DoEvents();
                            using (var bmp = new Bitmap(form.Width, form.Height))
                            using (var g = Graphics.FromImage(bmp))
                            { g.CopyFromScreen(form.Location, Point.Empty, form.Size); bmp.Save(Path.Combine(dir, tn[t] + "-" + pages[i] + ".png")); }
                        }
                        Application.Exit();
                    }; timer.Start();
                };
            }
            Application.Run(form);
        }
    }
}
