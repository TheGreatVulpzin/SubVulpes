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
        public Color Bg, Win, Panel, Line, Fg, Muted, Accent, Ok, Warn, Bad, Chip, Console, ConsoleFg, Bar, BarFg, BarMuted;
        public static readonly Theme Light = new Theme {
            Bg = Color.FromArgb(238, 240, 243), Win = Color.FromArgb(246, 247, 249), Panel = Color.White, Line = Color.FromArgb(217, 220, 225),
            Fg = Color.FromArgb(27, 29, 33), Muted = Color.FromArgb(106, 112, 122), Accent = Color.FromArgb(217, 99, 30),
            Ok = Color.FromArgb(42, 138, 74), Warn = Color.FromArgb(183, 121, 31), Bad = Color.FromArgb(194, 54, 43),
            Chip = Color.FromArgb(236, 239, 243), Console = Color.Black, ConsoleFg = Color.FromArgb(212, 215, 220),
            Bar = Color.FromArgb(36, 43, 58), BarFg = Color.White, BarMuted = Color.FromArgb(160, 170, 188) };
        public static readonly Theme Dark = new Theme {
            Bg = Color.FromArgb(15, 17, 20), Win = Color.FromArgb(26, 29, 34), Panel = Color.FromArgb(33, 37, 43), Line = Color.FromArgb(50, 56, 65),
            Fg = Color.FromArgb(232, 234, 238), Muted = Color.FromArgb(154, 161, 172), Accent = Color.FromArgb(240, 138, 75),
            Ok = Color.FromArgb(94, 194, 125), Warn = Color.FromArgb(224, 176, 80), Bad = Color.FromArgb(239, 111, 100),
            Chip = Color.FromArgb(43, 48, 56), Console = Color.Black, ConsoleFg = Color.FromArgb(201, 204, 210),
            Bar = Color.FromArgb(14, 16, 20), BarFg = Color.White, BarMuted = Color.FromArgb(140, 148, 162) };
    }

    static class UI
    {
        public static Theme T = Theme.Light;
        public static readonly ToolTip Tips = new ToolTip { InitialDelay = 250, ShowAlways = true, AutoPopDelay = 12000 };
        public static Font F(float size, FontStyle style) { return new Font("Segoe UI", size, style, GraphicsUnit.Point); }
        public static GraphicsPath Round(Rectangle r, int rad)
        {
            var p = new GraphicsPath(); int d = Math.Max(2, rad * 2);
            p.AddArc(r.X, r.Y, d, d, 180, 90); p.AddArc(r.Right - d, r.Y, d, d, 270, 90);
            p.AddArc(r.Right - d, r.Bottom - d, d, d, 0, 90); p.AddArc(r.X, r.Bottom - d, d, d, 90, 90);
            p.CloseFigure(); return p;
        }
        public const ControlStyles Paint = ControlStyles.AllPaintingInWmPaint | ControlStyles.UserPaint | ControlStyles.OptimizedDoubleBuffer | ControlStyles.ResizeRedraw;
    }

    class FlatButton : Control
    {
        public string Kind = "normal";
        bool hover;
        public FlatButton(string text)
        {
            Text = text; Cursor = Cursors.Hand; Font = UI.F(9.5f, FontStyle.Regular); Height = 34; Margin = new Padding(0, 0, 8, 0);
            SetStyle(UI.Paint, true);
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
            using (var path = UI.Round(r, 6)) using (var b = new SolidBrush(fill)) using (var p = new Pen(border)) { g.FillPath(b, path); g.DrawPath(p, path); }
            TextRenderer.DrawText(g, Text, Font, r, fg, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
        }
    }

    class Switch : Control
    {
        bool on;
        public event EventHandler Changed;
        public Switch() { Size = new Size(40, 22); Cursor = Cursors.Hand; SetStyle(UI.Paint, true); }
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
        public string Title = "", Value = "", Unit = "";
        public Card(string title, string value, string unit)
        {
            Title = title; Value = value; Unit = unit; Dock = DockStyle.Fill; Margin = new Padding(0, 0, 10, 0); SetStyle(UI.Paint, true);
        }
        protected static void Frame(Graphics g, int w, int h, Control parent)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(parent.BackColor);
            using (var path = UI.Round(new Rectangle(0, 0, w - 1, h - 1), 8)) using (var b = new SolidBrush(UI.T.Panel)) using (var p = new Pen(UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; Frame(g, Width, Height, Parent);
            TextRenderer.DrawText(g, Title, UI.F(8.5f, FontStyle.Regular), new Point(14, 12), UI.T.Muted, TextFormatFlags.NoPadding);
            var vf = UI.F(19f, FontStyle.Bold); int vw = TextRenderer.MeasureText(g, Value, vf, new Size(999, 40), TextFormatFlags.NoPadding).Width;
            TextRenderer.DrawText(g, Value, vf, new Point(14, 34), UI.T.Fg, TextFormatFlags.NoPadding);
            if (Unit.Length > 0) TextRenderer.DrawText(g, Unit, UI.F(9f, FontStyle.Regular), new Point(14 + vw + 5, 46), UI.T.Muted, TextFormatFlags.NoPadding);
        }
    }

    class StatsList : Card
    {
        public string[][] Items = { new[] { "Uptime", "00:05:39" }, new[] { "Speed", "59.6 ticks/s" }, new[] { "Players", "1" } };
        public StatsList() : base("", "", "") { }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; Frame(g, Width, Height, Parent);
            int rowH = (Height - 8) / Items.Length;
            for (int i = 0; i < Items.Length; i++)
            {
                int y = 4 + i * rowH;
                TextRenderer.DrawText(g, Items[i][0], UI.F(9.5f, FontStyle.Regular), new Rectangle(16, y, Width / 2, rowH), UI.T.Muted, TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
                TextRenderer.DrawText(g, Items[i][1], UI.F(13f, FontStyle.Bold), new Rectangle(Width / 2 - 16, y, Width / 2, rowH), UI.T.Fg, TextFormatFlags.Right | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
                if (i < Items.Length - 1) using (var pen = new Pen(Color.FromArgb(110, UI.T.Line))) g.DrawLine(pen, 14, y + rowH, Width - 16, y + rowH);
            }
        }
    }

    class AreaChart : Card
    {
        public double[] Values = new double[] { 61.8, 60.9, 61.2, 60.2, 59.7, 60.5, 61.0, 60.1, 59.9, 60.6, 60.4, 59.8, 60.9, 61.2, 60.3, 59.6, 59.9, 60.2, 59.8, 59.6 };
        public AreaChart() : base("Speed per minute", "", "") { Margin = new Padding(0); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; Frame(g, Width, Height, Parent);
            TextRenderer.DrawText(g, Title, UI.F(8.5f, FontStyle.Regular), new Point(14, 12), UI.T.Muted, TextFormatFlags.NoPadding);
            TextRenderer.DrawText(g, "ticks/s", UI.F(8.5f, FontStyle.Regular), new Point(Width - 56, 12), UI.T.Muted, TextFormatFlags.NoPadding);
            int left = 46, right = Width - 20, top = 40, bottom = Height - 34; double lo = 58, hi = 63;
            if (right - left < 40 || bottom - top < 20) return;
            double[] marks = { 63, 61, 59 };
            foreach (var m in marks)
            {
                int y = bottom - (int)((m - lo) / (hi - lo) * (bottom - top));
                using (var pen = new Pen(Color.FromArgb(m == 61 ? 140 : 70, UI.T.Line)) { DashStyle = m == 61 ? DashStyle.Dash : DashStyle.Solid }) g.DrawLine(pen, left, y, right, y);
                TextRenderer.DrawText(g, m.ToString(), UI.F(8f, FontStyle.Regular), new Rectangle(8, y - 8, left - 14, 16), UI.T.Muted, TextFormatFlags.Right | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
            }
            int n = Values.Length; var pts = new PointF[n];
            for (int i = 0; i < n; i++) pts[i] = new PointF(left + i * (right - left) / (float)(n - 1), bottom - (float)((Values[i] - lo) / (hi - lo)) * (bottom - top));
            using (var fill = new GraphicsPath())
            {
                fill.AddCurve(pts, 0.4f); fill.AddLine(pts[n - 1].X, pts[n - 1].Y, pts[n - 1].X, bottom); fill.AddLine(pts[n - 1].X, bottom, pts[0].X, bottom); fill.CloseFigure();
                using (var br = new LinearGradientBrush(new Rectangle(left, top - 1, right - left, bottom - top + 2), Color.FromArgb(120, UI.T.Accent), Color.FromArgb(0, UI.T.Accent), 90f)) g.FillPath(br, fill);
            }
            using (var pen = new Pen(UI.T.Accent, 2.5f) { LineJoin = LineJoin.Round }) g.DrawCurve(pen, pts, 0.4f);
            var last = pts[n - 1];
            using (var b = new SolidBrush(UI.T.Panel)) g.FillEllipse(b, last.X - 6, last.Y - 6, 12, 12);
            using (var b = new SolidBrush(UI.T.Accent)) g.FillEllipse(b, last.X - 4, last.Y - 4, 8, 8);
            string label = Values[n - 1].ToString("0.0");
            var lr = new Rectangle((int)last.X - 40, (int)last.Y - 30, 34, 20);
            using (var path = UI.Round(lr, 5)) using (var b = new SolidBrush(UI.T.Fg)) g.FillPath(b, path);
            TextRenderer.DrawText(g, label, UI.F(8.5f, FontStyle.Bold), lr, UI.T.Panel, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
            TextRenderer.DrawText(g, "20 min ago", UI.F(8f, FontStyle.Regular), new Point(left, bottom + 10), UI.T.Muted, TextFormatFlags.NoPadding);
            TextRenderer.DrawText(g, "now", UI.F(8f, FontStyle.Regular), new Rectangle(right - 40, bottom + 10, 40, 14), UI.T.Muted, TextFormatFlags.Right | TextFormatFlags.NoPadding);
        }
    }

    class HelpIcon : Control
    {
        public HelpIcon(string tip)
        {
            Size = new Size(20, 20); Cursor = Cursors.Help; SetStyle(UI.Paint, true); UI.Tips.SetToolTip(this, tip);
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
            Width = TextRenderer.MeasureText(text, UI.F(10f, FontStyle.Bold)).Width + 40; SetStyle(UI.Paint, true);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.Clear(UI.T.Bar);
            TextRenderer.DrawText(e.Graphics, Text, Selected ? UI.F(10f, FontStyle.Bold) : Font, new Rectangle(0, 0, Width, Height - 3),
                Selected ? UI.T.BarFg : UI.T.BarMuted, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
            if (Selected) using (var b = new SolidBrush(UI.T.Accent)) e.Graphics.FillRectangle(b, 12, Height - 3, Width - 24, 3);
        }
    }

    class PillTab : Control
    {
        public bool Selected;
        public PillTab(string text)
        {
            Text = text; Cursor = Cursors.Hand; Height = 30; Margin = new Padding(0, 0, 6, 0); SetStyle(UI.Paint, true);
            Width = TextRenderer.MeasureText(text, UI.F(9.5f, FontStyle.Bold)).Width + 28;
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            var r = new Rectangle(0, 0, Width - 1, Height - 1);
            if (Selected) using (var path = UI.Round(r, 14)) using (var b = new SolidBrush(Color.FromArgb(44, UI.T.Accent))) g.FillPath(b, path);
            TextRenderer.DrawText(g, Text, Selected ? UI.F(9.5f, FontStyle.Bold) : UI.F(9.5f, FontStyle.Regular), r, Selected ? UI.T.Accent : UI.T.Muted, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
        }
    }

    class StateLabel : Control
    {
        public string Word = "Running";
        public StateLabel() { Height = 34; Width = 128; SetStyle(UI.Paint, true); }
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

    class EventList : Control
    {
        readonly string[][] items = {
            new[] { "20:53:40", "w", "Speed was below 58 ticks/s for one minute" },
            new[] { "20:53:01", "e", "Packet capture could not start" },
            new[] { "20:51:20", "i", "Alex joined the game" },
            new[] { "20:50:05", "w", "Test Room needs a restart" },
            new[] { "20:50:04", "i", "5 plugins loaded" },
            new[] { "20:50:04", "o", "Server started" },
            new[] { "20:49:58", "i", "Settings saved" } };
        public EventList() { Dock = DockStyle.Fill; SetStyle(UI.Paint, true); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(UI.T.Panel);
            using (var pen = new Pen(UI.T.Line)) g.DrawRectangle(pen, 0, 0, Width - 1, Height - 1);
            for (int i = 0; i < items.Length; i++)
            {
                int y = 6 + i * 34;
                Color c = items[i][1] == "w" ? UI.T.Warn : items[i][1] == "e" ? UI.T.Bad : items[i][1] == "o" ? UI.T.Ok : UI.T.Muted;
                using (var b = new SolidBrush(c)) g.FillEllipse(b, 16, y + 12, 9, 9);
                TextRenderer.DrawText(g, items[i][0], UI.F(9f, FontStyle.Regular), new Rectangle(36, y, 70, 34), UI.T.Muted, TextFormatFlags.VerticalCenter);
                TextRenderer.DrawText(g, items[i][2], UI.F(10f, FontStyle.Regular), new Rectangle(110, y, Width - 120, 34), UI.T.Fg, TextFormatFlags.VerticalCenter);
                if (i < items.Length - 1) using (var pen = new Pen(Color.FromArgb(90, UI.T.Line))) g.DrawLine(pen, 16, y + 33, Width - 16, y + 33);
            }
        }
    }

    class Plugin
    {
        public string Id, Name, Version, State, Risk, Source, Desc, By, Needs;
        public bool On; public string[] Caps; public string[][] Settings;
    }

    class MainForm : Form
    {
        readonly List<Control> pages = new List<Control>();
        readonly List<TabLabel> tabs = new List<TabLabel>();
        readonly List<Control> subPages = new List<Control>();
        readonly List<PillTab> subTabs = new List<PillTab>();
        readonly List<Control> viewPages = new List<Control>();
        readonly List<PillTab> viewTabs = new List<PillTab>();
        readonly List<Plugin> plugins = new List<Plugin>();
        Panel tabBar; RichTextBox console; DataGridView grid; Panel detail; int sel = 2;

        public MainForm()
        {
            Text = "SubVulpes Server Launcher - v0.1.0"; Size = new Size(1040, 700); MinimumSize = new Size(900, 600);
            StartPosition = FormStartPosition.CenterScreen; Font = UI.F(9.5f, FontStyle.Regular); DoubleBuffered = true;
            KeyPreview = true; KeyDown += delegate (object o, KeyEventArgs ke) { if (ke.Control && ke.KeyCode == Keys.T) SetTheme(UI.T == Theme.Light ? Theme.Dark : Theme.Light); };
            LoadData();
            tabBar = new Panel { Dock = DockStyle.Top, Height = 42, Padding = new Padding(8, 0, 8, 0) };
            string[] names = { "Server", "Settings", "Plugins", "Help" };
            for (int i = names.Length - 1; i >= 0; i--)
            {
                var t = new TabLabel(names[i]); int index = i;
                t.Click += delegate { ShowPage(index); };
                tabs.Insert(0, t); tabBar.Controls.Add(t);
            }
            pages.Add(ServerPage()); pages.Add(SettingsHost()); pages.Add(PluginsPage()); pages.Add(HelpPage());
            foreach (var p in pages) { p.Dock = DockStyle.Fill; p.Padding = new Padding(16, 12, 16, 10); Controls.Add(p); }
            Controls.Add(tabBar);
            tabBar.SendToBack(); foreach (var p in pages) p.BringToFront();
            ShowPage(0); SetTheme(Theme.Light);
        }

        public void ShowPage(int i)
        {
            for (int k = 0; k < pages.Count; k++) { pages[k].Visible = k == i; tabs[k].Selected = k == i; tabs[k].Invalidate(); }
        }

        void ShowSub(int i) { for (int k = 0; k < subPages.Count; k++) { subPages[k].Visible = k == i; subTabs[k].Selected = k == i; subTabs[k].Invalidate(); } }
        void ShowView(int i) { for (int k = 0; k < viewPages.Count; k++) { viewPages[k].Visible = k == i; viewTabs[k].Selected = k == i; viewTabs[k].Invalidate(); } }

        public void Go(string what)
        {
            if (what == "server") { ShowPage(0); ShowView(0); }
            else if (what == "events") { ShowPage(0); ShowView(1); }
            else if (what == "settings-server") { ShowPage(1); ShowSub(0); }
            else if (what == "settings-launcher") { ShowPage(1); ShowSub(1); }
            else if (what == "settings-advanced") { ShowPage(1); ShowSub(2); }
            else if (what == "plugins") ShowPage(2);
            else if (what == "help") ShowPage(3);
        }
        public static string[] Names = { "server", "events", "settings-server", "settings-launcher", "settings-advanced", "plugins", "help" };

        [DllImport("dwmapi.dll")] static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);

        public void SetTheme(Theme t)
        {
            UI.T = t; BackColor = t.Win;
            if (IsHandleCreated) { int dark = t == Theme.Dark ? 1 : 0; if (DwmSetWindowAttribute(Handle, 20, ref dark, 4) != 0) DwmSetWindowAttribute(Handle, 19, ref dark, 4); Size = new Size(Width + 1, Height); Size = new Size(Width - 1, Height); }
            tabBar.BackColor = t.Bar;
            Apply(this); FillConsole(); RenderDetail(); Invalidate(true);
        }

        void Apply(Control c)
        {
            if (c == tabBar) { c.BackColor = UI.T.Bar; foreach (Control k in c.Controls) k.Invalidate(); return; }
            if (c is TextBox) { c.BackColor = UI.T.Panel; c.ForeColor = UI.T.Fg; }
            else if (c is RichTextBox) { c.BackColor = UI.T.Console; c.ForeColor = UI.T.ConsoleFg; }
            else if (c is ComboBox) { c.BackColor = UI.T.Panel; c.ForeColor = UI.T.Fg; }
            else if (c is DataGridView) StyleGrid((DataGridView)c);
            else if (c is Label && (c.Tag as string) == "chip") { c.BackColor = UI.T.Chip; c.ForeColor = UI.T.Fg; }
            else if (c is Label) { c.BackColor = Color.Transparent; c.ForeColor = (c.Tag as string) == "muted" ? UI.T.Muted : (c.Tag as string) == "ok" ? UI.T.Ok : (c.Tag as string) == "warn" ? UI.T.Warn : UI.T.Fg; }
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

        void LoadData()
        {
            plugins.Add(new Plugin { Id = "sv.status", Name = "Status", Version = "0.1.0", State = "Active", Risk = "low", Source = "Official", On = true, By = "SubVulpes", Desc = "Small example plugin.", Caps = new[] { "read", "write" }, Settings = new string[0][] });
            plugins.Add(new Plugin { Id = "sv.permissions", Name = "Permission System", Version = "0.1.0", State = "Active", Risk = "high", Source = "Official", On = true, By = "SubVulpes", Desc = "Groups, permission nodes and password login policy.", Caps = new[] { "read", "write", "storage", "props", "config", "plugins" }, Settings = new[] { new[] { "Default group", "guest" }, new[] { "Login group", "admin" } } });
            plugins.Add(new Plugin { Id = "sv.admcmd", Name = "Admin Commands", Version = "0.1.0", State = "Active", Risk = "high", Source = "Official", On = true, By = "SubVulpes", Needs = "sv.permissions", Desc = "Player tools and server commands.", Caps = new[] { "read", "write", "props", "config", "plugins" }, Settings = new[] { new[] { "Admin group", "admin" } } });
            plugins.Add(new Plugin { Id = "sv.jukebox", Name = "MIDI Jukebox", Version = "0.2.0", State = "Active", Risk = "low", Source = "Installed", On = true, By = "SubVulpes", Desc = "Play MIDI music with any game sound.", Caps = new[] { "read", "write", "config" }, Settings = new[] { new[] { "Who can use it", "Admins" } } });
            plugins.Add(new Plugin { Id = "sv.testroom", Name = "Test Room", Version = "0.2.18", State = "Restart needed", Risk = "highest", Source = "Installed", On = true, By = "SubVulpes", Desc = "Tests of the plugin API inside the game.", Caps = new[] { "read", "write", "ai", "economy", "mission", "settings", "storage", "engine", "shop", "props", "config", "plugins", "network" }, Settings = new string[0][] });
        }

        // ---- small builders
        static Label L(string text, bool bold, bool muted)
        {
            return new Label { Text = text, AutoSize = true, Font = UI.F(bold ? 10f : 9.5f, bold ? FontStyle.Bold : FontStyle.Regular), Tag = muted ? "muted" : null, Margin = new Padding(0) };
        }

        static TextBox TB(string text, int width)
        {
            return new TextBox { Text = text, Width = width, BorderStyle = BorderStyle.FixedSingle, Margin = new Padding(0), Anchor = AnchorStyles.Left };
        }

        TableLayoutPanel RowTable()
        {
            var t = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 4 };
            t.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 240)); t.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 34));
            t.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); t.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 230));
            return t;
        }

        void AddRow(TableLayoutPanel t, string label, string help, Control input, string hint, bool autosave)
        {
            int row = t.RowCount++; t.RowStyles.Add(new RowStyle(SizeType.Absolute, 48));
            var lab = L(label, true, false); lab.Anchor = AnchorStyles.Left; t.Controls.Add(lab, 0, row);
            if (help != null) { var h = new HelpIcon(help) { Anchor = AnchorStyles.Left }; t.Controls.Add(h, 1, row); }
            input.Anchor = AnchorStyles.Left; input.Margin = new Padding(0); t.Controls.Add(input, 2, row);
            var right = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Anchor = AnchorStyles.Left, Margin = new Padding(0) };
            var saved = L("Saved ✓", false, false); saved.Tag = "ok"; saved.Visible = false; saved.Margin = new Padding(0, 0, 10, 0);
            if (hint != null) { var h2 = L(hint, false, true); h2.Margin = new Padding(0, 0, 10, 0); right.Controls.Add(h2); }
            right.Controls.Add(saved); t.Controls.Add(right, 3, row);
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

        static FlowLayoutPanel Inline(params Control[] items)
        {
            var f = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0) };
            foreach (var c in items) f.Controls.Add(c);
            return f;
        }

        static ComboBox Combo(int width, params object[] items)
        {
            var c = new ComboBox { DropDownStyle = ComboBoxStyle.DropDownList, Width = width, Margin = new Padding(0) }; c.Items.AddRange(items); c.SelectedIndex = 0; return c;
        }

        // ---- server page
        Control ServerPage()
        {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 5 };
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 44)); page.RowStyles.Add(new RowStyle(SizeType.Absolute, 168));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 40)); page.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); page.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));

            var top = new Panel { Dock = DockStyle.Fill };
            var buttons = new FlowLayoutPanel { Dock = DockStyle.Right, Width = 372, FlowDirection = FlowDirection.LeftToRight, Padding = new Padding(0, 4, 0, 0) };
            buttons.Controls.AddRange(new Control[] { new FlatButton("Start") { Kind = "primary", Enabled = false }, new FlatButton("Stop"), new FlatButton("Restart"), new FlatButton("Kill") { Kind = "danger", Margin = new Padding(0) } });
            top.Controls.Add(buttons); top.Controls.Add(new StateLabel { Dock = DockStyle.Left, Width = 128 });
            page.Controls.Add(top, 0, 0);

            var stats = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, Padding = new Padding(0, 0, 0, 8) };
            stats.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 30)); stats.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 70));
            var list = new StatsList(); UI.Tips.SetToolTip(list, "Uptime is the time since the server started. Speed is how many game steps (ticks) the server runs each second. About 60 is normal. Players does not count bots.");
            stats.Controls.Add(list, 0, 0);
            var chart = new AreaChart(); UI.Tips.SetToolTip(chart, "The speed of the server in each of the last 20 minutes."); stats.Controls.Add(chart, 1, 0);
            page.Controls.Add(stats, 0, 1);

            var pills = new FlowLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(0, 6, 0, 0) };
            var pc = new PillTab("Console") { Selected = true }; var pe = new PillTab("Events");
            pc.Click += delegate { ShowView(0); }; pe.Click += delegate { ShowView(1); };
            viewTabs.Add(pc); viewTabs.Add(pe); pills.Controls.Add(pc); pills.Controls.Add(pe);
            page.Controls.Add(pills, 0, 2);

            console = new RichTextBox { Dock = DockStyle.Fill, ReadOnly = true, BorderStyle = BorderStyle.None, Font = new Font("Consolas", 9.5f), Padding = new Padding(8) };
            var events = new EventList();
            var area = new Panel { Dock = DockStyle.Fill };
            area.Controls.Add(console); area.Controls.Add(events); events.Visible = false;
            viewPages.Add(console); viewPages.Add(events);
            page.Controls.Add(area, 0, 3);

            var cmd = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, Padding = new Padding(0, 8, 0, 0) };
            cmd.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); cmd.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 88)); cmd.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 112));
            var box = new TextBox { BorderStyle = BorderStyle.FixedSingle, Anchor = AnchorStyles.Left | AnchorStyles.Right, Margin = new Padding(0, 0, 8, 0) };
            cmd.Controls.Add(box, 0, 0); cmd.Controls.Add(new FlatButton("Send") { Anchor = AnchorStyles.Left }, 1, 0); cmd.Controls.Add(new FlatButton("Save Log") { Anchor = AnchorStyles.Left, Margin = new Padding(0) }, 2, 0);
            page.Controls.Add(cmd, 0, 4);
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

        // ---- settings with sub-tabs
        Control SettingsHost()
        {
            var host = new TableLayoutPanel { ColumnCount = 1, RowCount = 2 };
            host.RowStyles.Add(new RowStyle(SizeType.Absolute, 42)); host.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            var pills = new FlowLayoutPanel { Dock = DockStyle.Fill, Padding = new Padding(0, 4, 0, 0) };
            string[] names = { "Server Settings", "Launcher Settings", "Advanced Settings" };
            var area = new Panel { Dock = DockStyle.Fill };
            Control[] subs = { ServerSettings(), LauncherSettings(), AdvancedSettings() };
            for (int i = 0; i < 3; i++)
            {
                var p = new PillTab(names[i]) { Selected = i == 0 }; int index = i; p.Click += delegate { ShowSub(index); };
                subTabs.Add(p); pills.Controls.Add(p); subs[i].Dock = DockStyle.Fill; subs[i].Visible = i == 0; area.Controls.Add(subs[i]); subPages.Add(subs[i]);
            }
            host.Controls.Add(pills, 0, 0); host.Controls.Add(area, 0, 1);
            return host;
        }

        Control ServerSettings()
        {
            var page = new Panel { AutoScroll = true, Padding = new Padding(0, 8, 0, 0) }; var t = RowTable(); page.Controls.Add(t);
            var pw = TB("secret123", 220); pw.UseSystemPasswordChar = true;
            AddRow(t, "Server name", null, TB("SubVulpes test", 300), null, true);
            AddRow(t, "Admin password", "In the game, type /admin and this password.", Inline(pw, new FlatButton("Show") { Margin = new Padding(8, 0, 0, 0) }), null, true);
            AddRow(t, "Port", "Open this UDP port in the router.", TB("27584", 100), "Applies at the next start", true);
            AddRow(t, "Max players", null, TB("16", 80), "Applies at the next start", true);
            AddRow(t, "Game mode", null, Combo(160, "Round", "World"), "Applies at the next start", true);
            AddRow(t, "Round time", "Minutes in Round mode.", TB("7", 80), null, true);
            AddRow(t, "Upload limit", "Bytes per second for all the players.", TB("100000", 120), null, true);
            AddRow(t, "Welcome text", "The newspaper (readme.txt).", new TextBox { Text = "Welcome to the test server.", Multiline = true, Height = 54, Width = 380, BorderStyle = BorderStyle.FixedSingle }, null, true);
            return page;
        }

        Control LauncherSettings()
        {
            var page = new Panel { AutoScroll = true, Padding = new Padding(0, 8, 0, 0) }; var t = RowTable(); page.Controls.Add(t);
            AddRow(t, "Server folder", null, Inline(TB("C:\\SubVulpes-server", 320), new FlatButton("Browse…") { Margin = new Padding(8, 0, 0, 0) }, new FlatButton("Open Folder") { Margin = new Padding(8, 0, 0, 0) }), null, false);
            AddRow(t, "Updates", "Looks for a newer version on GitHub.", Inline(new FlatButton("Check for Updates"), new FlatButton("Update from .ZIP") { Margin = new Padding(0) }), null, false);
            AddRow(t, "Include Beta Versions", "Includes beta versions. They can still contain bugs.", new Switch(), null, true);
            var theme = Combo(160, "Light", "Dark"); theme.SelectedIndexChanged += delegate { SetTheme(theme.SelectedIndex == 1 ? Theme.Dark : Theme.Light); };
            AddRow(t, "Theme", null, theme, null, true);
            return page;
        }

        Control AdvancedSettings()
        {
            var page = new Panel { AutoScroll = true, Padding = new Padding(0, 8, 0, 0) }; var t = RowTable(); page.Controls.Add(t);
            AddRow(t, "Capture mode", "Records the server speed. Saves a small memory dump if the server crashes or freezes.", Combo(180, "Normal", "Speed Check", "Network Check", "Custom"), null, true);
            var dumpLabel = L("Save a memory dump:", false, true); dumpLabel.Margin = new Padding(0, 4, 12, 0);
            var dump = Inline(dumpLabel, new RadioButton { Text = "None", AutoSize = true, Margin = new Padding(0, 0, 12, 0) }, new RadioButton { Text = "Small (about 1 MB)", AutoSize = true, Checked = true, Margin = new Padding(0, 0, 12, 0) }, new RadioButton { Text = "Full (about 600 MB)", AutoSize = true, Margin = new Padding(0) });
            AddRow(t, "If the server crashes or freezes", null, dump, null, true);
            AddRow(t, "Speed Profile", "Finds slow functions.", new Switch { On = true }, null, true);
            AddRow(t, "Game Timers", "Writes the time of each part of the game, once each minute.", new Switch(), null, true);
            AddRow(t, "Network Packet Capture", "Records network traffic, including player names and chat.", new Switch(), null, true);
            AddRow(t, "Memory dump", "A copy of the server memory. Send it to the developers to find a problem.", new FlatButton("Save Memory Dump Now") { Margin = new Padding(0) }, null, false);
            return page;
        }

        // ---- plugins
        Control PluginsPage()
        {
            var page = new Panel();
            var bar = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 44, FlowDirection = FlowDirection.RightToLeft, Padding = new Padding(0, 4, 0, 0) };
            bar.Controls.Add(new FlatButton("Refresh") { Margin = new Padding(0) }); bar.Controls.Add(new FlatButton("Install plugin…"));
            detail = new Panel { Dock = DockStyle.Right, Width = 380, Padding = new Padding(16), Tag = "panel", AutoScroll = true };
            var sp = new Panel { Dock = DockStyle.Right, Width = 12 };
            var drop = new Label { Dock = DockStyle.Bottom, Height = 54, Text = "Drop a .svplugin file here to install it", TextAlign = ContentAlignment.MiddleCenter, Tag = "muted" };
            grid = new DataGridView
            {
                Dock = DockStyle.Fill, ReadOnly = true, AllowUserToAddRows = false, RowHeadersVisible = false, MultiSelect = false,
                SelectionMode = DataGridViewSelectionMode.FullRowSelect, BorderStyle = BorderStyle.None, CellBorderStyle = DataGridViewCellBorderStyle.SingleHorizontal,
                EnableHeadersVisualStyles = false, ColumnHeadersBorderStyle = DataGridViewHeaderBorderStyle.None, AllowUserToResizeRows = false, ColumnHeadersHeight = 36,
                ColumnHeadersHeightSizeMode = DataGridViewColumnHeadersHeightSizeMode.DisableResizing, AutoSizeColumnsMode = DataGridViewAutoSizeColumnsMode.Fill
            };
            grid.RowTemplate.Height = 40;
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
            var title = L(p.Name, true, false); title.Font = UI.F(13f, FontStyle.Bold); title.Margin = new Padding(0, 0, 0, 2); flow.Controls.Add(title);
            var sub = L(p.Version + " · " + p.By, false, true); sub.Margin = new Padding(0, 0, 0, 10); flow.Controls.Add(sub);
            var d = L(p.Desc, false, false); d.MaximumSize = new Size(330, 0); d.Margin = new Padding(0, 0, 0, 12); flow.Controls.Add(d);
            if (p.Needs != null) { var nl = L("Needs", true, true); nl.Margin = new Padding(0, 0, 0, 4); flow.Controls.Add(nl); var c = Chip(p.Needs); c.Margin = new Padding(0, 0, 0, 12); flow.Controls.Add(c); }
            var cl = L("Capabilities", true, true); cl.Margin = new Padding(0, 0, 0, 4); flow.Controls.Add(cl);
            var caps = new FlowLayoutPanel { AutoSize = true, MaximumSize = new Size(334, 0), Tag = "panel", Margin = new Padding(0, 0, 0, 12) };
            foreach (var c in p.Caps) caps.Controls.Add(Chip(c));
            flow.Controls.Add(caps);
            if (p.Risk == "highest")
            {
                var n = L("This plugin has full access to the server. It can read private data, freeze it or crash it.", false, false);
                n.MaximumSize = new Size(330, 0); n.Tag = "warn"; n.Margin = new Padding(0, 0, 0, 12); flow.Controls.Add(n);
            }
            if (p.Settings.Length > 0)
            {
                var sl = L("Settings", true, true); sl.Margin = new Padding(0, 0, 0, 4); flow.Controls.Add(sl);
                var tbl = new TableLayoutPanel { AutoSize = true, ColumnCount = 2, Tag = "panel", Margin = new Padding(0, 0, 0, 12) };
                tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 140)); tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 190));
                foreach (var s in p.Settings)
                {
                    int row = tbl.RowCount++; tbl.RowStyles.Add(new RowStyle(SizeType.Absolute, 36));
                    var lab = L(s[0], false, false); lab.Anchor = AnchorStyles.Left; tbl.Controls.Add(lab, 0, row);
                    var tb = TB(s[1], 180); tbl.Controls.Add(tb, 1, row);
                }
                flow.Controls.Add(tbl);
            }
            var acts = new FlowLayoutPanel { AutoSize = true, Tag = "panel", Margin = new Padding(0, 4, 0, 0) };
            acts.Controls.Add(new FlatButton("Update…")); acts.Controls.Add(new FlatButton("Remove") { Kind = "danger", Margin = new Padding(0) });
            flow.Controls.Add(acts);
            detail.Controls.Add(flow);
            Apply(detail);
            detail.ResumeLayout();
        }

        static Label Chip(string text)
        {
            var l = new Label { Text = text, AutoSize = true, Padding = new Padding(8, 3, 8, 3), Margin = new Padding(0, 0, 6, 6), Font = UI.F(9f, FontStyle.Regular), Tag = "chip" };
            l.BackColor = UI.T.Chip; l.ForeColor = UI.T.Fg; return l;
        }

        // ---- help
        Control HelpPage()
        {
            var page = new Panel { AutoScroll = true };
            var t = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 1 };
            string[][] topics = {
                new[] { "Speed (ticks/s)", "How many game steps (ticks) the server runs each second. About 60 is normal. A lower number means that the server is slow." },
                new[] { "Stop and Kill", "Stop asks the server to save and close. Kill ends the server at once. Use Kill only if Stop does not work." },
                new[] { "Applies at the next start", "The setting is saved. The server uses it only after the next start." },
                new[] { "Events and Console", "The console shows every line that the server writes. Events show the main things that happened, such as a start, a plugin that needs a restart, or a slow minute." },
                new[] { "Capture mode", "Chooses what the launcher records while the server runs. Normal records the speed and saves a small memory dump if the server crashes or freezes." },
                new[] { "Memory dump", "A file with a copy of the server memory. Send it to the developers to find the cause of a crash or a freeze." },
                new[] { "Plugin risk", "Low: reads data. High: changes the game. Highest: runs inside the server with full access to it. Only turn on plugins that you trust." },
                new[] { "Logs", "Each time that you start the server, the launcher saves a log folder. You can find old logs in the Server tab, under Past Logs." } };
            foreach (var topic in topics)
            {
                var card = new Panel { Dock = DockStyle.Top, AutoSize = true, Padding = new Padding(14, 10, 14, 10), Margin = new Padding(0, 0, 0, 10), Tag = "panel", MinimumSize = new Size(0, 60) };
                var inner = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false, Tag = "panel" };
                var h = L(topic[0], true, false); h.Margin = new Padding(0, 0, 0, 4); inner.Controls.Add(h);
                var b = L(topic[1], false, true); b.MaximumSize = new Size(820, 0); inner.Controls.Add(b);
                card.Controls.Add(inner); t.Controls.Add(card);
            }
            page.Controls.Add(t);
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
                    var timer = new System.Windows.Forms.Timer { Interval = 400 };
                    timer.Tick += delegate
                    {
                        timer.Stop();
                        Theme[] themes = { Theme.Light, Theme.Dark }; string[] tn = { "light", "dark" };
                        for (int t = 0; t < 2; t++) foreach (string name in MainForm.Names)
                        {
                            form.SetTheme(themes[t]); form.Go(name); Application.DoEvents(); Thread.Sleep(350); Application.DoEvents();
                            using (var bmp = new Bitmap(form.Width, form.Height))
                            using (var g = Graphics.FromImage(bmp))
                            { g.CopyFromScreen(form.Location, Point.Empty, form.Size); bmp.Save(Path.Combine(dir, tn[t] + "-" + name + ".png")); }
                        }
                        Application.Exit();
                    };
                    timer.Start();
                };
            }
            Application.Run(form);
        }
    }
}
