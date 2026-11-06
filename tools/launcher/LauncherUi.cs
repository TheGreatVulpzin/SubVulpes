// The look of the launcher: colors, icons and the flat controls. Every control paints itself, so the whole window follows the theme.
// The icons are from Lucide (https://lucide.dev, ISC license, Copyright (c) Lucide Icons and Contributors).
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Globalization;
using System.Drawing.Drawing2D;
using System.Windows.Forms;

namespace SubVulpes.Ui
{
    class Theme
    {
        // AccentFg is the accent for text. Accent alone is for lines and shapes. OkFill is the fill behind white text.
        public Color Bg, Win, Panel, Line, Fg, Muted, Accent, AccentFg, Ok, OkFill, Warn, Bad, Chip, Console, ConsoleFg, Bar, BarFg, BarMuted, SubBar;
        // Warm neutrals go with the orange. Every text color passes a contrast of 4.5 on the window, the panel and the chip.
        // A new accent color for lines and shapes. The accent for text follows it, darker on the light theme and lighter on the dark one,
        // until it has a contrast of 4.5 on the window, the panel and the chip.
        public void SetAccent(Color accent)
        {
            Accent = accent;
            bool light = this == Light;
            Color text = accent;
            for (int step = 0; step < 40 && Worst(text) < 4.6; step++)
                text = Color.FromArgb((int)(text.R + ((light ? 0 : 255) - text.R) * 0.06), (int)(text.G + ((light ? 0 : 255) - text.G) * 0.06), (int)(text.B + ((light ? 0 : 255) - text.B) * 0.06));
            AccentFg = text;
        }

        double Worst(Color text)
        {
            return Math.Min(Contrast(text, Win), Math.Min(Contrast(text, Panel), Contrast(text, Chip)));
        }

        public static double Contrast(Color a, Color b)
        {
            double la = Luminance(a), lb = Luminance(b);
            return (Math.Max(la, lb) + 0.05) / (Math.Min(la, lb) + 0.05);
        }

        static double Luminance(Color c)
        {
            Func<int, double> channel = v => { double x = v / 255.0; return x <= 0.03928 ? x / 12.92 : Math.Pow((x + 0.055) / 1.055, 2.4); };
            return 0.2126 * channel(c.R) + 0.7152 * channel(c.G) + 0.0722 * channel(c.B);
        }

        public static readonly Theme Light = new Theme {
            Bg = Color.FromArgb(238, 235, 230), Win = Color.FromArgb(246, 244, 240), Panel = Color.White, Line = Color.FromArgb(220, 215, 207),
            Fg = Color.FromArgb(32, 29, 26), Muted = Color.FromArgb(104, 97, 89), Accent = Color.FromArgb(217, 99, 30), AccentFg = Color.FromArgb(172, 66, 10),
            Ok = Color.FromArgb(26, 112, 54), OkFill = Color.FromArgb(26, 112, 54), Warn = Color.FromArgb(136, 84, 6), Bad = Color.FromArgb(176, 38, 28),
            Chip = Color.FromArgb(238, 234, 228), Console = Color.FromArgb(226, 223, 217), ConsoleFg = Color.FromArgb(38, 35, 32),
            Bar = Color.FromArgb(228, 223, 215), SubBar = Color.FromArgb(238, 234, 228), BarFg = Color.White, BarMuted = Color.FromArgb(92, 85, 77) };
        public static readonly Theme Dark = new Theme {
            Bg = Color.FromArgb(17, 16, 15), Win = Color.FromArgb(27, 26, 24), Panel = Color.FromArgb(36, 34, 32), Line = Color.FromArgb(58, 55, 51),
            Fg = Color.FromArgb(236, 233, 228), Muted = Color.FromArgb(165, 158, 150), Accent = Color.FromArgb(240, 138, 75), AccentFg = Color.FromArgb(244, 150, 92),
            Ok = Color.FromArgb(94, 194, 125), OkFill = Color.FromArgb(36, 128, 68), Warn = Color.FromArgb(224, 176, 80), Bad = Color.FromArgb(239, 111, 100),
            Chip = Color.FromArgb(47, 44, 41), Console = Color.FromArgb(34, 32, 30), ConsoleFg = Color.FromArgb(205, 208, 212),
            Bar = Color.FromArgb(43, 41, 38), SubBar = Color.FromArgb(33, 31, 29), BarFg = Color.White, BarMuted = Color.FromArgb(165, 158, 150) };
    }

    static class UI
    {
        public static Theme T = Theme.Light;
        public static readonly ToolTip Tips = new ToolTip { InitialDelay = 250, ShowAlways = true, AutoPopDelay = 12000 };
        // Windows stresses text with Semibold. The Bold face is too heavy for a small interface.
        public static Font F(float size, FontStyle style)
        {
            if ((style & FontStyle.Bold) != 0) return new Font("Segoe UI Semibold", size, style & ~FontStyle.Bold, GraphicsUnit.Point);
            return new Font("Segoe UI", size, style, GraphicsUnit.Point);
        }
        // One corner radius for controls, and one for the panels that hold them.
        public const int Radius = 3, PanelRadius = 4;
        // The ring of a control that has the keyboard focus. It shows only after a key press, like the system controls.
        public static void FocusRing(Graphics g, Rectangle r)
        {
            using (var pen = new Pen(T.Accent) { DashStyle = DashStyle.Dot }) g.DrawRectangle(pen, r);
        }
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
            SetStyle(UI.Paint | ControlStyles.Selectable, true); TabStop = true;
            AccessibleRole = AccessibleRole.PushButton; AccessibleName = text;
            Width = TextRenderer.MeasureText(text, Font).Width + 32;
        }
        protected override void OnTextChanged(EventArgs e) { AccessibleName = Text; base.OnTextChanged(e); }
        protected override void OnMouseEnter(EventArgs e) { hover = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hover = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnGotFocus(EventArgs e) { Invalidate(); base.OnGotFocus(e); }
        protected override void OnLostFocus(EventArgs e) { Invalidate(); base.OnLostFocus(e); }
        protected override void OnEnabledChanged(EventArgs e) { Invalidate(); base.OnEnabledChanged(e); }
        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (Enabled && (e.KeyCode == Keys.Space || e.KeyCode == Keys.Return)) { e.Handled = true; OnClick(EventArgs.Empty); }
            base.OnKeyDown(e);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias;
            Color back = Parent != null ? Parent.BackColor : UI.T.Win;
            g.Clear(back);
            var r = new Rectangle(0, 0, Width - 1, Height - 1);
            Color fill = UI.T.Panel, border = UI.T.Line, fg = UI.T.Fg;
            if (Kind == "primary") { fill = UI.T.OkFill; border = UI.T.OkFill; fg = Color.White; }
            else if (Kind == "danger") { border = UI.T.Bad; fg = UI.T.Bad; }
            if (Kind == "link") { fill = back; border = back; fg = hover ? UI.T.Bad : UI.T.Muted; }
            else if (!Enabled) { fill = back; border = Icons.Blend(back, UI.T.Line, 0.7); fg = Icons.Blend(back, UI.T.Muted, 0.75); }
            else if (hover) border = UI.T.Accent;
            using (var path = UI.Round(r, UI.Radius)) using (var b = new SolidBrush(fill)) using (var p = new Pen(border)) { g.FillPath(b, path); g.DrawPath(p, path); }
            if (glyph == null) TextRenderer.DrawText(g, Text, Font, r, fg, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
            else
            {
                int tw = TextRenderer.MeasureText(g, Text, Font, new Size(999, 40), TextFormatFlags.NoPadding).Width, start = (Width - (16 + 6 + tw)) / 2;
                Icons.Draw(g, glyph, start, (Height - 16) / 2, fg);
                TextRenderer.DrawText(g, Text, Font, new Rectangle(start + 22, 0, tw + 4, Height), fg, TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
            }
            if (Focused && ShowFocusCues && Enabled) UI.FocusRing(g, new Rectangle(3, 3, Width - 7, Height - 7));
        }
    }

    class Switch : Control
    {
        bool on;
        public event EventHandler Changed;
        public Switch()
        {
            Size = new Size(40, 22); Cursor = Cursors.Hand; SetStyle(UI.Paint | ControlStyles.Selectable, true); TabStop = true;
            AccessibleRole = AccessibleRole.CheckButton;
        }
        public bool On { get { return on; } set { on = value; Invalidate(); } }
        protected override void OnClick(EventArgs e) { on = !on; Invalidate(); if (Changed != null) Changed(this, EventArgs.Empty); base.OnClick(e); }
        protected override void OnGotFocus(EventArgs e) { Invalidate(); base.OnGotFocus(e); }
        protected override void OnLostFocus(EventArgs e) { Invalidate(); base.OnLostFocus(e); }
        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (Enabled && e.KeyCode == Keys.Space) { e.Handled = true; OnClick(EventArgs.Empty); }
            base.OnKeyDown(e);
        }
        // A screen reader reads the switch as a check box, and it needs the checked state.
        protected override AccessibleObject CreateAccessibilityInstance() { return new SwitchAccess(this); }
        sealed class SwitchAccess : ControlAccessibleObject
        {
            readonly Switch owner;
            public SwitchAccess(Switch owner) : base(owner) { this.owner = owner; }
            public override AccessibleStates State { get { return owner.on ? base.State | AccessibleStates.Checked : base.State; } }
        }
        public static void Draw(Graphics g, Rectangle r, bool isOn)
        {
            g.SmoothingMode = SmoothingMode.AntiAlias;
            using (var path = UI.Round(r, r.Height / 2)) using (var b = new SolidBrush(isOn ? UI.T.OkFill : Icons.Blend(UI.T.Line, UI.T.Muted, 0.35))) g.FillPath(b, path);
            int k = r.Height - 6;
            using (var b = new SolidBrush(Color.White)) g.FillEllipse(b, isOn ? r.Right - k - 3 : r.X + 3, r.Y + 3, k, k);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            e.Graphics.Clear(Parent != null ? Parent.BackColor : UI.T.Win);
            Draw(e.Graphics, new Rectangle(0, 0, Width - 1, Height - 1), on);
            if (Focused && ShowFocusCues) UI.FocusRing(e.Graphics, new Rectangle(-1, -1, Width + 1, Height + 1));
        }
    }

    class Dropdown : Control
    {
        public readonly List<object> Items = new List<object>();
        int sel = -1; bool hot, open;
        public event EventHandler SelectedIndexChanged;
        public Dropdown(int width)
        {
            Width = width; Height = 28; Margin = new Padding(0); Cursor = Cursors.Hand; Font = UI.F(9.5f, FontStyle.Regular);
            SetStyle(UI.Paint | ControlStyles.Selectable, true); TabStop = true; AccessibleRole = AccessibleRole.ComboBox;
        }
        protected override bool IsInputKey(Keys keyData) { return keyData == Keys.Up || keyData == Keys.Down || base.IsInputKey(keyData); }
        protected override void OnGotFocus(EventArgs e) { Invalidate(); base.OnGotFocus(e); }
        protected override void OnLostFocus(EventArgs e) { Invalidate(); base.OnLostFocus(e); }
        // Up and Down change the choice. Space, Enter, F4 and Alt+Down open the list.
        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (Enabled) {
                if (e.KeyCode == Keys.Down && !e.Alt && sel < Items.Count - 1) { SelectedIndex = sel + 1; e.Handled = true; }
                else if (e.KeyCode == Keys.Up && sel > 0) { SelectedIndex = sel - 1; e.Handled = true; }
                else if (e.KeyCode == Keys.Space || e.KeyCode == Keys.Return || e.KeyCode == Keys.F4 || (e.Alt && e.KeyCode == Keys.Down)) { e.Handled = true; OnClick(EventArgs.Empty); }
            }
            base.OnKeyDown(e);
        }
        protected override AccessibleObject CreateAccessibilityInstance() { return new DropdownAccess(this); }
        sealed class DropdownAccess : ControlAccessibleObject
        {
            readonly Dropdown owner;
            public DropdownAccess(Dropdown owner) : base(owner) { this.owner = owner; }
            public override string Value { get { return owner.SelectedItem == null ? "" : owner.SelectedItem.ToString(); } }
        }
        public int SelectedIndex
        {
            get { return sel; }
            set { if (sel == value) return; sel = value; Invalidate(); if (SelectedIndexChanged != null) SelectedIndexChanged(this, EventArgs.Empty); }
        }
        public object SelectedItem
        {
            get { return sel >= 0 && sel < Items.Count ? Items[sel] : null; }
            set { SelectedIndex = Items.IndexOf(value); }
        }
        protected override void OnEnabledChanged(EventArgs e) { Invalidate(); base.OnEnabledChanged(e); }
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
                var it = new ToolStripMenuItem(Items[i].ToString()) { ForeColor = i == sel ? UI.T.AccentFg : UI.T.Fg, Padding = new Padding(2, 5, 2, 5) };
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
            using (var path = UI.Round(r, UI.Radius)) using (var b = new SolidBrush(Enabled ? UI.T.Panel : UI.T.Chip)) using (var p = new Pen(Enabled && (hot || open) ? UI.T.Accent : UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
            if (Focused && ShowFocusCues && Enabled) UI.FocusRing(g, new Rectangle(3, 3, Width - 7, Height - 7));
            string text = sel >= 0 && sel < Items.Count ? Items[sel].ToString() : "";
            TextRenderer.DrawText(g, text, Font, new Rectangle(10, 0, Width - 32, Height), Enabled ? UI.T.Fg : UI.T.Muted, TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPadding);
            using (var pen = new Pen(UI.T.Muted, 1.6f) { StartCap = LineCap.Round, EndCap = LineCap.Round })
                g.DrawLines(pen, new[] { new Point(Width - 20, Height / 2 - 2), new Point(Width - 15, Height / 2 + 3), new Point(Width - 10, Height / 2 - 2) });
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
        protected override void OnEnabledChanged(EventArgs e) { base.OnEnabledChanged(e); if (Box != null) { Box.Enabled = Enabled; Box.BackColor = Enabled ? UI.T.Panel : UI.T.Chip; } Invalidate(); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            using (var path = UI.Round(new Rectangle(0, 0, Width - 1, Height - 1), UI.Radius)) using (var b = new SolidBrush(Enabled ? UI.T.Panel : UI.T.Chip)) using (var p = new Pen(Box.Focused ? UI.T.Accent : UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
        }
    }

    // A text field for whole numbers. It keeps the value between Minimum and Maximum.
    class NumberField : TextField
    {
        public decimal Minimum = 0, Maximum = 100;
        public event EventHandler ValueChanged;
        public NumberField(int width) : base(width, false)
        {
            Box.KeyPress += delegate (object s, KeyPressEventArgs k) { if (!char.IsControl(k.KeyChar) && !char.IsDigit(k.KeyChar) && !(k.KeyChar == '-' && Minimum < 0)) k.Handled = true; };
            Box.Leave += delegate { Value = Value; };
            Box.TextChanged += delegate { if (ValueChanged != null) ValueChanged(this, EventArgs.Empty); };
        }
        public decimal Value
        {
            get
            {
                decimal v;
                if (!decimal.TryParse(Box.Text, System.Globalization.NumberStyles.AllowLeadingSign, System.Globalization.CultureInfo.InvariantCulture, out v)) v = Minimum;
                return Math.Max(Minimum, Math.Min(Maximum, v));
            }
            set { Box.Text = Math.Max(Minimum, Math.Min(Maximum, value)).ToString(System.Globalization.CultureInfo.InvariantCulture); }
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
            using (var path = UI.Round(new Rectangle(0, 0, Width - 1, Height - 1), UI.Radius)) using (var b = new SolidBrush(UI.T.Panel)) using (var p = new Pen(Box.Focused ? UI.T.Accent : UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
            Svg.Draw(g, "search", new RectangleF(9, (Height - 15) / 2f, 15, 15), UI.T.Muted);
        }
    }

    class Segmented : Control
    {
        public string[] Options; int sel; int[] widths;
        public event EventHandler Changed;
        public int SelectedIndex
        {
            get { return sel; }
            set { if (sel == value) return; sel = value; Invalidate(); if (Changed != null) Changed(this, EventArgs.Empty); }
        }
        public Segmented(params string[] options)
        {
            Options = options; Height = 32; Cursor = Cursors.Hand; Margin = new Padding(0); SetStyle(UI.Paint | ControlStyles.Selectable, true); TabStop = true;
            AccessibleRole = AccessibleRole.Grouping;
            widths = new int[options.Length]; int total = 0;
            for (int i = 0; i < options.Length; i++) { widths[i] = TextRenderer.MeasureText(options[i], UI.F(9.5f, FontStyle.Regular)).Width + 24; total += widths[i]; }
            Width = total + 4; sel = 1 < options.Length ? 1 : 0;
        }
        protected override bool IsInputKey(Keys keyData) { return keyData == Keys.Left || keyData == Keys.Right || base.IsInputKey(keyData); }
        protected override void OnGotFocus(EventArgs e) { Invalidate(); base.OnGotFocus(e); }
        protected override void OnLostFocus(EventArgs e) { Invalidate(); base.OnLostFocus(e); }
        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (Enabled && e.KeyCode == Keys.Left && sel > 0) { SelectedIndex = sel - 1; e.Handled = true; }
            else if (Enabled && e.KeyCode == Keys.Right && sel < Options.Length - 1) { SelectedIndex = sel + 1; e.Handled = true; }
            base.OnKeyDown(e);
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
            using (var path = UI.Round(new Rectangle(0, 0, Width - 1, Height - 1), UI.Radius)) using (var b = new SolidBrush(UI.T.Chip)) using (var p = new Pen(UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
            int x = 2;
            for (int i = 0; i < Options.Length; i++)
            {
                var r = new Rectangle(x, 2, widths[i], Height - 5);
                if (i == sel) using (var path = UI.Round(r, UI.Radius - 1)) using (var b = new SolidBrush(UI.T.Panel)) using (var p = new Pen(UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
                TextRenderer.DrawText(g, Options[i], UI.F(9.5f, FontStyle.Regular), r, i == sel ? UI.T.AccentFg : UI.T.Muted, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
                x += widths[i];
            }
            if (Focused && ShowFocusCues) UI.FocusRing(g, new Rectangle(1, 1, Width - 3, Height - 3));
        }
    }

    // A small accent colored pill with the restart icon. It tells that a setting needs a restart of the server.
    class Badge : Control
    {
        readonly string text;
        public Badge(string text)
        {
            this.text = text; Height = 22; SetStyle(UI.Paint, true); AccessibleRole = AccessibleRole.StaticText; AccessibleName = text;
            Width = 9 + 12 + 6 + TextRenderer.MeasureText(this.text, UI.F(9f, FontStyle.Regular), new Size(999, 30), TextFormatFlags.NoPadding).Width + 11;
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            using (var path = UI.Round(new Rectangle(0, 0, Width - 1, Height - 1), UI.Radius)) using (var b = new SolidBrush(Icons.Blend(UI.T.Win, UI.T.Accent, 0.12))) g.FillPath(b, path);
            Svg.Draw(g, "restart", new RectangleF(9, (Height - 12) / 2f, 12, 12), UI.T.AccentFg);
            TextRenderer.DrawText(g, text, UI.F(9f, FontStyle.Regular), new Rectangle(27, 0, Width - 27, Height), UI.T.AccentFg, TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
        }
    }

    // A tab. The main tabs have an accent underline. The second level has a thin neutral line. A quiet tab has no line and only changes its text.
    class TabLabel : Control
    {
        public bool Selected, Sub, Quiet;
        public TabLabel(string text)
        {
            Text = text; Font = UI.F(10f, FontStyle.Regular); Cursor = Cursors.Hand; Dock = DockStyle.Left;
            Width = Measure(text); SetStyle(UI.Paint | ControlStyles.Selectable, true); TabStop = true;
            AccessibleRole = AccessibleRole.PageTab; AccessibleName = text;
        }
        static int Measure(string text) { return TextRenderer.MeasureText(text, UI.F(10f, FontStyle.Bold)).Width + 40; }
        public void SetText(string text)
        {
            Text = text; AccessibleName = text; Width = Measure(text); Invalidate();
        }
        protected override void OnGotFocus(EventArgs e) { Invalidate(); base.OnGotFocus(e); }
        protected override void OnLostFocus(EventArgs e) { Invalidate(); base.OnLostFocus(e); }
        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (Enabled && (e.KeyCode == Keys.Space || e.KeyCode == Keys.Return)) { e.Handled = true; OnClick(EventArgs.Empty); }
            base.OnKeyDown(e);
        }
        protected override AccessibleObject CreateAccessibilityInstance() { return new TabAccess(this); }
        sealed class TabAccess : ControlAccessibleObject
        {
            readonly TabLabel owner;
            public TabAccess(TabLabel owner) : base(owner) { this.owner = owner; }
            public override AccessibleStates State { get { return owner.Selected ? base.State | AccessibleStates.Selected : base.State; } }
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.Clear(Parent.BackColor);
            int line = Quiet || Sub ? 2 : 3;
            Color idle = Sub || Quiet ? UI.T.Muted : UI.T.BarMuted;
            TextRenderer.DrawText(g, Text, Selected ? UI.F(10f, FontStyle.Bold) : Font, new Rectangle(0, 0, Width, Height - line),
                Selected ? UI.T.Fg : idle, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter);
            if (Selected && line > 0) using (var b = new SolidBrush(Sub || Quiet ? Icons.Blend(Parent.BackColor, UI.T.Muted, 0.4) : UI.T.Accent)) g.FillRectangle(b, 12, Height - line, Width - 24, line);
            if (Focused && ShowFocusCues) UI.FocusRing(g, new Rectangle(2, 2, Width - 5, Height - 5));
        }
    }

    // A panel with a thin line along its bottom edge. The tab row of the plugin page uses it.
    class RuledPanel : Panel
    {
        public RuledPanel() { SetStyle(ControlStyles.ResizeRedraw, true); }
        protected override void OnPaint(PaintEventArgs e)
        {
            base.OnPaint(e);
            using (var pen = new Pen(UI.T.Line)) e.Graphics.DrawLine(pen, 0, Height - 1, Width, Height - 1);
        }
    }

    // The toolbar of the Server page. A line in the color of the state joins the state to the buttons and fades to gray.
    class ToolbarLine : Panel
    {
        public Control From, To; public StateLabel State;
        public ToolbarLine() { SetStyle(UI.Paint | ControlStyles.ResizeRedraw, true); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.Clear(Parent != null ? Parent.BackColor : UI.T.Win);
            if (From == null || To == null || State == null) return;
            // The line ends at the edge of the first button (the panel has room before it). Both ends are soft.
            int x0 = From.Right + 10, x1 = To.Left + 16, y = 4 + 17;
            if (x1 - x0 < 12) return;
            Color back = g.GetNearestColor(Parent != null ? Parent.BackColor : UI.T.Win);
            Color start = Icons.Blend(back, State.ToneColor, 0.7), end = Icons.Blend(back, UI.T.Line, 0.8);
            using (var b = new LinearGradientBrush(new Rectangle(x0, y - 1, x1 - x0, 2), start, end, 0f)) g.FillRectangle(b, x0, y - 1, x1 - x0, 2);
        }
    }

    // The state of the server: a colored dot and a word. The color alone never tells the state.
    class StateLabel : Control
    {
        string word = "Running", tone = "ok";
        readonly System.Windows.Forms.Timer pulse = new System.Windows.Forms.Timer { Interval = 60 };
        int phase;
        // The owner repaints what depends on the color, such as the line next to the buttons.
        public event Action Changed;
        public StateLabel()
        {
            Height = 34; Fit(); SetStyle(UI.Paint, true); AccessibleRole = AccessibleRole.StaticText; AccessibleName = word;
            pulse.Tick += delegate { phase = (phase + 1) % 34; Invalidate(); };
        }
        // The dot and the line use brighter colors than the text colors of the theme. The word beside the dot carries the meaning.
        public Color ToneColor
        {
            get
            {
                bool dark = UI.T == Theme.Dark;
                return tone == "ok" ? (dark ? Color.FromArgb(70, 190, 105) : Color.FromArgb(38, 160, 78))
                     : tone == "warn" ? (dark ? Color.FromArgb(245, 180, 40) : Color.FromArgb(232, 160, 0))
                     : tone == "bad" ? (dark ? Color.FromArgb(240, 100, 90) : Color.FromArgb(214, 58, 46)) : UI.T.Muted;
            }
        }
        // Tone is ok (green), warn (orange), bad (red) or muted (gray). The dot pulses while the server runs or starts.
        public void Set(string text, string toneName)
        {
            word = text; tone = toneName; AccessibleName = text; Fit();
            bool pulsing = text == "Running" || text == "Starting";
            if (pulsing != pulse.Enabled) { pulse.Enabled = pulsing; phase = 0; }
            Invalidate();
            if (Changed != null) Changed();
        }
        protected override void Dispose(bool disposing) { if (disposing) pulse.Dispose(); base.Dispose(disposing); }
        void Fit() { Width = 25 + TextRenderer.MeasureText(word, UI.F(10.5f, FontStyle.Bold), new Size(999, 40), TextFormatFlags.NoPadding).Width + 8; }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            Color c = ToneColor;
            // While the server runs or starts, the dot goes from its color to a lighter one and back, once each two seconds.
            if (pulse.Enabled) c = Icons.Blend(c, Color.White, 0.5 * (1 - Math.Cos(phase / 34.0 * 2 * Math.PI)) / 2);
            using (var b = new SolidBrush(c)) g.FillEllipse(b, 4, Height / 2 - 6, 13, 13);
            TextRenderer.DrawText(g, word, UI.F(10.5f, FontStyle.Bold), new Rectangle(25, 0, Width - 25, Height), UI.T.Fg, TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
        }
    }

    class Surface : Panel
    {
        public Surface() { SetStyle(UI.Paint, true); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            using (var path = UI.Round(new Rectangle(0, 0, Width - 1, Height - 1), UI.PanelRadius)) using (var b = new SolidBrush(UI.T.Panel)) using (var p = new Pen(UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
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
        public IconButton(string glyph)
        {
            Glyph = glyph; Size = new Size(28, 28); Cursor = Cursors.Hand; SetStyle(UI.Paint | ControlStyles.Selectable, true); TabStop = true;
            AccessibleRole = AccessibleRole.PushButton;
        }
        // The name for a screen reader and the tooltip are the same words.
        public string Caption { set { AccessibleName = value; UI.Tips.SetToolTip(this, value); } }
        protected override void OnMouseEnter(EventArgs e) { hot = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hot = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnGotFocus(EventArgs e) { Invalidate(); base.OnGotFocus(e); }
        protected override void OnLostFocus(EventArgs e) { Invalidate(); base.OnLostFocus(e); }
        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (Enabled && (e.KeyCode == Keys.Space || e.KeyCode == Keys.Return)) { e.Handled = true; OnClick(EventArgs.Empty); }
            base.OnKeyDown(e);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(UI.T.Console);
            var r = new Rectangle(0, 0, Width - 1, Height - 1);
            using (var path = UI.Round(r, UI.Radius)) using (var b = new SolidBrush(Color.FromArgb(hot ? 55 : 20, UI.T.ConsoleFg))) using (var p = new Pen(Color.FromArgb(hot ? 110 : 60, UI.T.ConsoleFg))) { g.FillPath(b, path); g.DrawPath(p, path); }
            Svg.Draw(g, Glyph, new RectangleF(6, 6, 16, 16), Color.FromArgb(hot ? 255 : 190, UI.T.ConsoleFg));
            if (Focused && ShowFocusCues) UI.FocusRing(g, new Rectangle(2, 2, Width - 5, Height - 5));
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
            using (var path = UI.Round(new Rectangle(0, 0, w - 1, h - 1), UI.PanelRadius)) using (var b = new SolidBrush(UI.T.Panel)) using (var p = new Pen(UI.T.Line)) { g.FillPath(b, path); g.DrawPath(p, path); }
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

    // The speed of the server for the last 20 minutes. A line on even grid lines, and the current value as text in the corner.
    class AreaChart : Card
    {
        public const int Slots = 20;
        public double[] Values = new double[0];
        public AreaChart() : base("Tick rate", "", "") { Margin = new Padding(0); AccessibleRole = AccessibleRole.Graphic; AccessibleName = "Tick rate of the last 20 minutes"; }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; Frame(g, Width, Height, Parent);
            TextRenderer.DrawText(g, Title, UI.F(8.5f, FontStyle.Regular), new Point(14, 12), UI.T.Muted, TextFormatFlags.NoPadding);
            int n = Values.Length;
            if (n > 0) TextRenderer.DrawText(g, Values[n - 1].ToString("0.0", CultureInfo.InvariantCulture) + " ticks/s", UI.F(10f, FontStyle.Bold),
                new Rectangle(Width - 160, 8, 146, 20), UI.T.Fg, TextFormatFlags.Right | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
            int left = 46, right = Width - 20, top = 40, bottom = Height - 34;
            if (right - left < 40 || bottom - top < 20) return;
            // The scale holds 58 to 64 at least. A lower or higher value widens it. The lines sit at even steps, about one for each 28 pixels.
            double lo = 58, hi = 64;
            foreach (double v in Values) { lo = Math.Min(lo, v); hi = Math.Max(hi, v); }
            double raw = (hi - lo) / Math.Max(2, (bottom - top) / 28), step = 100;
            foreach (double candidate in new double[] { 1, 2, 5, 10, 20, 50 }) if (candidate >= raw) { step = candidate; break; }
            lo = Math.Floor(lo / step) * step;
            hi = Math.Ceiling(hi / step) * step;
            // The dashed line is the usual speed of the server.
            if (lo < 61 && hi > 61) {
                int y61 = bottom - (int)((61 - lo) / (hi - lo) * (bottom - top));
                using (var pen = new Pen(Color.FromArgb(160, UI.T.Line)) { DashStyle = DashStyle.Dash }) g.DrawLine(pen, left, y61, right, y61);
            }
            for (double m = lo; m <= hi + 0.001; m += step)
            {
                int y = bottom - (int)((m - lo) / (hi - lo) * (bottom - top));
                using (var pen = new Pen(Color.FromArgb(110, UI.T.Line))) g.DrawLine(pen, left, y, right, y);
                TextRenderer.DrawText(g, m.ToString("0", CultureInfo.InvariantCulture), UI.F(8f, FontStyle.Regular), new Rectangle(8, y - 8, left - 14, 16), UI.T.Muted, TextFormatFlags.Right | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
            }
            if (n < 2) return;
            // One slot for each minute, newest on the right. A short history grows from the right and does not stretch.
            var pts = new PointF[n];
            for (int i = 0; i < n; i++) pts[i] = new PointF(right - (n - 1 - i) * (right - left) / (float)(Slots - 1), bottom - (float)((Values[i] - lo) / (hi - lo)) * (bottom - top));
            // The curve may overshoot between points. The clip keeps it inside the plot.
            g.SetClip(new Rectangle(left - 4, top - 3, right - left + 8, bottom - top + 6));
            using (var pen = new Pen(UI.T.Accent, 2f) { LineJoin = LineJoin.Round }) g.DrawCurve(pen, pts, 0.3f);
            g.ResetClip();
            var last = pts[n - 1];
            using (var b = new SolidBrush(UI.T.Panel)) g.FillEllipse(b, last.X - 5, last.Y - 5, 10, 10);
            using (var b = new SolidBrush(UI.T.Accent)) g.FillEllipse(b, last.X - 3.5f, last.Y - 3.5f, 7, 7);
            TextRenderer.DrawText(g, "20 min ago", UI.F(8f, FontStyle.Regular), new Point(left, bottom + 10), UI.T.Muted, TextFormatFlags.NoPadding);
            TextRenderer.DrawText(g, "now", UI.F(8f, FontStyle.Regular), new Rectangle(right - 40, bottom + 10, 40, 14), UI.T.Muted, TextFormatFlags.Right | TextFormatFlags.NoPadding);
        }
    }

    class GearButton : Control
    {
        bool hot;
        public GearButton()
        {
            Size = new Size(24, 24); Cursor = Cursors.Hand; SetStyle(UI.Paint | ControlStyles.Selectable, true); TabStop = true;
            AccessibleRole = AccessibleRole.PushButton; AccessibleName = "Dashboard settings";
        }
        protected override void OnMouseEnter(EventArgs e) { hot = true; Invalidate(); base.OnMouseEnter(e); }
        protected override void OnMouseLeave(EventArgs e) { hot = false; Invalidate(); base.OnMouseLeave(e); }
        protected override void OnGotFocus(EventArgs e) { Invalidate(); base.OnGotFocus(e); }
        protected override void OnLostFocus(EventArgs e) { Invalidate(); base.OnLostFocus(e); }
        protected override void OnKeyDown(KeyEventArgs e)
        {
            if (Enabled && (e.KeyCode == Keys.Space || e.KeyCode == Keys.Return)) { e.Handled = true; OnClick(EventArgs.Empty); }
            base.OnKeyDown(e);
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(UI.T.Panel);
            Svg.Draw(g, "sliders", new RectangleF(3, 3, 18, 18), hot ? UI.T.AccentFg : UI.T.Muted);
            if (Focused && ShowFocusCues) UI.FocusRing(g, new Rectangle(0, 0, Width - 1, Height - 1));
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
}
