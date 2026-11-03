// A dummy launcher in Windows Forms. It shows the new look with fake data and talks to no server.
// Build: tools\launcher-mockup\build.bat. Run with --shot DIR to save a picture of each page (the form draws itself, it does not copy the screen).
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
        public Color Violet, Bg, Win, Panel, Line, Fg, Muted, Accent, Ok, Warn, Bad, Chip, Console, ConsoleFg, Bar, BarFg, BarMuted, SubBar;
        public static readonly Theme Light = new Theme { Violet = Color.FromArgb(112, 82, 196),
            Bg = Color.FromArgb(238, 240, 243), Win = Color.FromArgb(246, 247, 249), Panel = Color.White, Line = Color.FromArgb(217, 220, 225),
            Fg = Color.FromArgb(27, 29, 33), Muted = Color.FromArgb(106, 112, 122), Accent = Color.FromArgb(217, 99, 30),
            Ok = Color.FromArgb(42, 138, 74), Warn = Color.FromArgb(183, 121, 31), Bad = Color.FromArgb(194, 54, 43),
            Chip = Color.FromArgb(236, 239, 243), Console = Color.Black, ConsoleFg = Color.FromArgb(212, 215, 220),
            Bar = Color.FromArgb(222, 226, 232), SubBar = Color.FromArgb(238, 240, 244), BarFg = Color.White, BarMuted = Color.FromArgb(96, 103, 115) };
        public static readonly Theme Dark = new Theme { Violet = Color.FromArgb(176, 158, 240),
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
            Svg.Draw(g, name, new RectangleF(x, y, 16, 16), c);
        }
    }

    // Draws the icons of Lucide (https://lucide.dev, ISC license, Copyright (c) Lucide Icons and Contributors).
    // Each icon is a list of shapes in a 24 by 24 box: "p:" path data, "c:" circle (cx,cy,r), "r:" rectangle (x,y,width,height,radius).
    static class Svg
    {
        static readonly Dictionary<string, string[]> Shapes = new Dictionary<string, string[]> {
            { "play", new[] { "p:M5 5a2 2 0 0 1 3.008-1.728l11.997 6.998a2 2 0 0 1 .003 3.458l-12 7A2 2 0 0 1 5 19z" } },
            { "stop", new[] { "r:3,3,18,18,2" } },
            { "restart", new[] { "p:M21 12a9 9 0 1 1-9-9c2.52 0 4.93 1 6.74 2.74L21 8", "p:M21 3v5h-5" } },
            { "bolt", new[] { "p:M15.914 4a1.5 1.5 0 00-2.474-1.561l-9 9A1.5 1.5 0 005.5 14h4.002a.5.5 0 01.471.666L8.086 20a1.5 1.5 0 002.475 1.56l9-9A1.5 1.5 0 0018.5 10h-3.997a.5.5 0 01-.472-.667z" } },
            { "download", new[] { "p:M12 15V3", "p:M21 15v4a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2v-4", "p:m7 10 5 5 5-5" } },
            { "trash", new[] { "p:M10 11v6", "p:M14 11v6", "p:M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6", "p:M3 6h18", "p:M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2" } },
            { "copy", new[] { "r:8,8,14,14,2", "p:M4 16c-1.1 0-2-.9-2-2V4c0-1.1.9-2 2-2h10c1.1 0 2 .9 2 2" } },
            { "expand", new[] { "p:M15 3h6v6", "p:m21 3-7 7", "p:m3 21 7-7", "p:M9 21H3v-6" } },
            { "collapse", new[] { "p:m14 10 7-7", "p:M20 10h-6V4", "p:m3 21 7-7", "p:M4 14h6v6" } },
            { "sliders", new[] { "p:M10 5H3", "p:M12 19H3", "p:M14 3v4", "p:M16 17v4", "p:M21 12h-9", "p:M21 19h-5", "p:M21 5h-7", "p:M8 10v4", "p:M8 12H3" } },
            { "search", new[] { "p:m21 21-4.34-4.34", "c:11,11,8" } },
            { "info", new[] { "c:12,12,10", "p:M12 16v-4", "p:M12 8h.01" } } };
        static readonly Dictionary<string, string> Alias = new Dictionary<string, string> { { "refresh", "restart" }, { "save", "download" }, { "clear", "trash" }, { "bolt", "bolt" } };

        public static void Draw(Graphics g, string name, RectangleF box, Color color)
        {
            string key; if (!Alias.TryGetValue(name, out key)) key = name;
            string[] shapes; if (!Shapes.TryGetValue(key, out shapes)) return;
            var old = g.Transform; var oldSmooth = g.SmoothingMode; g.SmoothingMode = SmoothingMode.AntiAlias;
            g.TranslateTransform(box.X, box.Y); g.ScaleTransform(box.Width / 24f, box.Height / 24f);
            using (var pen = new Pen(color, 2f) { StartCap = LineCap.Round, EndCap = LineCap.Round, LineJoin = LineJoin.Round })
                foreach (string s in shapes)
                    using (var path = Build(s)) g.DrawPath(pen, path);
            g.Transform = old; g.SmoothingMode = oldSmooth;
        }

        static GraphicsPath Build(string shape)
        {
            string kind = shape.Substring(0, 2), body = shape.Substring(2);
            if (kind == "p:") return Parse(body);
            var path = new GraphicsPath(); var n = body.Split(',');
            if (kind == "c:") path.AddEllipse(float.Parse(n[0]) - float.Parse(n[2]), float.Parse(n[1]) - float.Parse(n[2]), 2 * float.Parse(n[2]), 2 * float.Parse(n[2]));
            else
            {
                float x = float.Parse(n[0]), y = float.Parse(n[1]), w = float.Parse(n[2]), h = float.Parse(n[3]), r = float.Parse(n[4]), d = 2 * r;
                path.AddArc(x, y, d, d, 180, 90); path.AddArc(x + w - d, y, d, d, 270, 90); path.AddArc(x + w - d, y + h - d, d, d, 0, 90); path.AddArc(x, y + h - d, d, d, 90, 90); path.CloseFigure();
            }
            return path;
        }

        static void Skip(string d, ref int i) { while (i < d.Length && (d[i] == ' ' || d[i] == ',' || d[i] == '\n' || d[i] == '\t')) i++; }

        static float Num(string d, ref int i)
        {
            Skip(d, ref i); int start = i;
            if (i < d.Length && (d[i] == '-' || d[i] == '+')) i++;
            while (i < d.Length && char.IsDigit(d[i])) i++;
            if (i < d.Length && d[i] == '.') { i++; while (i < d.Length && char.IsDigit(d[i])) i++; }
            return float.Parse(d.Substring(start, i - start), System.Globalization.CultureInfo.InvariantCulture);
        }

        static bool Flag(string d, ref int i) { Skip(d, ref i); bool f = d[i] == '1'; i++; return f; }

        static GraphicsPath Parse(string d)
        {
            var path = new GraphicsPath(); int i = 0; PointF cur = new PointF(), start = new PointF(), ctrl = new PointF(); char cmd = ' ', prev = ' ';
            while (true)
            {
                Skip(d, ref i); if (i >= d.Length) break;
                if (char.IsLetter(d[i]))
                {
                    cmd = d[i++];
                    if (cmd == 'Z' || cmd == 'z') { path.CloseFigure(); cur = start; prev = 'Z'; continue; }
                }
                bool rel = char.IsLower(cmd); char op = char.ToUpper(cmd); float ox = rel ? cur.X : 0, oy = rel ? cur.Y : 0;
                if (op == 'M') { cur = new PointF(ox + Num(d, ref i), oy + Num(d, ref i)); start = cur; path.StartFigure(); cmd = rel ? 'l' : 'L'; }
                else if (op == 'L') { var p = new PointF(ox + Num(d, ref i), oy + Num(d, ref i)); path.AddLine(cur, p); cur = p; }
                else if (op == 'H') { var p = new PointF(ox + Num(d, ref i), cur.Y); path.AddLine(cur, p); cur = p; }
                else if (op == 'V') { var p = new PointF(cur.X, oy + Num(d, ref i)); path.AddLine(cur, p); cur = p; }
                else if (op == 'C' || op == 'S')
                {
                    PointF p1;
                    if (op == 'C') p1 = new PointF(ox + Num(d, ref i), oy + Num(d, ref i));
                    else p1 = (prev == 'C') ? new PointF(2 * cur.X - ctrl.X, 2 * cur.Y - ctrl.Y) : cur;
                    var p2 = new PointF(ox + Num(d, ref i), oy + Num(d, ref i)); var p3 = new PointF(ox + Num(d, ref i), oy + Num(d, ref i));
                    path.AddBezier(cur, p1, p2, p3); ctrl = p2; cur = p3;
                }
                else if (op == 'A')
                {
                    float rx = Num(d, ref i), ry = Num(d, ref i), rot = Num(d, ref i); bool large = Flag(d, ref i), sweep = Flag(d, ref i);
                    var p = new PointF(ox + Num(d, ref i), oy + Num(d, ref i)); Arc(path, cur, rx, ry, rot, large, sweep, p); cur = p;
                }
                else break;
                prev = op == 'S' ? 'C' : op;
            }
            return path;
        }

        static PointF OnEllipse(double cx, double cy, double rx, double ry, double cos, double sin, double ux, double uy)
        {
            return new PointF((float)(cx + rx * cos * ux - ry * sin * uy), (float)(cy + rx * sin * ux + ry * cos * uy));
        }

        // Turns an SVG arc into curves (the formulas of the SVG specification, appendix F.6).
        static void Arc(GraphicsPath path, PointF a, double rx, double ry, double phiDeg, bool large, bool sweep, PointF b)
        {
            if (a.X == b.X && a.Y == b.Y) return;
            rx = Math.Abs(rx); ry = Math.Abs(ry);
            if (rx == 0 || ry == 0) { path.AddLine(a, b); return; }
            double phi = phiDeg * Math.PI / 180, cos = Math.Cos(phi), sin = Math.Sin(phi);
            double dx = (a.X - b.X) / 2.0, dy = (a.Y - b.Y) / 2.0, x1 = cos * dx + sin * dy, y1 = -sin * dx + cos * dy;
            double lambda = x1 * x1 / (rx * rx) + y1 * y1 / (ry * ry);
            if (lambda > 1) { double s = Math.Sqrt(lambda); rx *= s; ry *= s; }
            double num = rx * rx * ry * ry - rx * rx * y1 * y1 - ry * ry * x1 * x1, den = rx * rx * y1 * y1 + ry * ry * x1 * x1;
            double co = Math.Sqrt(Math.Max(0, num / den)); if (large == sweep) co = -co;
            double cxp = co * rx * y1 / ry, cyp = -co * ry * x1 / rx;
            double cx = cos * cxp - sin * cyp + (a.X + b.X) / 2.0, cy = sin * cxp + cos * cyp + (a.Y + b.Y) / 2.0;
            double th1 = Math.Atan2((y1 - cyp) / ry, (x1 - cxp) / rx), th2 = Math.Atan2((-y1 - cyp) / ry, (-x1 - cxp) / rx), dth = th2 - th1;
            if (sweep && dth < 0) dth += 2 * Math.PI; else if (!sweep && dth > 0) dth -= 2 * Math.PI;
            int n = (int)Math.Ceiling(Math.Abs(dth) / (Math.PI / 2) - 1e-9); if (n < 1) n = 1;
            double step = dth / n, t = 4.0 / 3.0 * Math.Tan(step / 4); PointF cur = a;
            for (int k = 0; k < n; k++)
            {
                double a1 = th1 + k * step, a2 = a1 + step, c1 = Math.Cos(a1), s1 = Math.Sin(a1), c2 = Math.Cos(a2), s2 = Math.Sin(a2);
                var p1 = OnEllipse(cx, cy, rx, ry, cos, sin, c1 - t * s1, s1 + t * c1);
                var p2 = OnEllipse(cx, cy, rx, ry, cos, sin, c2 + t * s2, s2 - t * c2);
                var p3 = k == n - 1 ? b : OnEllipse(cx, cy, rx, ry, cos, sin, c2, s2);
                path.AddBezier(cur, p1, p2, p3); cur = p3;
            }
        }
    }

    // A search field with a magnifier. The text sits in the middle of the box and lines up with the text of the dropdowns.
    class SearchBox : Control
    {
        public readonly TextBox Box;
        public SearchBox(int width)
        {
            Width = width; Height = 28; Anchor = AnchorStyles.Left; Margin = new Padding(0); SetStyle(UI.Paint, true);
            Box = new TextBox { Text = "Search…", BorderStyle = BorderStyle.None, Tag = "hint", Font = UI.F(9.5f, FontStyle.Regular) };
            Box.GotFocus += delegate { if (Box.Text == "Search…") { Box.Text = ""; Box.ForeColor = UI.T.Fg; } Invalidate(); };
            Box.LostFocus += delegate { if (Box.Text.Length == 0) { Box.Text = "Search…"; Box.ForeColor = UI.T.Muted; } Invalidate(); };
            Controls.Add(Box); Place();
        }
        public override string Text { get { return Box.Text == "Search…" ? "" : Box.Text; } set { Box.Text = value; } }
        public new event EventHandler TextChanged { add { Box.TextChanged += value; } remove { Box.TextChanged -= value; } }
        void Place() { Box.Location = new Point(32, (Height - Box.Height) / 2); Box.Width = Math.Max(20, Width - 32 - 10); }
        protected override void OnResize(EventArgs e) { base.OnResize(e); if (Box != null) Place(); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            using (var path = UI.Round(new Rectangle(0, 0, Width - 1, Height - 1), 6)) using (var b = new SolidBrush(UI.T.Panel)) using (var p = new Pen(Box.Focused ? UI.T.Accent : UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
            Svg.Draw(g, "search", new RectangleF(9, (Height - 15) / 2f, 15, 15), UI.T.Muted);
        }
    }

    // A text field with rounded corners, like the dropdowns and the search box.
    class TextField : Control
    {
        public readonly TextBox Box; readonly bool multiline;
        public TextField(int width, bool multiline)
        {
            this.multiline = multiline; Width = width; Height = multiline ? 56 : 28; Anchor = AnchorStyles.Left; Margin = new Padding(0); SetStyle(UI.Paint, true);
            Box = new TextBox { BorderStyle = BorderStyle.None, Multiline = multiline, Font = UI.F(9.5f, FontStyle.Regular) };
            Box.GotFocus += delegate { Invalidate(); }; Box.LostFocus += delegate { Invalidate(); };
            Controls.Add(Box); Place();
        }
        public override string Text { get { return Box.Text; } set { Box.Text = value; } }
        public new event EventHandler TextChanged { add { Box.TextChanged += value; } remove { Box.TextChanged -= value; } }
        void Place()
        {
            Box.Width = Math.Max(20, Width - 20);
            if (multiline) { Box.Height = Math.Max(20, Height - 14); Box.Location = new Point(10, 7); }
            else Box.Location = new Point(10, (Height - Box.Height) / 2);
        }
        protected override void OnResize(EventArgs e) { base.OnResize(e); if (Box != null) Place(); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            using (var path = UI.Round(new Rectangle(0, 0, Width - 1, Height - 1), 6)) using (var b = new SolidBrush(UI.T.Panel)) using (var p = new Pen(Box.Focused ? UI.T.Accent : UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
        }
    }

    // A small violet pill with the restart icon. It tells that a setting needs a restart of the server.
    class Badge : Control
    {
        readonly string text;
        public Badge(string text)
        {
            this.text = text.ToUpperInvariant(); Height = 22; SetStyle(UI.Paint, true);
            Width = 9 + 12 + 6 + TextRenderer.MeasureText(this.text, UI.F(8f, FontStyle.Bold), new Size(999, 30), TextFormatFlags.NoPadding).Width + 11;
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            using (var path = UI.Round(new Rectangle(0, 0, Width - 1, Height - 1), 11)) using (var b = new SolidBrush(Icons.Blend(UI.T.Win, UI.T.Violet, 0.14))) g.FillPath(b, path);
            Svg.Draw(g, "restart", new RectangleF(9, (Height - 12) / 2f, 12, 12), UI.T.Violet);
            TextRenderer.DrawText(g, text, UI.F(8f, FontStyle.Bold), new Rectangle(27, 0, Width - 27, Height), UI.T.Violet, TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
        }
    }

    class Avatar : Control
    {
        public Avatar(string text) { Text = text; Size = new Size(44, 44); SetStyle(UI.Paint, true); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(UI.T.Panel);
            using (var path = UI.Round(new Rectangle(0, 0, Width - 1, Height - 1), 10)) using (var b = new SolidBrush(Icons.Blend(UI.T.Panel, UI.T.Accent, 0.16))) g.FillPath(b, path);
            TextRenderer.DrawText(g, Text.Substring(0, 1), UI.F(14f, FontStyle.Bold), new Rectangle(0, 0, Width, Height), UI.T.Accent, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
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
            for (int i = 0; i < options.Length; i++) { widths[i] = TextRenderer.MeasureText(options[i], UI.F(9.5f, FontStyle.Regular)).Width + 24; total += widths[i]; }
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
                TextRenderer.DrawText(g, Options[i], UI.F(9.5f, FontStyle.Regular), r, i == sel ? UI.T.Accent : UI.T.Muted, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
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
            if (Kind == "link") { Color back = Parent != null ? Parent.BackColor : UI.T.Win; fill = back; border = back; fg = hover ? UI.T.Bad : UI.T.Muted; }
            else if (!Enabled) { fill = UI.T.Chip; border = UI.T.Line; fg = UI.T.Muted; }
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
                TextRenderer.DrawText(g, items[i][0], UI.F(9.5f, FontStyle.Regular), new Rectangle(16, y, Math.Max(20, Width - 32 - vw - 12), rowH), UI.T.Muted, TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPadding);
                TextRenderer.DrawText(g, items[i][1], vf, new Rectangle(16, y, Width - 32, rowH), UI.T.Fg, TextFormatFlags.Right | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
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
            Svg.Draw(g, "sliders", new RectangleF(3, 3, 18, 18), hot ? UI.T.Accent : UI.T.Muted);
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
            Svg.Draw(g, "info", new RectangleF(2, 2, Width - 4, Height - 4), UI.T.Muted);
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
        public event Action<int> Dragged; int start; bool down; readonly bool vertical;
        // A thin line that the user drags. Vertical means that it splits left from right.
        public Grip(bool vertical)
        {
            this.vertical = vertical; SetStyle(UI.Paint, true); Margin = new Padding(0);
            if (vertical) { Width = 9; Dock = DockStyle.Fill; Cursor = Cursors.SizeWE; } else { Height = 9; Dock = DockStyle.Bottom; Cursor = Cursors.SizeNS; }
        }
        protected override void OnMouseDown(MouseEventArgs e) { down = true; start = vertical ? Cursor.Position.X : Cursor.Position.Y; base.OnMouseDown(e); }
        protected override void OnMouseUp(MouseEventArgs e) { down = false; base.OnMouseUp(e); }
        protected override void OnMouseMove(MouseEventArgs e)
        {
            if (down && Dragged != null) { int now = vertical ? Cursor.Position.X : Cursor.Position.Y; Dragged(now - start); start = now; }
            base.OnMouseMove(e);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.Clear(Parent.BackColor);
            using (var p = new Pen(UI.T.Line))
            {
                if (vertical) e.Graphics.DrawLine(p, Width / 2, 6, Width / 2, Height - 10);
                else e.Graphics.DrawLine(p, 0, Height - 1, Width, Height - 1);
            }
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
            Svg.Draw(g, Glyph, new RectangleF(6, 6, 16, 16), Color.FromArgb(hot ? 255 : 130, UI.T.ConsoleFg));
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
        const int StatsHeight = 214; int dashHeight = StatsHeight;

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
            else if (what == "plugins") { ShowPage(1); showPluginSub(0); }
            else if (what == "plugins-browse") { ShowPage(1); showPluginSub(1); }
            else if (what == "help") ShowPage(3);
        }
        public static string[] Names = { "server", "settings-server", "settings-launcher", "settings-advanced", "plugins", "plugins-browse", "help" };
        Action<int> showPluginSub;

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
            else if (c is Surface) c.BackColor = UI.T.Panel;
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

        static TextField TB(string text, int width)
        {
            var f = new TextField(width, false); f.Box.Text = text; return f;
        }

        TableLayoutPanel RowTable()
        {
            var t = new TableLayoutPanel { Dock = DockStyle.Top, AutoSize = true, ColumnCount = 4 };
            t.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 240)); t.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 34));
            t.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100)); t.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 20));
            return t;
        }

        void AddRow(TableLayoutPanel t, string label, string help, Control input, string hint, bool autosave)
        {
            int row = t.RowCount++; t.RowStyles.Add(new RowStyle(SizeType.Absolute, 48));
            var lab = L(label, true, false); lab.Anchor = AnchorStyles.Left; t.Controls.Add(lab, 0, row);
            if (help != null) { var h = new HelpIcon(help) { Anchor = AnchorStyles.Left }; t.Controls.Add(h, 1, row); }
            input.Anchor = AnchorStyles.Left; input.Margin = new Padding(0);
            Control cell = input;
            if (hint != null) cell = Inline(input, new Badge(hint) { Margin = new Padding(12, 2, 0, 0) });  // the badge sits right next to the field
            cell.Anchor = AnchorStyles.Left; t.Controls.Add(cell, 2, row);
            if (autosave)
            {
                // A change only marks the page as edited. The Save button at the bottom of Settings writes it.
                EventHandler show = delegate { MarkDirty(); };
                if (input is TextField) ((TextField)input).TextChanged += show;
                else if (input is TextBox) ((TextBox)input).TextChanged += show;
                else if (input is Dropdown) ((Dropdown)input).SelectedIndexChanged += show;
                else if (input is Switch) ((Switch)input).Changed += show;
                else if (input is Segmented) ((Segmented)input).Changed += show;
            }
        }

        static SearchBox HintBox(int width) { return new SearchBox(width); }

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

            var stats = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, Padding = new Padding(0, 4, 0, 8), Tag = "panel" };
            int listWidth = 290;
            stats.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, listWidth)); stats.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 9)); stats.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            var vgrip = new Grip(true); UI.Tips.SetToolTip(vgrip, "Drag to resize.");
            var list = new StatsList { Flat = true }; UI.Tips.SetToolTip(list, "Tick rate is how many updates the server runs each second. About 60 is normal. Players does not include bots.");
            var chart = new AreaChart { Flat = true }; UI.Tips.SetToolTip(chart, "Average tick rate for each of the last 20 minutes.");
            // The customize icon sits in a small header row, at the right edge of the panel.
            var head = new Panel { Dock = DockStyle.Top, Height = 28, Tag = "panel" };
            var gear = new GearButton { Dock = DockStyle.Right };
            head.Controls.Add(gear);
            UI.Tips.SetToolTip(gear, "Customize this panel.");
            gear.Click += delegate
            {
                var menu = Dropdown.Menu(); menu.ShowCheckMargin = true;
                var graphItem = new ToolStripMenuItem("Tick rate graph") { Checked = chart.Visible, ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) };
                graphItem.Click += delegate
                {
                    chart.Visible = !chart.Visible; vgrip.Visible = chart.Visible;
                    stats.ColumnStyles[0] = chart.Visible ? new ColumnStyle(SizeType.Absolute, listWidth) : new ColumnStyle(SizeType.Percent, 100);
                    stats.ColumnStyles[1] = new ColumnStyle(SizeType.Absolute, chart.Visible ? 9 : 0);
                    stats.ColumnStyles[2] = chart.Visible ? new ColumnStyle(SizeType.Percent, 100) : new ColumnStyle(SizeType.Absolute, 0);
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
            vgrip.Dragged += delegate (int dx)
            {
                if (!chart.Visible) return;
                listWidth = Math.Max(170, Math.Min(listWidth + dx, stats.Width - 280)); stats.ColumnStyles[0].Width = listWidth;
            };
            stats.Controls.Add(list, 0, 0);
            stats.Controls.Add(vgrip, 1, 0);
            stats.Controls.Add(chart, 2, 0);
            var dash = new Panel { Dock = DockStyle.Fill, Tag = "panel" }; statsRow = dash;
            var grip = new Grip(false); UI.Tips.SetToolTip(grip, "Drag to resize.");
            grip.Dragged += delegate (int dy) { if (!expanded) { dashHeight = Math.Max(90, Math.Min(dashHeight + dy, page.Height - 240)); page.RowStyles[1].Height = dashHeight; } };
            dash.Controls.Add(stats); dash.Controls.Add(head); dash.Controls.Add(grip);
            page.Controls.Add(dash, 0, 1);

            var bar = new TableLayoutPanel { Dock = DockStyle.Fill, ColumnCount = 3, RowCount = 1, Padding = new Padding(0, 6, 0, 4), Tag = "panel" };
            bar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 150)); bar.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 260)); bar.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            var level = Combo(138, "All levels", "Info", "Warnings", "Errors"); level.Anchor = AnchorStyles.Left;
            string[] fk = { "all", "i", "w", "e" };
            level.SelectedIndexChanged += delegate { levelFilter = fk[level.SelectedIndex]; FillConsole(); };
            UI.Tips.SetToolTip(level, "Filter by log level.");
            var searchField = HintBox(240); searchBox = searchField.Box;
            UI.Tips.SetToolTip(searchField, "Filter the log by text.");
            searchField.TextChanged += delegate { FillConsole(); };
            bar.Controls.Add(level, 0, 0); bar.Controls.Add(searchField, 1, 0);
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
            var box = new TextField(200, false) { Anchor = AnchorStyles.Left | AnchorStyles.Right, Margin = new Padding(0, 0, 8, 0) };
            var send = new FlatButton("Send") { Anchor = AnchorStyles.Left, Margin = new Padding(0), Height = 28 };
            cmd.Controls.Add(box, 0, 0); cmd.Controls.Add(send, 1, 0);
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
            var host = new TableLayoutPanel { ColumnCount = 1, RowCount = 3 };
            host.RowStyles.Add(new RowStyle(SizeType.Absolute, 40)); host.RowStyles.Add(new RowStyle(SizeType.Percent, 100)); host.RowStyles.Add(new RowStyle(SizeType.Absolute, 58));
            var strip = new Panel { Dock = DockStyle.Fill, Padding = new Padding(8, 0, 8, 0), Tag = "subbar", Margin = new Padding(0) };
            string[] names = { "Server Settings", "Launcher Settings", "Advanced Settings" };
            var settingsBar = SettingsBar();
            var area = new Panel { Dock = DockStyle.Fill, Padding = new Padding(16, 8, 16, 10) };
            Control[] subs = { ServerSettings(), LauncherSettings(), AdvancedSettings() };
            for (int i = names.Length - 1; i >= 0; i--)
            {
                var tab = new TabLabel(names[i]) { Selected = i == 0, Sub = true }; int index = i; tab.Click += delegate { ShowSub(index); };
                subTabs.Insert(0, tab); strip.Controls.Add(tab);
                subs[i].Dock = DockStyle.Fill; subs[i].Visible = i == 0; area.Controls.Add(subs[i]); subPages.Insert(0, subs[i]);
            }
            host.Controls.Add(strip, 0, 0); host.Controls.Add(area, 0, 1); host.Controls.Add(settingsBar, 0, 2);
            return host;
        }

        void Section(TableLayoutPanel t, string title)
        {
            int row = t.RowCount++; t.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));
            var p = new Panel { Dock = DockStyle.Fill, Margin = new Padding(0) };
            var l = L(title, true, false); l.Font = UI.F(12f, FontStyle.Regular); l.Location = new Point(0, 12);
            var line = new Panel { Dock = DockStyle.Bottom, Height = 1, Tag = "line" };
            p.Controls.Add(l); p.Controls.Add(line);
            t.Controls.Add(p, 0, row); t.SetColumnSpan(p, 4);
        }

        Control SettingsPage(Action<TableLayoutPanel> build)
        {
            var scroll = new Panel { Dock = DockStyle.Fill, AutoScroll = true };
            var t = RowTable(); build(t); scroll.Controls.Add(t);
            return scroll;
        }

        FlatButton saveBtn, revertBtn; Label dirtyLabel;

        void MarkDirty()
        {
            if (saveBtn == null) return;
            saveBtn.Enabled = true; revertBtn.Enabled = true; saveBtn.Invalidate(); revertBtn.Invalidate();
            dirtyLabel.Text = "Unsaved changes"; dirtyLabel.Tag = "warn"; Apply(dirtyLabel);
        }

        // One bar for all the Settings pages. Reset to defaults sits apart on the left, because it is a last resort.
        Control SettingsBar()
        {
            var bar = new Panel { Dock = DockStyle.Fill, Padding = new Padding(16, 8, 16, 8) };
            var line = new Panel { Dock = DockStyle.Top, Height = 1, Tag = "line" };
            var reset = new FlatButton("Reset to defaults…") { Kind = "link", Dock = DockStyle.Left };
            reset.Click += delegate
            {
                if (MessageBox.Show("Set every setting back to its default value?", "Reset to defaults", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.Yes) MarkDirty();
            };
            var flow = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, WrapContents = false, FlowDirection = FlowDirection.RightToLeft };
            saveBtn = new FlatButton("Save") { Kind = "primary", Enabled = false, Margin = new Padding(8, 0, 0, 0) };
            revertBtn = new FlatButton("Revert changes") { Enabled = false, Margin = new Padding(8, 0, 0, 0) };
            dirtyLabel = L("", false, true); dirtyLabel.Margin = new Padding(0, 8, 16, 0);
            saveBtn.Click += delegate { saveBtn.Enabled = false; revertBtn.Enabled = false; dirtyLabel.Text = "Saved ✓"; dirtyLabel.Tag = "ok"; Apply(dirtyLabel); saveBtn.Invalidate(); revertBtn.Invalidate(); };
            revertBtn.Click += delegate { saveBtn.Enabled = false; revertBtn.Enabled = false; dirtyLabel.Text = ""; saveBtn.Invalidate(); revertBtn.Invalidate(); };
            flow.Controls.Add(saveBtn); flow.Controls.Add(revertBtn); flow.Controls.Add(dirtyLabel);
            bar.Controls.Add(flow); bar.Controls.Add(reset); bar.Controls.Add(line);
            return bar;
        }

        Control ServerSettings()
        {
            return SettingsPage(delegate (TableLayoutPanel t)
            {
                var pw = TB("secret123", 220); pw.Box.UseSystemPasswordChar = true;
                Section(t, "General");
                AddRow(t, "Server name", null, TB("SubVulpes test", 300), null, true);
                AddRow(t, "Admin password", "In the game, type /admin and this password.", Inline(pw, new FlatButton("Show") { Margin = new Padding(8, 0, 0, 0) }), null, true);
                AddRow(t, "Welcome text", "Shown to players in the in-game newspaper (readme.txt).", new TextField(380, true) { Text = "Welcome to the test server." }, null, true);
                Section(t, "Network");
                AddRow(t, "Port", "Forward this UDP port on your router.", TB("27584", 100), "Restart required", true);
                AddRow(t, "Upload limit", "Shared by all players, in bytes per second.", TB("100000", 120), null, true);
                Section(t, "Game");
                AddRow(t, "Game mode", null, Combo(160, "Round", "World"), "Restart required", true);
                AddRow(t, "Max players", null, TB("16", 80), "Restart required", true);
                AddRow(t, "Round time", "Minutes in Round mode.", TB("7", 80), null, true);
            });
        }

        Control LauncherSettings()
        {
            return SettingsPage(delegate (TableLayoutPanel t)
            {
                Section(t, "Server");
                AddRow(t, "Server folder", null, Inline(TB("C:\\SubVulpes-server", 240), new FlatButton("Browse…") { Margin = new Padding(8, 0, 0, 0) }, new FlatButton("Open Folder") { Margin = new Padding(8, 0, 0, 0) }), null, false);
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
                AddRow(t, "Diagnostics mode", "Records the tick rate while the server runs.", Combo(180, "Normal", "Performance", "Custom"), null, true);
                Section(t, "Tools");
                AddRow(t, "Profiler", "Finds slow code.", new Switch { On = true }, null, true);
                AddRow(t, "Timing Log", "Logs how long each part of the game takes, once a minute.", new Switch(), null, true);
                Section(t, "Crash reports");
                AddRow(t, "Crash dump", "Saved when the server crashes or hangs. Send it to the developers.", new Segmented("Off", "Minidump (about 1 MB)", "Full dump (about 600 MB)"), null, true);
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
            string[] names = { "My Plugins", "Catalog" };
            Control[] subs = { PluginsPage(), BrowsePage() };
            showPluginSub = delegate (int k)
            {
                for (int n = 0; n < pluginSubPages.Count; n++) { pluginSubPages[n].Visible = n == k; pluginSubTabs[n].Selected = n == k; pluginSubTabs[n].Invalidate(); }
            };
            for (int i = names.Length - 1; i >= 0; i--)
            {
                var tab = new TabLabel(names[i]) { Selected = i == 0, Sub = true }; int index = i;
                tab.Click += delegate { showPluginSub(index); };
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
            var search = HintBox(260); search.Location = new Point(0, 8);
            var kindFilter = Combo(150, "All plugins", "Official", "Community"); kindFilter.Location = new Point(272, 8);
            bar.Controls.Add(search); bar.Controls.Add(kindFilter);
            var scroll = new Panel { Dock = DockStyle.Fill, AutoScroll = true };
            var cards = new List<Panel>(); var cardKind = new List<string>();
            string[][] catalog = {
                new[] { "Bans", "Bans players by account or address, with an optional expiry.", "Official", "0.1.0", "Install" },
                new[] { "Message of the Day", "Shows a message to each player who joins.", "Official", "0.1.0", "Install" },
                new[] { "Audit Log", "Writes who used which admin command, and when.", "Official", "0.1.0", "Install" },
                new[] { "MIDI Jukebox", "Play MIDI music with any game sound.", "Official", "0.2.0", "Installed" },
                new[] { "Flight", "Lets players fly with a key.", "Community", "0.3.1", "Install" } };
            for (int i = catalog.Length - 1; i >= 0; i--)
            {
                var c = catalog[i];
                // Each plugin is a rounded card on a gap of its own.
                var wrap = new Panel { Dock = DockStyle.Top, Height = 92, Padding = new Padding(0, 0, 0, 10) };
                var card = new Surface { Dock = DockStyle.Fill };
                var avatar = new Avatar(c[0]) { Location = new Point(16, 16) };
                var name = L(c[0], true, false); name.Font = UI.F(10.5f, FontStyle.Bold); name.Location = new Point(76, 14);
                var chip = Chip(c[2]); chip.Location = new Point(76 + name.PreferredWidth + 10, 12);
                var desc = L(c[1], false, true); desc.Location = new Point(76, 42);
                var ver = L("v" + c[3], false, true);
                Control action;
                if (c[4] == "Installed") { var done = L("✓ Installed", false, false); done.Tag = "ok"; action = done; }
                else action = new FlatButton("Install") { Glyph = "download" };
                card.Controls.AddRange(new Control[] { avatar, name, chip, desc, ver, action });
                Action place = delegate { action.Location = new Point(card.Width - action.Width - 18, (card.Height - action.Height) / 2); ver.Location = new Point(action.Left - ver.Width - 18, (card.Height - ver.Height) / 2); };
                card.Resize += delegate { place(); };
                action.SizeChanged += delegate { place(); };
                wrap.Controls.Add(card); scroll.Controls.Add(wrap); cards.Add(wrap); cardKind.Add(c[2]);
            }
            kindFilter.SelectedIndexChanged += delegate
            {
                string want = kindFilter.SelectedIndex == 1 ? "Official" : kindFilter.SelectedIndex == 2 ? "Community" : null;
                for (int i = 0; i < cards.Count; i++) cards[i].Visible = want == null || cardKind[i] == want;
            };
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
                    e.Paint(e.ClipBounds, DataGridViewPaintParts.Background | DataGridViewPaintParts.SelectionBackground | DataGridViewPaintParts.Border);
                    var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; int my = e.CellBounds.Y + e.CellBounds.Height / 2;
                    if (e.ColumnIndex == 2)
                    {
                        Color sc = pl.State == "Active" ? UI.T.Ok : pl.State == "Restart needed" ? UI.T.Warn : UI.T.Muted;
                        using (var b = new SolidBrush(sc)) g.FillEllipse(b, e.CellBounds.X + 6, my - 4, 8, 8);
                        TextRenderer.DrawText(g, pl.State, UI.F(9.5f, FontStyle.Regular), new Rectangle(e.CellBounds.X + 20, e.CellBounds.Y, e.CellBounds.Width - 20, e.CellBounds.Height), UI.T.Fg, TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding | TextFormatFlags.EndEllipsis);
                    }
                    else
                    {
                        Color rc = pl.Risk == "low" ? UI.T.Ok : pl.Risk == "high" ? UI.T.Warn : UI.T.Bad; var rf = UI.F(9f, FontStyle.Regular);
                        int rw = TextRenderer.MeasureText(g, pl.Risk, rf, new Size(999, 30), TextFormatFlags.NoPadding).Width + 20; var pill = new Rectangle(e.CellBounds.X + 6, my - 11, rw, 22);
                        using (var path = UI.Round(pill, 11)) using (var b = new SolidBrush(Icons.Blend(UI.T.Panel, rc, 0.16))) g.FillPath(b, path);
                        TextRenderer.DrawText(g, pl.Risk, rf, pill, rc, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
                    }
                    e.Handled = true; return;
                }
                if (e.ColumnIndex != 5) return;
                e.Paint(e.ClipBounds, DataGridViewPaintParts.Background | DataGridViewPaintParts.SelectionBackground | DataGridViewPaintParts.Border);
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
            var flow = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false, Width = 340, Padding = new Padding(0, 14, 0, 0), Tag = "panel" };
            // The icon of the plugin comes from its package. The first letter stands in when there is none.
            var header = new Panel { Width = 340, Height = 56, Tag = "panel", Margin = new Padding(0, 0, 0, 8) };
            var title = L(p.Name, true, false); title.Font = UI.F(13f, FontStyle.Bold); title.Location = new Point(56, 5);
            var sub = L(p.Version + " · " + p.By, false, true); sub.Location = new Point(56, 31);
            header.Controls.Add(new Avatar(p.Name) { Location = new Point(0, 6) }); header.Controls.Add(title); header.Controls.Add(sub); flow.Controls.Add(header);
            if (detailTab == 0)
            {
                var d = L(p.Desc, false, false); d.MaximumSize = new Size(330, 0); d.Margin = new Padding(0, 0, 0, 12); flow.Controls.Add(d);
                if (p.Risk == "highest")
                {
                    var n = L("This plugin has full access to the server. It can read private data, freeze it or crash it.", false, false);
                    n.MaximumSize = new Size(330, 0); n.Tag = "warn"; n.Margin = new Padding(0, 0, 0, 12); flow.Controls.Add(n);
                }
                var acts = new FlowLayoutPanel { AutoSize = true, Tag = "panel", Margin = new Padding(0, 4, 0, 0) };
                acts.Controls.Add(new FlatButton("Update…")); acts.Controls.Add(new FlatButton("Remove") { Kind = "danger", Margin = new Padding(0) });
                flow.Controls.Add(acts);
            }
            else if (detailTab == 1)
            {
                var tbl = new TableLayoutPanel { AutoSize = true, ColumnCount = 2, Tag = "panel", Margin = new Padding(0, 0, 0, 12) };
                tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 100)); tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 230));
                string[][] info = { new[] { "ID", p.Id }, new[] { "Version", p.Version }, new[] { "Author", p.By }, new[] { "Source", p.Source }, new[] { "Risk", p.Risk }, new[] { "Requires", p.Needs ?? "Nothing" } };
                foreach (var kv in info)
                {
                    int row = tbl.RowCount++; tbl.RowStyles.Add(new RowStyle(SizeType.Absolute, 28));
                    var k = L(kv[0], false, true); k.Anchor = AnchorStyles.Left; tbl.Controls.Add(k, 0, row);
                    var v = L(kv[1], false, false); v.Anchor = AnchorStyles.Left; tbl.Controls.Add(v, 1, row);
                }
                flow.Controls.Add(tbl);
                var cl = L("This plugin can", true, false); cl.Margin = new Padding(0, 4, 0, 6); flow.Controls.Add(cl);
                foreach (var c in p.Caps)
                {
                    string what; if (!CapText.TryGetValue(c, out what)) what = c;
                    var line = L("•  " + what, false, true); line.MaximumSize = new Size(330, 0); line.Margin = new Padding(0, 0, 0, 3); flow.Controls.Add(line);
                }
            }
            else if (p.Settings.Length == 0)
            {
                var none = L("This plugin has no settings.", false, true); flow.Controls.Add(none);
            }
            else
            {
                var tbl = new TableLayoutPanel { AutoSize = true, ColumnCount = 2, Tag = "panel", Margin = new Padding(0, 0, 0, 12) };
                tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 140)); tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 190));
                var save = new FlatButton("Save") { Kind = "primary", Enabled = false, Margin = new Padding(0, 0, 8, 0) };
                var revert = new FlatButton("Revert") { Enabled = false, Margin = new Padding(0) };
                var done = L("Saved ✓", false, false); done.Tag = "ok"; done.Visible = false; done.Margin = new Padding(12, 8, 0, 0);
                foreach (var s in p.Settings)
                {
                    int row = tbl.RowCount++; tbl.RowStyles.Add(new RowStyle(SizeType.Absolute, 36));
                    var lab = L(s[0], false, false); lab.Anchor = AnchorStyles.Left; tbl.Controls.Add(lab, 0, row);
                    var tb = TB(s[1], 180); tbl.Controls.Add(tb, 1, row);
                    tb.TextChanged += delegate { save.Enabled = true; revert.Enabled = true; done.Visible = false; save.Invalidate(); revert.Invalidate(); };
                }
                flow.Controls.Add(tbl);
                // The settings of a plugin are saved with a button.
                save.Click += delegate { save.Enabled = false; revert.Enabled = false; done.Visible = true; save.Invalidate(); revert.Invalidate(); };
                revert.Click += delegate { RenderDetail(); };
                var bar = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Tag = "panel", Margin = new Padding(0) };
                bar.Controls.Add(save); bar.Controls.Add(revert); bar.Controls.Add(done); flow.Controls.Add(bar);
            }
            detail.Controls.Add(flow);
            var strip = new Panel { Dock = DockStyle.Top, Height = 38, Tag = "panel" };
            string[] tabNames = { "Summary", "Info", "Settings" };
            var tabItems = new List<TabLabel>();
            for (int i = tabNames.Length - 1; i >= 0; i--)
            {
                var tab = new TabLabel(tabNames[i]) { Sub = true, Selected = i == detailTab }; int index = i;
                tab.Click += delegate { detailTab = index; RenderDetail(); };
                strip.Controls.Add(tab);
            }
            detail.Controls.Add(strip);
            Apply(detail);
            detail.ResumeLayout();
        }

        int detailTab;

        static readonly Dictionary<string, string> CapText = new Dictionary<string, string> {
            { "read", "Read game data" }, { "write", "Change the game" }, { "storage", "Save its own files" }, { "props", "Change props and objects" },
            { "config", "Read and change server settings" }, { "plugins", "Control other plugins" }, { "ai", "Control the AI characters" },
            { "economy", "Change money" }, { "mission", "Control missions" }, { "settings", "Change the game rules" }, { "engine", "Change engine behavior" },
            { "shop", "Change the shops" }, { "network", "Send and receive network data" } };

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
            new[] { "Plugins", "# Plugins\nPlugins add features to the server.\n## Risk\n- **low**: reads data.\n- **high**: changes the game.\n- **highest**: runs inside the server with full access to it.\nTurn on only plugins that you trust.\n## Install\nOpen **Catalog** to find plugins, or use `Install from File…` for a `.svplugin` file. The launcher shows what the plugin can do before it installs.\n" },
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
                            // The form paints itself into the picture, so nothing else on the screen can show up in it.
                            using (var bmp = new Bitmap(form.Width, form.Height))
                            {
                                // Rich text boxes (the console and the help text) do not draw themselves into a picture and stay blank.
                                form.DrawToBitmap(bmp, new Rectangle(Point.Empty, form.Size));
                                bmp.Save(Path.Combine(dir, tn[t] + "-" + name + ".png"));
                            }
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
