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
        public Color Bg, Win, Panel, Line, Fg, Muted, Accent, Ok, Warn, Bad, Chip, Console, ConsoleFg, Bar, BarFg, BarMuted, SubBar;
        public static readonly Theme Light = new Theme {
            Bg = Color.FromArgb(238, 240, 243), Win = Color.FromArgb(246, 247, 249), Panel = Color.White, Line = Color.FromArgb(217, 220, 225),
            Fg = Color.FromArgb(27, 29, 33), Muted = Color.FromArgb(106, 112, 122), Accent = Color.FromArgb(217, 99, 30),
            Ok = Color.FromArgb(42, 138, 74), Warn = Color.FromArgb(183, 121, 31), Bad = Color.FromArgb(194, 54, 43),
            Chip = Color.FromArgb(236, 239, 243), Console = Color.Black, ConsoleFg = Color.FromArgb(212, 215, 220),
            Bar = Color.FromArgb(222, 226, 232), SubBar = Color.FromArgb(238, 240, 244), BarFg = Color.White, BarMuted = Color.FromArgb(96, 103, 115) };
        public static readonly Theme Dark = new Theme {
            Bg = Color.FromArgb(15, 17, 20), Win = Color.FromArgb(26, 29, 34), Panel = Color.FromArgb(33, 37, 43), Line = Color.FromArgb(50, 56, 65),
            Fg = Color.FromArgb(232, 234, 238), Muted = Color.FromArgb(154, 161, 172), Accent = Color.FromArgb(240, 138, 75),
            Ok = Color.FromArgb(94, 194, 125), Warn = Color.FromArgb(224, 176, 80), Bad = Color.FromArgb(239, 111, 100),
            Chip = Color.FromArgb(43, 48, 56), Console = Color.Black, ConsoleFg = Color.FromArgb(201, 204, 210),
            Bar = Color.FromArgb(40, 45, 53), SubBar = Color.FromArgb(31, 35, 41), BarFg = Color.White, BarMuted = Color.FromArgb(154, 161, 172) };
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

    static class Icons
    {
        public static Color Blend(Color back, Color front, double amount)
        {
            return Color.FromArgb((int)(back.R + (front.R - back.R) * amount), (int)(back.G + (front.G - back.G) * amount), (int)(back.B + (front.B - back.B) * amount));
        }

        // Draws a small icon in a 16 by 16 box at x, y.
        public static void Draw(Graphics g, string name, int x, int y, Color c)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            using (var pen = new Pen(c, 1.6f) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round }) using (var b = new SolidBrush(c))
            {
                switch (name)
                {
                    case "play": g.FillPolygon(b, new[] { new Point(x + 4, y + 2), new Point(x + 4, y + 14), new Point(x + 14, y + 8) }); break;
                    case "stop": using (var path = UI.Round(new Rectangle(x + 3, y + 3, 10, 10), 2)) g.FillPath(b, path); break;
                    case "bolt": g.FillPolygon(b, new[] { new PointF(x + 9, y + 1), new PointF(x + 3, y + 9), new PointF(x + 7.5f, y + 9), new PointF(x + 6.5f, y + 15), new PointF(x + 13, y + 6.5f), new PointF(x + 8.5f, y + 6.5f) }); break;
                    case "restart": case "refresh":
                        g.DrawArc(pen, x + 3, y + 3, 10, 10, -40, 290);
                        g.DrawLines(pen, new[] { new Point(x + 12, y + 1), new Point(x + 12, y + 5), new Point(x + 8, y + 5) }); break;
                    case "download":
                        g.DrawLine(pen, x + 8, y + 2, x + 8, y + 10); g.DrawLines(pen, new[] { new Point(x + 4, y + 7), new Point(x + 8, y + 11), new Point(x + 12, y + 7) });
                        g.DrawLines(pen, new[] { new Point(x + 3, y + 11), new Point(x + 3, y + 14), new Point(x + 13, y + 14), new Point(x + 13, y + 11) }); break;
                }
            }
        }
    }

    class Segmented : Control
    {
        public string[] Options; int sel; int[] widths;
        public event EventHandler Changed;
        public Segmented(params string[] options)
        {
            Options = options; Height = 32; Cursor = Cursors.Hand; Margin = new Padding(0); SetStyle(UI.Paint, true);
            widths = new int[options.Length]; int total = 0;
            for (int i = 0; i < options.Length; i++) { widths[i] = TextRenderer.MeasureText(options[i], UI.F(9.5f, FontStyle.Bold)).Width + 20; total += widths[i]; }
            Width = total + 4; sel = 1 < options.Length ? 1 : 0;
        }
        protected override void OnMouseClick(MouseEventArgs e)
        {
            int x = 2;
            for (int i = 0; i < Options.Length; i++) { if (e.X >= x && e.X < x + widths[i]) { if (sel != i) { sel = i; Invalidate(); if (Changed != null) Changed(this, EventArgs.Empty); } break; } x += widths[i]; }
            base.OnMouseClick(e);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            using (var path = UI.Round(new Rectangle(0, 0, Width - 1, Height - 1), 7)) using (var b = new SolidBrush(UI.T.Chip)) using (var p = new Pen(UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
            int x = 2;
            for (int i = 0; i < Options.Length; i++)
            {
                var r = new Rectangle(x, 2, widths[i], Height - 5);
                if (i == sel) using (var path = UI.Round(r, 5)) using (var b = new SolidBrush(UI.T.Panel)) using (var p = new Pen(UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
                TextRenderer.DrawText(g, Options[i], i == sel ? UI.F(9.5f, FontStyle.Bold) : UI.F(9.5f, FontStyle.Regular), r, i == sel ? UI.T.Accent : UI.T.Muted, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
                x += widths[i];
            }
        }
    }

    class FlatButton : Control
    {
        public string Kind = "normal";
        bool hover; string glyph;
        public string Glyph
        {
            get { return glyph; }
            set { glyph = value; Width = TextRenderer.MeasureText(Text, Font).Width + 32 + (value != null ? 22 : 0); Invalidate(); }
        }
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
            if (glyph == null) { TextRenderer.DrawText(g, Text, Font, r, fg, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter); return; }
            int tw = TextRenderer.MeasureText(g, Text, Font, new Size(999, 40), TextFormatFlags.NoPadding).Width, start = (Width - (16 + 6 + tw)) / 2;
            Icons.Draw(g, glyph, start, (Height - 16) / 2, fg);
            TextRenderer.DrawText(g, Text, Font, new Rectangle(start + 22, 0, tw + 4, Height), fg, TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
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
        public string Title = "", Value = "", Unit = ""; public bool Flat;
        public Card(string title, string value, string unit)
        {
            Title = title; Value = value; Unit = unit; Dock = DockStyle.Fill; Margin = new Padding(0, 0, 10, 0); SetStyle(UI.Paint, true);
        }
        protected void Frame(Graphics g, int w, int h, Control parent)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(parent.BackColor);
            if (Flat) return;
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
        public string[][] All = {
            new[] { "Uptime", "00:05:39" }, new[] { "Tick rate", "59.6 ticks/s" }, new[] { "Players", "1" }, new[] { "Peak players today", "3" },
            new[] { "Round", "3 · 4:12 left" }, new[] { "Network out", "18 KB/s" }, new[] { "Errors (last hour)", "1" }, new[] { "Warnings (last hour)", "2" },
            new[] { "Last crash", "None" }, new[] { "Started at", "20:50" }, new[] { "Server address", "192.168.0.10:27584" } };
        public bool[] On = { true, true, true, false, true, false, true, false, false, false, false };
        public StatsList() : base("", "", "") { }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; Frame(g, Width, Height, Parent);
            var items = new List<string[]>(); for (int i = 0; i < All.Length; i++) if (On[i]) items.Add(All[i]);
            if (items.Count == 0) return;
            int rowH = (Height - 8) / items.Count;
            for (int i = 0; i < items.Count; i++)
            {
                int y = 4 + i * rowH;
                // The label gives way to the value: it is cut with "..." when the two would touch.
                var vf = UI.F(12f, FontStyle.Bold); int vw = TextRenderer.MeasureText(g, items[i][1], vf, new Size(999, 40), TextFormatFlags.NoPadding).Width;
                TextRenderer.DrawText(g, items[i][0], UI.F(9.5f, FontStyle.Regular), new Rectangle(16, y, Math.Max(20, Width - 52 - vw - 12), rowH), UI.T.Muted, TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPadding);
                TextRenderer.DrawText(g, items[i][1], vf, new Rectangle(16, y, Width - 52, rowH), UI.T.Fg, TextFormatFlags.Right | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
                if (i < items.Count - 1) using (var pen = new Pen(Color.FromArgb(110, UI.T.Line))) g.DrawLine(pen, 14, y + rowH, Width - 16, y + rowH);
            }
        }
    }

    class GearButton : Control
    {
        bool hot;
        public GearButton() { Size = new Size(24, 24); Cursor = Cursors.Hand; SetStyle(UI.Paint, true); }
        protected override void OnMouseEnter(EventArgs e) { hot = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hot = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(UI.T.Panel);
            Color c = hot ? UI.T.Accent : UI.T.Muted;
            using (var pen = new Pen(c, 1.6f)) using (var fill = new SolidBrush(UI.T.Panel))
            {
                int[] ys = { 6, 12, 18 }; int[] xs = { 8, 15, 10 };
                for (int i = 0; i < 3; i++) { g.DrawLine(pen, 4, ys[i], 20, ys[i]); g.FillEllipse(fill, xs[i] - 2.5f, ys[i] - 2.5f, 5, 5); g.DrawEllipse(pen, xs[i] - 2.5f, ys[i] - 2.5f, 5, 5); }
            }
        }
    }

    class DropColors : ProfessionalColorTable
    {
        public override Color ToolStripDropDownBackground { get { return UI.T.Panel; } }
        public override Color ImageMarginGradientBegin { get { return UI.T.Panel; } }
        public override Color ImageMarginGradientMiddle { get { return UI.T.Panel; } }
        public override Color ImageMarginGradientEnd { get { return UI.T.Panel; } }
        public override Color MenuBorder { get { return UI.T.Line; } }
        public override Color MenuItemBorder { get { return UI.T.Chip; } }
        public override Color MenuItemSelected { get { return UI.T.Chip; } }
        public override Color MenuItemSelectedGradientBegin { get { return UI.T.Chip; } }
        public override Color MenuItemSelectedGradientEnd { get { return UI.T.Chip; } }
        public override Color CheckBackground { get { return UI.T.Chip; } }
        public override Color CheckSelectedBackground { get { return UI.T.Chip; } }
        public override Color CheckPressedBackground { get { return UI.T.Chip; } }
        public override Color ButtonSelectedBorder { get { return UI.T.Chip; } }
    }

    class Dropdown : Control
    {
        public readonly List<object> Items = new List<object>();
        int sel = -1; bool hot, open;
        public event EventHandler SelectedIndexChanged;
        public Dropdown(int width) { Width = width; Height = 28; Margin = new Padding(0); Cursor = Cursors.Hand; Font = UI.F(9.5f, FontStyle.Regular); SetStyle(UI.Paint, true); }
        public int SelectedIndex
        {
            get { return sel; }
            set { if (sel == value) return; sel = value; Invalidate(); if (SelectedIndexChanged != null) SelectedIndexChanged(this, EventArgs.Empty); }
        }
        public static ContextMenuStrip Menu()
        {
            return new ContextMenuStrip { ShowImageMargin = false, Font = UI.F(9.5f, FontStyle.Regular), BackColor = UI.T.Panel, Renderer = new ToolStripProfessionalRenderer(new DropColors()) { RoundedEdges = false } };
        }
        protected override void OnMouseEnter(EventArgs e) { hot = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hot = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnClick(EventArgs e)
        {
            var menu = Menu(); menu.MinimumSize = new Size(Width, 0);
            for (int i = 0; i < Items.Count; i++)
            {
                int idx = i;
                var it = new ToolStripMenuItem(Items[i].ToString()) { ForeColor = i == sel ? UI.T.Accent : UI.T.Fg, Padding = new Padding(2, 5, 2, 5) };
                if (i == sel) it.Font = UI.F(9.5f, FontStyle.Bold);
                it.Click += delegate { SelectedIndex = idx; };
                menu.Items.Add(it);
            }
            menu.Closed += delegate { open = false; Invalidate(); };
            open = true; Invalidate(); menu.Show(this, new Point(0, Height + 2));
            base.OnClick(e);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            var r = new Rectangle(0, 0, Width - 1, Height - 1);
            using (var path = UI.Round(r, 6)) using (var b = new SolidBrush(UI.T.Panel)) using (var p = new Pen(hot || open ? UI.T.Accent : UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
            string text = sel >= 0 && sel < Items.Count ? Items[sel].ToString() : "";
            TextRenderer.DrawText(g, text, Font, new Rectangle(10, 0, Width - 32, Height), UI.T.Fg, TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPadding);
            using (var pen = new Pen(UI.T.Muted, 1.6f) { StartCap = LineCap.Round, EndCap = LineCap.Round })
                g.DrawLines(pen, new[] { new Point(Width - 20, Height / 2 - 2), new Point(Width - 15, Height / 2 + 3), new Point(Width - 10, Height / 2 - 2) });
        }
    }

    class NavItem : Control
    {
        public bool Selected;
        public NavItem(string text) { Text = text; Height = 36; Dock = DockStyle.Top; Cursor = Cursors.Hand; SetStyle(UI.Paint, true); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            if (Selected)
            {
                using (var path = UI.Round(new Rectangle(0, 2, Width - 1, Height - 5), 6)) using (var b = new SolidBrush(UI.T.Chip)) g.FillPath(b, path);
                using (var b = new SolidBrush(UI.T.Accent)) g.FillRectangle(b, 0, 9, 3, Height - 18);
            }
            TextRenderer.DrawText(g, Text, Selected ? UI.F(9.5f, FontStyle.Bold) : UI.F(9.5f, FontStyle.Regular), new Rectangle(14, 0, Width - 20, Height), Selected ? UI.T.Fg : UI.T.Muted, TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
        }
    }

    class AreaChart : Card
    {
        public double[] Values = new double[] { 61.8, 60.9, 61.2, 60.2, 59.7, 60.5, 61.0, 60.1, 59.9, 60.6, 60.4, 59.8, 60.9, 61.2, 60.3, 59.6, 59.9, 60.2, 59.8, 59.6 };
        public AreaChart() : base("Tick rate", "", "") { Margin = new Padding(0); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; Frame(g, Width, Height, Parent);
            if (Flat) using (var pen = new Pen(UI.T.Line)) g.DrawLine(pen, 0, 6, 0, Height - 10);
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
            using (var p = new Pen(UI.T.Muted, 1.4f)) g.DrawEllipse(p, 2, 2, Width - 5, Height - 5);
            using (var f = new Font("Georgia", 9f, FontStyle.Bold | FontStyle.Italic, GraphicsUnit.Point))
                TextRenderer.DrawText(g, "i", f, new Rectangle(0, 0, Width, Height), UI.T.Muted, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
        }
    }

    class TabLabel : Control
    {
        public bool Selected, Sub;
        public TabLabel(string text)
        {
            Text = text; Font = UI.F(10f, FontStyle.Regular); Cursor = Cursors.Hand; Dock = DockStyle.Left;
            Width = TextRenderer.MeasureText(text, UI.F(10f, FontStyle.Bold)).Width + 40; SetStyle(UI.Paint, true);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.Clear(Parent.BackColor);
            int line = Sub ? 2 : 3;
            TextRenderer.DrawText(g, Text, Selected ? UI.F(10f, FontStyle.Bold) : Font, new Rectangle(0, 0, Width, Height - line),
                Selected ? UI.T.Fg : UI.T.BarMuted, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
            if (Selected) using (var b = new SolidBrush(Sub ? Color.FromArgb(150, UI.T.Accent) : UI.T.Accent)) g.FillRectangle(b, 12, Height - line, Width - 24, line);
        }
    }

    class Grip : Control
    {
        public event Action<int> Dragged; int startY; bool down;
        public Grip() { Height = 9; Dock = DockStyle.Bottom; Cursor = Cursors.SizeNS; SetStyle(UI.Paint, true); }
        protected override void OnMouseDown(MouseEventArgs e) { down = true; startY = Cursor.Position.Y; base.OnMouseDown(e); }
        protected override void OnMouseUp(MouseEventArgs e) { down = false; base.OnMouseUp(e); }
        protected override void OnMouseMove(MouseEventArgs e)
        {
            if (down && Dragged != null) { int y = Cursor.Position.Y; Dragged(y - startY); startY = y; }
            base.OnMouseMove(e);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.Clear(Parent.BackColor);
            using (var p = new Pen(UI.T.Line)) e.Graphics.DrawLine(p, 0, Height - 1, Width, Height - 1);
        }
    }

    class IconButton : Control
    {
        public string Glyph; bool hot;
        public IconButton(string glyph) { Glyph = glyph; Size = new Size(28, 28); Cursor = Cursors.Hand; SetStyle(UI.Paint, true); }
        protected override void OnMouseEnter(EventArgs e) { hot = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hot = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(UI.T.Console);
            var r = new Rectangle(0, 0, Width - 1, Height - 1);
            using (var path = UI.Round(r, 5)) using (var b = new SolidBrush(Color.FromArgb(hot ? 55 : 20, 255, 255, 255))) using (var p = new Pen(Color.FromArgb(hot ? 110 : 40, 255, 255, 255))) { g.FillPath(b, path); g.DrawPath(p, path); }
            using (var pen = new Pen(Color.FromArgb(hot ? 255 : 120, UI.T.ConsoleFg), 1.6f) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round })
            {
                if (Glyph == "save")
                {
                    g.DrawLine(pen, 14, 7, 14, 17); g.DrawLines(pen, new[] { new Point(10, 13), new Point(14, 17), new Point(18, 13) });
                    g.DrawLines(pen, new[] { new Point(8, 18), new Point(8, 21), new Point(20, 21), new Point(20, 18) });
                }
                else if (Glyph == "expand" || Glyph == "collapse")
                {
                    bool o = Glyph == "expand";
                    g.DrawLines(pen, o ? new[] { new Point(8, 13), new Point(8, 8), new Point(13, 8) } : new[] { new Point(8, 13), new Point(13, 13), new Point(13, 8) });
                    g.DrawLines(pen, o ? new[] { new Point(15, 8), new Point(20, 8), new Point(20, 13) } : new[] { new Point(15, 8), new Point(15, 13), new Point(20, 13) });
                    g.DrawLines(pen, o ? new[] { new Point(8, 15), new Point(8, 20), new Point(13, 20) } : new[] { new Point(8, 15), new Point(13, 15), new Point(13, 20) });
                    g.DrawLines(pen, o ? new[] { new Point(20, 15), new Point(20, 20), new Point(15, 20) } : new[] { new Point(15, 20), new Point(15, 15), new Point(20, 15) });
                }
                else if (Glyph == "clear")
                {
                    g.DrawLine(pen, 8, 9, 20, 9); g.DrawLine(pen, 12, 9, 12, 7); g.DrawLine(pen, 12, 7, 16, 7); g.DrawLine(pen, 16, 7, 16, 9);
                    g.DrawLines(pen, new[] { new Point(10, 11), new Point(11, 21), new Point(17, 21), new Point(18, 11) });
                }
                else
                {
                    g.DrawLines(pen, new[] { new Point(8, 17), new Point(8, 7), new Point(17, 7), new Point(17, 10) });
                    g.DrawRectangle(pen, 11, 11, 9, 10);
                }
            }
        }
    }

    class ServerName : Control
    {
        public ServerName(string text) { Text = text; Dock = DockStyle.Right; Width = TextRenderer.MeasureText(text, UI.F(10f, FontStyle.Bold)).Width + 28; SetStyle(UI.Paint, true); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.Clear(Parent.BackColor);
            TextRenderer.DrawText(g, Text, UI.F(10f, FontStyle.Bold), new Rectangle(0, 0, Width - 8, Height), UI.T.BarMuted, TextFormatFlags.Right | TextFormatFlags.VerticalCenter);
        }
    }

    class StateLabel : Control
    {
        public string Word = "Running";
        public StateLabel() { Height = 34; Width = 30 + TextRenderer.MeasureText("Running", UI.F(10.5f, FontStyle.Bold), new Size(999, 40), TextFormatFlags.NoPadding).Width + 14; SetStyle(UI.Paint, true); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            var r = new Rectangle(0, 0, Width - 1, Height - 1);
            using (var path = UI.Round(r, r.Height / 2)) using (var b = new SolidBrush(Color.FromArgb(38, UI.T.Ok))) g.FillPath(b, path);
            using (var b = new SolidBrush(UI.T.Ok)) g.FillEllipse(b, 13, r.Height / 2 - 5, 10, 10);
            TextRenderer.DrawText(g, Word, UI.F(10.5f, FontStyle.Bold), new Rectangle(30, 0, r.Width - 30, r.Height), UI.T.Ok, TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
        }
    }

    class Surface : Panel
    {
        public Surface() { SetStyle(UI.Paint, true); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            using (var path = UI.Round(new Rectangle(0, 0, Width - 1, Height - 1), 10)) using (var b = new SolidBrush(UI.T.Panel)) using (var p = new Pen(UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
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
        readonly List<TabLabel> subTabs = new List<TabLabel>();
        readonly List<Plugin> plugins = new List<Plugin>();
        Panel tabBar; RichTextBox console; DataGridView grid; Panel detail; int sel = 2;
        string levelFilter = "all"; TextBox searchBox; bool expanded; TableLayoutPanel serverPage; Control statsRow;
        const int StatsHeight = 184; int dashHeight = StatsHeight;

        public MainForm()
        {
            Text = "SubVulpes Server Launcher - v0.1.0"; Size = new Size(1040, 700); MinimumSize = new Size(900, 600);
            StartPosition = FormStartPosition.CenterScreen; Font = UI.F(9.5f, FontStyle.Regular); DoubleBuffered = true;
            KeyPreview = true; KeyDown += delegate (object o, KeyEventArgs ke) { if (ke.Control && ke.KeyCode == Keys.T) SetTheme(UI.T == Theme.Light ? Theme.Dark : Theme.Light); };
            LoadData();
            tabBar = new Panel { Dock = DockStyle.Top, Height = 46, Padding = new Padding(8, 0, 8, 0), Tag = "bar" };
            string[] names = { "Server", "Plugins", "Settings", "Help" };
            tabBar.Controls.Add(new ServerName("SubVulpes test"));
            for (int i = names.Length - 1; i >= 0; i--)
            {
                var t = new TabLabel(names[i]); int index = i;
                t.Click += delegate { ShowPage(index); };
                tabs.Insert(0, t); tabBar.Controls.Add(t);
            }
            pages.Add(ServerPage()); pages.Add(PluginsHost()); pages.Add(SettingsHost()); pages.Add(HelpPage());
            foreach (var p in pages) { p.Dock = DockStyle.Fill; p.Padding = new Padding(16, 12, 16, 10); Controls.Add(p); }
            pages[1].Padding = Padding.Empty; pages[2].Padding = Padding.Empty; pages[0].Padding = new Padding(16, 12, 16, 12);
            Controls.Add(tabBar);
            tabBar.SendToBack(); foreach (var p in pages) p.BringToFront();
            ShowPage(0); SetTheme(Theme.Light);
        }

        public void ShowPage(int i)
        {
            for (int k = 0; k < pages.Count; k++) { pages[k].Visible = k == i; tabs[k].Selected = k == i; tabs[k].Invalidate(); }
        }

        void ShowSub(int i) { for (int k = 0; k < subPages.Count; k++) { subPages[k].Visible = k == i; subTabs[k].Selected = k == i; subTabs[k].Invalidate(); } }

        public void Go(string what)
        {
            if (what == "server") ShowPage(0);
            else if (what == "settings-server") { ShowPage(2); ShowSub(0); }
            else if (what == "settings-launcher") { ShowPage(2); ShowSub(1); }
            else if (what == "settings-advanced") { ShowPage(2); ShowSub(2); }
            else if (what == "plugins") ShowPage(1);
            else if (what == "help") ShowPage(3);
        }
        public static string[] Names = { "server", "settings-server", "settings-launcher", "settings-advanced", "plugins", "help" };

        [DllImport("dwmapi.dll")] static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);

        public void SetTheme(Theme t)
        {
            UI.T = t; BackColor = t.Win;
            if (IsHandleCreated) { int dark = t == Theme.Dark ? 1 : 0; if (DwmSetWindowAttribute(Handle, 20, ref dark, 4) != 0) DwmSetWindowAttribute(Handle, 19, ref dark, 4); Size = new Size(Width + 1, Height); Size = new Size(Width - 1, Height); }
            tabBar.BackColor = t.Bar;
            Apply(this); FillConsole(); RenderDetail(); RenderHelp(); Invalidate(true);
        }

        void Apply(Control c)
        {
            if ((c.Tag as string) == "bar") { c.BackColor = UI.T.Bar; foreach (Control k in c.Controls) k.Invalidate(); return; }
            if ((c.Tag as string) == "subbar") { c.BackColor = UI.T.SubBar; foreach (Control k in c.Controls) k.Invalidate(); return; }
            if ((c.Tag as string) == "line") { c.BackColor = UI.T.Line; return; }
            if (c is TextBox) { c.BackColor = UI.T.Panel; c.ForeColor = (c.Tag as string) == "hint" && c.Text == "Search…" ? UI.T.Muted : UI.T.Fg; }
            else if (c is RichTextBox && (c.Tag as string) == "doc") { c.BackColor = UI.T.Panel; c.ForeColor = UI.T.Fg; }
            else if (c is RichTextBox) { c.BackColor = UI.T.Console; c.ForeColor = UI.T.ConsoleFg; }
            else if (c is DataGridView) StyleGrid((DataGridView)c);
            else if (c is Label && (c.Tag as string) == "restart") { c.BackColor = Icons.Blend(UI.T.Win, UI.T.Warn, 0.18); c.ForeColor = UI.T.Warn; }
            else if (c is Label && (c.Tag as string) == "chip") { c.BackColor = UI.T.Chip; c.ForeColor = UI.T.Fg; }
            else if (c is Label) { c.BackColor = Color.Transparent; c.ForeColor = (c.Tag as string) == "muted" ? UI.T.Muted : (c.Tag as string) == "ok" ? UI.T.Ok : (c.Tag as string) == "warn" ? UI.T.Warn : UI.T.Fg; }
            else if (c is Panel && (c.Tag as string) == "consolebg") c.BackColor = UI.T.Console;
            else if (c is FlowLayoutPanel || c is Panel || c is TableLayoutPanel) c.BackColor = (c.Tag as string) == "panel" ? UI.T.Panel : UI.T.Win;
            else if (c is RadioButton) { c.BackColor = Color.Transparent; c.ForeColor = UI.T.Fg; }
            ThemeScroll(c);
            foreach (Control k in c.Controls) Apply(k);
            c.Invalidate();
        }

        [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)] static extern int SetWindowTheme(IntPtr hwnd, string app, string idlist);
        readonly HashSet<Control> scrollHooked = new HashSet<Control>();

        // Thin modern scrollbars of Windows 10, in dark mode too.
        void ThemeScroll(Control c)
        {
            bool scrolls = c is RichTextBox || c is DataGridView || (c is Panel && ((Panel)c).AutoScroll);
            if (!scrolls) return;
            if (scrollHooked.Add(c)) c.HandleCreated += delegate { SetWindowTheme(c.Handle, UI.T == Theme.Dark ? "DarkMode_Explorer" : "Explorer", null); };
            if (c.IsHandleCreated) SetWindowTheme(c.Handle, UI.T == Theme.Dark ? "DarkMode_Explorer" : "Explorer", null);
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
            if (hint != null)
            {
                var h2 = L("↻ " + hint, false, false); h2.Tag = "restart"; h2.Padding = new Padding(8, 3, 8, 3); h2.Font = UI.F(9f, FontStyle.Regular); h2.Margin = new Padding(0, 0, 10, 0);
                right.Controls.Add(h2);
            }
            right.Controls.Add(saved); t.Controls.Add(right, 3, row);
            if (autosave)
            {
                var timer = new System.Windows.Forms.Timer { Interval = 1600 };
                timer.Tick += delegate { saved.Visible = false; timer.Stop(); };
                EventHandler show = delegate { saved.Visible = true; timer.Stop(); timer.Start(); };
                if (input is TextBox) ((TextBox)input).TextChanged += show;
                else if (input is Dropdown) ((Dropdown)input).SelectedIndexChanged += show;
                else if (input is Switch) ((Switch)input).Changed += show;
                else if (input is Segmented) ((Segmented)input).Changed += show;
            }
        }

        static TextBox HintBox(int width)
        {
            var box = new TextBox { Text = "Search…", BorderStyle = BorderStyle.FixedSingle, Width = width, Anchor = AnchorStyles.Left, Tag = "hint" };
            box.GotFocus += delegate { if (box.Text == "Search…") { box.Text = ""; box.ForeColor = UI.T.Fg; } };
            box.LostFocus += delegate { if (box.Text.Length == 0) { box.Text = "Search…"; box.ForeColor = UI.T.Muted; } };
            return box;
        }

        static FlowLayoutPanel Inline(params Control[] items)
        {
            var f = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Margin = new Padding(0) };
            foreach (var c in items) f.Controls.Add(c);
            return f;
        }

        static Dropdown Combo(int width, params object[] items)
        {
            var c = new Dropdown(width); c.Items.AddRange(items); c.SelectedIndex = 0; return c;
        }

        // ---- server page
        Control ServerPage()
        {
            var page = new TableLayoutPanel { ColumnCount = 1, RowCount = 5, Dock = DockStyle.Fill, Tag = "panel" }; serverPage = page;
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 46)); page.RowStyles.Add(new RowStyle(SizeType.Absolute, StatsHeight));
            page.RowStyles.Add(new RowStyle(SizeType.Absolute, 44)); page.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); page.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));

            var toolbar = new Panel { Dock = DockStyle.Fill, Tag = "panel" };
            var rightFlow = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, WrapContents = false, FlowDirection = FlowDirection.RightToLeft, Padding = new Padding(0, 4, 0, 0), Tag = "panel" };
            var kill = new FlatButton("Kill") { Kind = "danger", Margin = new Padding(8, 0, 0, 0), Glyph = "bolt" };
            var restart = new FlatButton("Restart") { Margin = new Padding(8, 0, 0, 0), Glyph = "restart" };
            var stop = new FlatButton("Stop") { Margin = new Padding(8, 0, 0, 0), Glyph = "stop" };
            var start = new FlatButton("Start") { Kind = "primary", Enabled = false, Margin = new Padding(8, 0, 0, 0), Glyph = "play" };
            rightFlow.Controls.AddRange(new Control[] { kill, restart, stop, start });
            var state = new StateLabel { Dock = DockStyle.Left };
            var stateHolder = new Panel { Dock = DockStyle.Left, Width = state.Width, Padding = new Padding(0, 4, 0, 0), Tag = "panel" };
            stateHolder.Controls.Add(state); state.Dock = DockStyle.Fill;
            toolbar.Controls.Add(rightFlow); toolbar.Controls.Add(stateHolder);
            page.Controls.Add(toolbar, 0, 0);

            var stats = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, Padding = new Padding(0, 4, 0, 8), Tag = "panel" };
            stats.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 30)); stats.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 70));
            var list = new StatsList { Flat = true }; UI.Tips.SetToolTip(list, "Tick rate is how many updates the server runs each second. About 60 is normal. Players does not include bots.");
            var chart = new AreaChart { Flat = true }; UI.Tips.SetToolTip(chart, "Average tick rate for each of the last 20 minutes.");
            var gear = new GearButton(); list.Controls.Add(gear);
            list.Resize += delegate { gear.Location = new Point(list.Width - 34, 6); };
            UI.Tips.SetToolTip(gear, "Customize this panel.");
            gear.Click += delegate
            {
                var menu = Dropdown.Menu(); menu.ShowCheckMargin = true;
                var graphItem = new ToolStripMenuItem("Tick rate graph") { Checked = chart.Visible, ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) };
                graphItem.Click += delegate
                {
                    chart.Visible = !chart.Visible;
                    stats.ColumnStyles[0].Width = chart.Visible ? 30 : 100; stats.ColumnStyles[1].Width = chart.Visible ? 70 : 0;
                };
                menu.Items.Add(graphItem); menu.Items.Add(new ToolStripSeparator());
                for (int i = 0; i < list.All.Length; i++)
                {
                    int idx = i;
                    var it = new ToolStripMenuItem(list.All[i][0]) { Checked = list.On[i], ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) };
                    it.Click += delegate
                    {
                        bool others = false; for (int k = 0; k < list.On.Length; k++) if (k != idx && list.On[k]) others = true;
                        if (others || !list.On[idx]) list.On[idx] = !list.On[idx];
                        list.Invalidate();
                    };
                    menu.Items.Add(it);
                }
                menu.Show(gear, new Point(0, gear.Height));
            };
            stats.Controls.Add(list, 0, 0);
            stats.Controls.Add(chart, 1, 0);
            var dash = new Panel { Dock = DockStyle.Fill, Tag = "panel" }; statsRow = dash;
            var grip = new Grip(); UI.Tips.SetToolTip(grip, "Drag to resize.");
            grip.Dragged += delegate (int dy) { if (!expanded) { dashHeight = Math.Max(90, Math.Min(dashHeight + dy, page.Height - 240)); page.RowStyles[1].Height = dashHeight; } };
            dash.Controls.Add(stats); dash.Controls.Add(grip);
            page.Controls.Add(dash, 0, 1);

            var bar = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, Padding = new Padding(0, 6, 0, 4), Tag = "panel" };
            bar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 150)); bar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 260)); bar.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            var level = Combo(138, "All levels", "Info", "Warnings", "Errors"); level.Anchor = AnchorStyles.Left;
            string[] fk = { "all", "i", "w", "e" };
            level.SelectedIndexChanged += delegate { levelFilter = fk[level.SelectedIndex]; FillConsole(); };
            UI.Tips.SetToolTip(level, "Filter by log level.");
            searchBox = HintBox(240); searchBox.Margin = new Padding(0);
            UI.Tips.SetToolTip(searchBox, "Filter the log by text.");
            searchBox.TextChanged += delegate { FillConsole(); };
            bar.Controls.Add(level, 0, 0); bar.Controls.Add(searchBox, 1, 0);
            page.Controls.Add(bar, 0, 2);

            console = new RichTextBox { Dock = DockStyle.Fill, ReadOnly = true, BorderStyle = BorderStyle.None, Font = new Font("Consolas", 9.5f), Padding = new Padding(8) };
            var holder = new Panel { Dock = DockStyle.Fill, Margin = new Padding(0), Tag = "consolebg" };
            holder.Controls.Add(console);
            var expandBtn = new IconButton("expand"); var copyBtn = new IconButton("copy"); var clearBtn = new IconButton("clear"); var saveBtn = new IconButton("save");
            UI.Tips.SetToolTip(expandBtn, "Expand or collapse the console."); UI.Tips.SetToolTip(copyBtn, "Copy all."); UI.Tips.SetToolTip(clearBtn, "Clear the console."); UI.Tips.SetToolTip(saveBtn, "Save the log to a file.");
            clearBtn.Click += delegate { console.Clear(); };
            copyBtn.Click += delegate { if (console.TextLength > 0) Clipboard.SetText(console.Text); };
            expandBtn.Click += delegate
            {
                expanded = !expanded; statsRow.Visible = !expanded; page.RowStyles[1].Height = expanded ? 0 : dashHeight;
                expandBtn.Glyph = expanded ? "collapse" : "expand"; expandBtn.Invalidate();
            };
            IconButton[] icons = { expandBtn, copyBtn, clearBtn, saveBtn };
            // The icons float in the top right corner, the same distance from the top and from the right edge.
            foreach (var ic in icons) { holder.Controls.Add(ic); ic.BringToFront(); }
            holder.Resize += delegate { for (int i = 0; i < icons.Length; i++) icons[i].Location = new Point(holder.Width - 8 - (icons.Length - i) * 28 - (icons.Length - 1 - i) * 6, 8); };
            page.Controls.Add(holder, 0, 3);

            var cmd = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 2, RowCount = 1, Padding = new Padding(0, 8, 0, 0), Tag = "panel" };
            cmd.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); cmd.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 88));
            var box = new TextBox { BorderStyle = BorderStyle.FixedSingle, Anchor = AnchorStyles.Left | AnchorStyles.Right, Margin = new Padding(0, 0, 8, 0) };
            cmd.Controls.Add(box, 0, 0); cmd.Controls.Add(new FlatButton("Send") { Anchor = AnchorStyles.Left, Margin = new Padding(0) }, 1, 0);
            page.Controls.Add(cmd, 0, 4);
            var surface = new Surface { Padding = new Padding(16, 12, 16, 12) };
            surface.Controls.Add(page);
            return surface;
        }

        static readonly string[][] AllLines = {
            new[] { "i", "[20:50:04 INFO]: Server started on port 27584." }, new[] { "i", "[20:50:04 INFO]: Plugin active: sv.status 0.1.0" },
            new[] { "i", "[20:50:04 INFO]: Plugin active: sv.permissions 0.1.0" }, new[] { "i", "[20:50:04 INFO]: Plugin active: sv.admcmd 0.1.0" },
            new[] { "i", "[20:50:04 INFO]: Plugin active: sv.jukebox 0.2.0" }, new[] { "w", "[20:50:05 WARN]: sv.testroom has full access to the server and can crash it." },
            new[] { "i", "[20:50:30 INFO]: Alex joined the game" }, new[] { "i", "[20:51:09 INFO]: Server status: 59.6 ticks/s, 1 players, 32 bots" },
            new[] { "i", "[20:52:09 INFO]: Server status: 59.4 ticks/s, 1 players, 32 bots" }, new[] { "e", "[20:53:01 ERROR]: Cannot open the packet file capture.bin" },
            new[] { "w", "[20:53:40 WARN]: Speed was below 58 ticks/s for one minute" }, new[] { "i", "[20:54:09 INFO]: Server status: 60.1 ticks/s, 1 players, 32 bots" },
            new[] { "i", "[20:55:09 INFO]: Server status: 59.8 ticks/s, 1 players, 32 bots" }, new[] { "i", "[20:55:41 INFO]: Alex left the game" } };

        void FillConsole()
        {
            if (console == null) return;
            console.Clear();
            string term = searchBox != null && searchBox.Text != "Search…" ? searchBox.Text.Trim().ToLowerInvariant() : "";
            foreach (var l in AllLines)
            {
                if (levelFilter != "all" && l[0] != levelFilter) continue;
                if (term.Length > 0 && l[1].ToLowerInvariant().IndexOf(term) < 0) continue;
                Color c = l[0] == "w" ? UI.T.Warn : l[0] == "e" ? UI.T.Bad : UI.T.ConsoleFg;
                console.SelectionStart = console.TextLength; console.SelectionColor = c; console.AppendText(l[1] + "\n");
            }
        }

        // ---- settings with sub-tabs
        Control SettingsHost()
        {
            var host = new TableLayoutPanel { ColumnCount = 1, RowCount = 2 };
            host.RowStyles.Add(new RowStyle(SizeType.Absolute, 40)); host.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            var strip = new Panel { Dock = DockStyle.Fill, Padding = new Padding(8, 0, 8, 0), Tag = "subbar", Margin = new Padding(0) };
            string[] names = { "Server Settings", "Launcher Settings", "Advanced Settings" };
            var area = new Panel { Dock = DockStyle.Fill, Padding = new Padding(16, 8, 16, 10) };
            Control[] subs = { ServerSettings(), LauncherSettings(), AdvancedSettings() };
            for (int i = names.Length - 1; i >= 0; i--)
            {
                var tab = new TabLabel(names[i]) { Selected = i == 0, Sub = true }; int index = i; tab.Click += delegate { ShowSub(index); };
                subTabs.Insert(0, tab); strip.Controls.Add(tab);
                subs[i].Dock = DockStyle.Fill; subs[i].Visible = i == 0; area.Controls.Add(subs[i]); subPages.Insert(0, subs[i]);
            }
            host.Controls.Add(strip, 0, 0); host.Controls.Add(area, 0, 1);
            return host;
        }

        void Section(TableLayoutPanel t, string title)
        {
            int row = t.RowCount++; t.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));
            var p = new Panel { Dock = DockStyle.Fill, Margin = new Padding(0) };
            var l = L(title, true, false); l.Font = UI.F(11.5f, FontStyle.Bold); l.Location = new Point(0, 12);
            var line = new Panel { Dock = DockStyle.Bottom, Height = 1, Tag = "line" };
            p.Controls.Add(l); p.Controls.Add(line);
            t.Controls.Add(p, 0, row); t.SetColumnSpan(p, 4);
        }

        Control SettingsPage(Action<TableLayoutPanel> build)
        {
            var outer = new Panel();
            var bottom = new Panel { Dock = DockStyle.Bottom, Height = 56, Padding = new Padding(0, 10, 0, 0) };
            var flow = new FlowLayoutPanel { Dock = DockStyle.Fill, WrapContents = false, FlowDirection = FlowDirection.RightToLeft };
            flow.Controls.Add(new FlatButton("Reset to defaults") { Kind = "danger", Margin = new Padding(8, 0, 0, 0) }); flow.Controls.Add(new FlatButton("Revert changes") { Margin = new Padding(8, 0, 0, 0) });
            bottom.Controls.Add(flow);
            var scroll = new Panel { Dock = DockStyle.Fill, AutoScroll = true };
            var t = RowTable(); build(t); scroll.Controls.Add(t);
            outer.Controls.Add(scroll); outer.Controls.Add(bottom); scroll.BringToFront();
            return outer;
        }

        Control ServerSettings()
        {
            return SettingsPage(delegate (TableLayoutPanel t)
            {
                var pw = TB("secret123", 220); pw.UseSystemPasswordChar = true;
                Section(t, "General");
                AddRow(t, "Server name", null, TB("SubVulpes test", 300), null, true);
                AddRow(t, "Admin password", "In the game, type /admin and this password.", Inline(pw, new FlatButton("Show") { Margin = new Padding(8, 0, 0, 0) }), null, true);
                AddRow(t, "Welcome text", "Shown to players in the in-game newspaper (readme.txt).", new TextBox { Text = "Welcome to the test server.", Multiline = true, Height = 54, Width = 380, BorderStyle = BorderStyle.FixedSingle }, null, true);
                Section(t, "Network");
                AddRow(t, "Port", "Forward this UDP port on your router.", TB("27584", 100), "Requires restart", true);
                AddRow(t, "Upload limit", "Shared by all players, in bytes per second.", TB("100000", 120), null, true);
                Section(t, "Game");
                AddRow(t, "Game mode", null, Combo(160, "Round", "World"), "Requires restart", true);
                AddRow(t, "Max players", null, TB("16", 80), "Requires restart", true);
                AddRow(t, "Round time", "Minutes in Round mode.", TB("7", 80), null, true);
            });
        }

        Control LauncherSettings()
        {
            return SettingsPage(delegate (TableLayoutPanel t)
            {
                Section(t, "Server");
                AddRow(t, "Server folder", null, Inline(TB("C:\\SubVulpes-server", 320), new FlatButton("Browse…") { Margin = new Padding(8, 0, 0, 0) }, new FlatButton("Open Folder") { Margin = new Padding(8, 0, 0, 0) }), null, false);
                Section(t, "Updates");
                AddRow(t, "Check now", "Looks for a newer version on GitHub.", Inline(new FlatButton("Check for Updates"), new FlatButton("Update from .ZIP") { Margin = new Padding(0) }), null, false);
                AddRow(t, "Include Beta Versions", "Includes beta versions. They can still contain bugs.", new Switch(), null, true);
                Section(t, "Appearance");
                var theme = Combo(160, "Light", "Dark"); theme.SelectedIndexChanged += delegate { SetTheme(theme.SelectedIndex == 1 ? Theme.Dark : Theme.Light); };
                AddRow(t, "Theme", null, theme, null, true);
            });
        }

        Control AdvancedSettings()
        {
            return SettingsPage(delegate (TableLayoutPanel t)
            {
                Section(t, "Diagnostics");
                AddRow(t, "Diagnostics mode", "Records the tick rate. Saves a minidump if the server crashes or hangs.", Combo(180, "Normal", "Performance", "Network", "Custom"), null, true);
                Section(t, "Tools");
                AddRow(t, "Profiler", "Finds slow code.", new Switch { On = true }, null, true);
                AddRow(t, "Timing Log", "Logs how long each part of the game takes, once a minute.", new Switch(), null, true);
                AddRow(t, "Packet Capture", "Records network traffic, including player names and chat.", new Switch(), null, true);
                Section(t, "Crash handling");
                AddRow(t, "If the server crashes or hangs", null, new Segmented("None", "Minidump (about 1 MB)", "Full dump (about 600 MB)"), null, true);
                AddRow(t, "Memory dump", "A copy of the server memory. Send it to the developers to help find a problem.", new FlatButton("Save Memory Dump Now") { Margin = new Padding(0) }, null, false);
            });
        }

        // ---- plugins
        readonly List<Control> pluginSubPages = new List<Control>();
        readonly List<TabLabel> pluginSubTabs = new List<TabLabel>();

        Control PluginsHost()
        {
            var host = new TableLayoutPanel { ColumnCount = 1, RowCount = 2 };
            host.RowStyles.Add(new RowStyle(SizeType.Absolute, 40)); host.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            var strip = new Panel { Dock = DockStyle.Fill, Padding = new Padding(8, 0, 8, 0), Tag = "subbar", Margin = new Padding(0) };
            var area = new Panel { Dock = DockStyle.Fill, Padding = new Padding(16, 8, 16, 10) };
            string[] names = { "Installed", "Browse" };
            Control[] subs = { PluginsPage(), BrowsePage() };
            for (int i = names.Length - 1; i >= 0; i--)
            {
                var tab = new TabLabel(names[i]) { Selected = i == 0, Sub = true }; int index = i;
                tab.Click += delegate { for (int k = 0; k < pluginSubPages.Count; k++) { pluginSubPages[k].Visible = k == index; pluginSubTabs[k].Selected = k == index; pluginSubTabs[k].Invalidate(); } };
                pluginSubTabs.Insert(0, tab); strip.Controls.Add(tab);
                subs[i].Dock = DockStyle.Fill; subs[i].Visible = i == 0; area.Controls.Add(subs[i]); pluginSubPages.Insert(0, subs[i]);
            }
            host.Controls.Add(strip, 0, 0); host.Controls.Add(area, 0, 1);
            return host;
        }

        Control BrowsePage()
        {
            var page = new Panel();
            var bar = new Panel { Dock = DockStyle.Top, Height = 44 };
            var search = HintBox(260); search.Location = new Point(0, 9);
            bar.Controls.Add(search);
            var scroll = new Panel { Dock = DockStyle.Fill, AutoScroll = true, Tag = "panel" };
            string[][] catalog = {
                new[] { "Bans", "Bans players by account or address, with an optional expiry.", "Official", "0.1.0", "Install" },
                new[] { "Message of the Day", "Shows a message to each player who joins.", "Official", "0.1.0", "Install" },
                new[] { "Audit Log", "Writes who used which admin command, and when.", "Official", "0.1.0", "Install" },
                new[] { "MIDI Jukebox", "Play MIDI music with any game sound.", "Official", "0.2.0", "Installed" },
                new[] { "Flight", "Lets players fly with a key.", "Community", "0.3.1", "Install" } };
            for (int i = catalog.Length - 1; i >= 0; i--)
            {
                var c = catalog[i];
                var row = new Panel { Dock = DockStyle.Top, Height = 78, Tag = "panel" };
                var name = L(c[0], true, false); name.Location = new Point(16, 14);
                var chip = Chip(c[2]); chip.Location = new Point(name.PreferredWidth + 28, 12);
                var desc = L(c[1], false, true); desc.Location = new Point(16, 42);
                var ver = L("v" + c[3], false, true); ver.Anchor = AnchorStyles.Top | AnchorStyles.Right;
                var btn = new FlatButton(c[4]) { Anchor = AnchorStyles.Top | AnchorStyles.Right, Enabled = c[4] == "Install" };
                btn.Location = new Point(row.Width - btn.Width - 16, 22); row.Resize += delegate { btn.Location = new Point(row.Width - btn.Width - 16, 22); ver.Location = new Point(btn.Left - ver.Width - 16, 30); };
                row.Controls.AddRange(new Control[] { name, chip, desc, ver, btn, new Panel { Dock = DockStyle.Bottom, Height = 1, Tag = "line" } });
                scroll.Controls.Add(row);
            }
            page.Controls.Add(scroll); page.Controls.Add(bar); bar.SendToBack(); scroll.BringToFront();
            return page;
        }

        Control PluginsPage()
        {
            var page = new Panel();
            var bar = new Panel { Dock = DockStyle.Top, Height = 44 };
            var barButtons = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, WrapContents = false, FlowDirection = FlowDirection.RightToLeft, Padding = new Padding(0, 4, 0, 0) };
            barButtons.Controls.Add(new FlatButton("Refresh") { Margin = new Padding(0), Glyph = "refresh" }); barButtons.Controls.Add(new FlatButton("Install from File…") { Glyph = "download" });
            var pluginSearch = HintBox(260); pluginSearch.Location = new Point(0, 9);
            bar.Controls.Add(barButtons); bar.Controls.Add(pluginSearch);
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
            foreach (DataGridViewColumn col in grid.Columns) col.SortMode = DataGridViewColumnSortMode.Programmatic;
            int sortCol = -1; bool sortAsc = true;
            Func<Plugin, int, string> key = delegate (Plugin p, int c)
            {
                return c == 0 ? p.Name : c == 1 ? p.Version : c == 2 ? p.State : c == 3 ? (p.Risk == "low" ? "1" : p.Risk == "high" ? "2" : "3") : p.Source;
            };
            Action reload = delegate
            {
                var shown = new List<Plugin>(plugins);
                if (sortCol >= 0) shown.Sort(delegate (Plugin a, Plugin b) { return (sortAsc ? 1 : -1) * string.Compare(key(a, sortCol), key(b, sortCol), StringComparison.OrdinalIgnoreCase); });
                var current = sel >= 0 && sel < plugins.Count ? plugins[sel] : null;
                grid.Rows.Clear();
                foreach (var p in shown) { int r = grid.Rows.Add(p.Name, p.Version, p.State, p.Risk, p.Source, ""); grid.Rows[r].Tag = p; if (p == current) grid.CurrentCell = grid.Rows[r].Cells[0]; }
                foreach (DataGridViewColumn col in grid.Columns) col.HeaderCell.SortGlyphDirection = col.Index == sortCol ? (sortAsc ? SortOrder.Ascending : SortOrder.Descending) : SortOrder.None;
            };
            Action<int, bool> sortBy = delegate (int c, bool asc) { sortCol = c; sortAsc = asc; reload(); };
            grid.ColumnHeaderMouseClick += delegate (object s, DataGridViewCellMouseEventArgs e)
            {
                if (e.Button == MouseButtons.Left) { if (e.ColumnIndex < 5) sortBy(e.ColumnIndex, sortCol == e.ColumnIndex ? !sortAsc : true); return; }
                // Right click on a header: choose the columns and the order, like in Windows Explorer.
                var menu = Dropdown.Menu(); menu.ShowCheckMargin = true;
                for (int i = 1; i <= 4; i++)
                {
                    var col = grid.Columns[i];
                    var it = new ToolStripMenuItem(col.HeaderText) { Checked = col.Visible, ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) };
                    it.Click += delegate { col.Visible = !col.Visible; };
                    menu.Items.Add(it);
                }
                if (e.ColumnIndex < 5)
                {
                    int ci = e.ColumnIndex; menu.Items.Add(new ToolStripSeparator());
                    var asc = new ToolStripMenuItem("Sort ascending") { ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) }; asc.Click += delegate { sortBy(ci, true); };
                    var desc = new ToolStripMenuItem("Sort descending") { ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) }; desc.Click += delegate { sortBy(ci, false); };
                    menu.Items.Add(asc); menu.Items.Add(desc);
                }
                menu.Show(Cursor.Position);
            };
            grid.CellFormatting += delegate (object s, DataGridViewCellFormattingEventArgs e)
            {
                if (e.RowIndex < 0) return; var p = (Plugin)grid.Rows[e.RowIndex].Tag; if (p == null) return;
                if (e.ColumnIndex == 2) e.CellStyle.ForeColor = p.State == "Active" ? UI.T.Ok : p.State == "Restart needed" ? UI.T.Warn : UI.T.Muted;
                if (e.ColumnIndex == 3) e.CellStyle.ForeColor = p.Risk == "low" ? UI.T.Ok : p.Risk == "high" ? UI.T.Warn : UI.T.Bad;
            };
            grid.CellPainting += delegate (object s, DataGridViewCellPaintingEventArgs e)
            {
                if (e.RowIndex < 0 || (e.ColumnIndex != 2 && e.ColumnIndex != 3 && e.ColumnIndex != 5)) return;
                var pl = (Plugin)grid.Rows[e.RowIndex].Tag; if (pl == null) return;
                if (e.ColumnIndex == 2 || e.ColumnIndex == 3)
                {
                    // State gets a colored dot, and Risk gets a tinted pill.
                    e.Paint(e.ClipBounds, DataGridViewPaintParts.Background | DataGridViewPaintParts.Border);
                    var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; int my = e.CellBounds.Y + e.CellBounds.Height / 2;
                    if (e.ColumnIndex == 2)
                    {
                        Color sc = pl.State == "Active" ? UI.T.Ok : pl.State == "Restart needed" ? UI.T.Warn : UI.T.Muted;
                        using (var b = new SolidBrush(sc)) g.FillEllipse(b, e.CellBounds.X + 6, my - 4, 8, 8);
                        TextRenderer.DrawText(g, pl.State, UI.F(9.5f, FontStyle.Regular), new Rectangle(e.CellBounds.X + 20, e.CellBounds.Y, e.CellBounds.Width - 20, e.CellBounds.Height), UI.T.Fg, TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding | TextFormatFlags.EndEllipsis);
                    }
                    else
                    {
                        Color rc = pl.Risk == "low" ? UI.T.Ok : pl.Risk == "high" ? UI.T.Warn : UI.T.Bad; var rf = UI.F(9f, FontStyle.Bold);
                        int rw = TextRenderer.MeasureText(g, pl.Risk, rf, new Size(999, 30), TextFormatFlags.NoPadding).Width + 20; var pill = new Rectangle(e.CellBounds.X + 6, my - 11, rw, 22);
                        using (var path = UI.Round(pill, 11)) using (var b = new SolidBrush(Icons.Blend(UI.T.Panel, rc, 0.16))) g.FillPath(b, path);
                        TextRenderer.DrawText(g, pl.Risk, rf, pill, rc, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
                    }
                    e.Handled = true; return;
                }
                if (e.ColumnIndex != 5) return;
                e.Paint(e.ClipBounds, DataGridViewPaintParts.Background | DataGridViewPaintParts.Border);
                Switch.Draw(e.Graphics, new Rectangle(e.CellBounds.X + 6, e.CellBounds.Y + (e.CellBounds.Height - 20) / 2, 36, 20), ((Plugin)grid.Rows[e.RowIndex].Tag).On);
                e.Handled = true;
            };
            grid.CellClick += delegate (object s, DataGridViewCellEventArgs e)
            {
                if (e.RowIndex < 0) return;
                sel = plugins.IndexOf((Plugin)grid.Rows[e.RowIndex].Tag);
                if (e.ColumnIndex == 5) { var p = plugins[sel]; p.On = !p.On; p.State = p.On ? "Active" : "Disabled"; grid.Rows[e.RowIndex].Cells[2].Value = p.State; grid.InvalidateRow(e.RowIndex); }
                RenderDetail();
            };
            grid.SelectionChanged += delegate
            {
                var p = grid.CurrentRow == null ? null : grid.CurrentRow.Tag as Plugin;
                if (p != null) { sel = plugins.IndexOf(p); RenderDetail(); }
            };
            reload();
            grid.Columns[3].Visible = false; grid.Columns[4].Visible = false;
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
            // The real launcher would read these pages from the .md files in the docs folder of the server. They are built in here for the mockup.
            var page = new Panel { Tag = "panel" };
            var nav = new Panel { Dock = DockStyle.Left, Width = 240, Padding = new Padding(12, 12, 12, 12), Tag = "panel" };
            var docHolder = new Panel { Dock = DockStyle.Fill, Padding = new Padding(36, 24, 36, 16), Tag = "panel" };
            helpDoc = new RichTextBox { Dock = DockStyle.Fill, ReadOnly = true, BorderStyle = BorderStyle.None, Tag = "doc", Cursor = Cursors.Default };
            docHolder.Controls.Add(helpDoc);
            var search = HintBox(214); search.Dock = DockStyle.Top;
            var navList = new Panel { Dock = DockStyle.Fill, Tag = "panel" };
            for (int i = HelpDocs.Length - 1; i >= 0; i--)
            {
                int idx = i; var item = new NavItem(HelpDocs[i][0]); helpNav.Insert(0, item);
                item.Click += delegate { helpIndex = idx; RenderHelp(); };
                navList.Controls.Add(item);
            }
            search.TextChanged += delegate
            {
                string term = search.Text == "Search…" ? "" : search.Text.Trim().ToLowerInvariant();
                for (int i = 0; i < helpNav.Count; i++) helpNav[i].Visible = term.Length == 0 || HelpDocs[i][0].ToLowerInvariant().Contains(term) || HelpDocs[i][1].ToLowerInvariant().Contains(term);
            };
            var gap = new Panel { Dock = DockStyle.Top, Height = 10, Tag = "panel" };
            nav.Controls.Add(navList); nav.Controls.Add(gap); nav.Controls.Add(search);
            var divider = new Panel { Dock = DockStyle.Left, Width = 1, Tag = "line" };
            page.Controls.Add(docHolder); page.Controls.Add(divider); page.Controls.Add(nav);
            docHolder.BringToFront();
            RenderHelp();
            return page;
        }

        RichTextBox helpDoc; int helpIndex; readonly List<NavItem> helpNav = new List<NavItem>();

        static readonly string[][] HelpDocs = {
            new[] { "Getting Started", "# Getting Started\nThis launcher starts and watches your SubVulpes server.\n## First start\n- Open **Settings** and pick a server folder.\n- Set a name and an admin password.\n- Go to **Server** and press **Start**.\n## Join the game\nPlayers join with the address of your computer and the port in **Settings**. Open the UDP port in your router.\n" },
            new[] { "The Server Page", "# The Server Page\nThe top row shows the state of the server and the buttons.\n- **Start** starts the server.\n- **Stop** asks the server to save and close.\n- **Restart** stops the server and starts it again.\n- **Kill** ends the server at once. Use it only if Stop does not work.\n## Tick rate\nThe server runs about 60 ticks per second. A much lower number means lag.\n## Customize the panel\nSelect the sliders icon to choose which values and graphs the panel shows. Drag the line below the panel to change its height.\n" },
            new[] { "Plugins", "# Plugins\nPlugins add features to the server.\n## Risk\n- **low**: reads data.\n- **high**: changes the game.\n- **highest**: runs inside the server with full access to it.\nTurn on only plugins that you trust.\n## Install\nOpen **Browse** to find plugins, or use `Install from File…` for a `.svplugin` file. The launcher shows what the plugin can do before it installs.\n" },
            new[] { "Settings", "# Settings\nThe launcher saves each change at once.\n## Requires restart\nThe launcher saves the setting. The server uses it after the next restart.\n## Diagnostics mode\nChooses what the launcher records while the server runs. **Normal** records the tick rate.\n## Memory dump\nA file with a copy of the server memory. Send it to the developers to help find the cause of a crash or a hang.\n" },
            new[] { "Problems", "# Problems\n## The server does not start\nRead the last lines in the console. Make sure that no other server uses the same port.\n## The server crashes\nThe launcher saves a minidump in the log folder. Send it with the log.\n## Logs\nEach start of the server makes a new log folder. Old logs are in the Server tab.\n```\nlogs\\2026-11-02_20-50\n```\n" } };

        void RenderHelp()
        {
            for (int i = 0; i < helpNav.Count; i++) { helpNav[i].Selected = i == helpIndex; helpNav[i].Invalidate(); }
            var box = helpDoc; box.Clear(); bool code = false;
            foreach (string raw in HelpDocs[helpIndex][1].Split('\n'))
            {
                string line = raw.TrimEnd();
                if (line.StartsWith("```")) { code = !code; continue; }
                if (line.Length == 0) continue;
                if (code) { Put(box, "    " + line + "\n", "Consolas", 10f, FontStyle.Regular, UI.T.Fg, UI.T.Chip, 0); continue; }
                if (line.StartsWith("# ")) Put(box, line.Substring(2) + "\n", "Segoe UI", 20f, FontStyle.Bold, UI.T.Fg, Color.Empty, 8);
                else if (line.StartsWith("## ")) { Put(box, "\n", "Segoe UI", 6f, FontStyle.Regular, UI.T.Fg, Color.Empty, 0); Put(box, line.Substring(3) + "\n", "Segoe UI", 13f, FontStyle.Bold, UI.T.Fg, Color.Empty, 4); }
                else if (line.StartsWith("- ")) Inline(box, "•  " + line.Substring(2) + "\n", 6, 16);
                else Inline(box, line + "\n", 6, 0);
            }
            box.SelectionStart = 0; box.SelectionLength = 0;
        }

        static void Put(RichTextBox box, string text, string font, float size, FontStyle style, Color fg, Color bg, int after)
        {
            box.SelectionStart = box.TextLength; box.SelectionLength = 0;
            box.SelectionFont = new Font(font, size, style, GraphicsUnit.Point); box.SelectionColor = fg;
            box.SelectionBackColor = bg.IsEmpty ? box.BackColor : bg; box.SelectionIndent = 0; box.SelectionHangingIndent = 0;
            box.AppendText(text);
            if (after > 0) { box.SelectionStart = box.TextLength; box.SelectionFont = new Font(font, after, FontStyle.Regular, GraphicsUnit.Point); box.AppendText("\n"); }
        }

        // Writes text with **bold** and `code` marks.
        static void Inline(RichTextBox box, string text, int after, int indent)
        {
            bool bold = false, code = false; var part = new System.Text.StringBuilder();
            Action flush = delegate
            {
                if (part.Length == 0) return;
                box.SelectionStart = box.TextLength; box.SelectionLength = 0; box.SelectionIndent = indent; box.SelectionHangingIndent = indent > 0 ? 14 : 0;
                box.SelectionFont = code ? new Font("Consolas", 9.5f, FontStyle.Regular) : UI.F(10.5f, bold ? FontStyle.Bold : FontStyle.Regular);
                box.SelectionColor = UI.T.Fg; box.SelectionBackColor = code ? UI.T.Chip : box.BackColor;
                box.AppendText(part.ToString()); part.Length = 0;
            };
            for (int i = 0; i < text.Length; i++)
            {
                if (text[i] == '*' && i + 1 < text.Length && text[i + 1] == '*') { flush(); bold = !bold; i++; }
                else if (text[i] == '`') { flush(); code = !code; }
                else part.Append(text[i]);
            }
            flush();
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
