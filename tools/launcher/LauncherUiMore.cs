// More controls of the launcher look, and the code that applies a theme to a whole window.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace SubVulpes.Ui
{
    // The icon of a plugin on a rounded square. Without a picture, the first letter of its name stands in.
    class Avatar : Control
    {
        public Image Picture;
        public Avatar(string text) : this(text, 44) { }
        public Avatar(string text, int size) { Text = text.Length == 0 ? "?" : text; Size = new Size(size, size); SetStyle(UI.Paint, true); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.Clear(Parent.BackColor);
            var r = new Rectangle(0, 0, Width - 1, Height - 1);
            using (var path = UI.Round(r, Math.Max(10, Width / 5)))
            {
                if (Picture != null)
                {
                    g.SetClip(path); g.DrawImage(Picture, r); g.ResetClip();
                    using (var pen = new Pen(UI.T.Line)) g.DrawPath(pen, path);
                    return;
                }
                using (var b = new SolidBrush(Icons.Blend(UI.T.Panel, UI.T.Accent, 0.16))) g.FillPath(b, path);
            }
            TextRenderer.DrawText(g, Text.Substring(0, 1).ToUpperInvariant(), UI.F(Width * 0.32f, FontStyle.Bold), new Rectangle(0, 0, Width, Height), UI.T.Accent, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
        }
    }

    // A picture that fits in its box, on a rounded frame.
    class ImageBox : Control
    {
        public Image Picture;
        public ImageBox() { SetStyle(UI.Paint, true); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.InterpolationMode = InterpolationMode.HighQualityBicubic; g.Clear(Parent.BackColor);
            var r = new Rectangle(0, 0, Width - 1, Height - 1);
            using (var path = UI.Round(r, 8))
            {
                using (var b = new SolidBrush(UI.T.Chip)) g.FillPath(b, path);
                if (Picture != null)
                {
                    double scale = Math.Min((double)(Width - 2) / Picture.Width, (double)(Height - 2) / Picture.Height);
                    int w = (int)(Picture.Width * scale), h = (int)(Picture.Height * scale);
                    g.SetClip(path); g.DrawImage(Picture, new Rectangle((Width - w) / 2, (Height - h) / 2, w, h)); g.ResetClip();
                }
                using (var pen = new Pen(UI.T.Line)) g.DrawPath(pen, path);
            }
        }
    }

    // A small preview of a picture. The chosen one has an accent frame.
    class Thumb : Control
    {
        public Image Picture; public bool Selected;
        public Thumb() { Size = new Size(112, 68); Cursor = Cursors.Hand; SetStyle(UI.Paint, true); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.SmoothingMode = SmoothingMode.AntiAlias; g.InterpolationMode = InterpolationMode.HighQualityBicubic; g.Clear(Parent.BackColor);
            var r = new Rectangle(1, 1, Width - 3, Height - 3);
            using (var path = UI.Round(r, 6))
            {
                using (var b = new SolidBrush(UI.T.Chip)) g.FillPath(b, path);
                if (Picture != null)
                {
                    // The picture fills the box and the extra part is cut.
                    double scale = Math.Max((double)r.Width / Picture.Width, (double)r.Height / Picture.Height);
                    int sw = (int)(r.Width / scale), sh = (int)(r.Height / scale);
                    g.SetClip(path);
                    g.DrawImage(Picture, r, new Rectangle((Picture.Width - sw) / 2, (Picture.Height - sh) / 2, sw, sh), GraphicsUnit.Pixel);
                    g.ResetClip();
                }
                using (var pen = new Pen(Selected ? UI.T.Accent : UI.T.Line, Selected ? 2.5f : 1f)) g.DrawPath(pen, path);
            }
        }
    }

    // One entry of the list at the left of the Help page.
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

    // The name of the server in the right corner of the tab bar.
    class ServerName : Control
    {
        public ServerName(string text) { Dock = DockStyle.Right; SetText(text); SetStyle(UI.Paint, true); }
        public void SetText(string text) { Text = text; Width = TextRenderer.MeasureText(text, UI.F(10f, FontStyle.Bold)).Width + 28; Invalidate(); }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; g.Clear(Parent.BackColor);
            TextRenderer.DrawText(g, Text, UI.F(10f, FontStyle.Bold), new Rectangle(0, 0, Width - 8, Height), UI.T.BarMuted, TextFormatFlags.Right | TextFormatFlags.VerticalCenter);
        }
    }

    // A list of named values. The user chooses which rows show.
    class StatsList : Card
    {
        public readonly List<string[]> All = new List<string[]>();
        public readonly HashSet<string> Hidden = new HashSet<string>();
        public int TopPad;
        public StatsList() : base("", "", "") { }
        public void Set(string name, string value)
        {
            foreach (var row in All) if (row[0] == name) { row[1] = value; Invalidate(); return; }
            All.Add(new[] { name, value }); Invalidate();
        }
        protected override void OnPaint(PaintEventArgs e)
        {
            var g = e.Graphics; Frame(g, Width, Height, Parent);
            var items = new List<string[]>(); foreach (var row in All) if (!Hidden.Contains(row[0])) items.Add(row);
            if (items.Count == 0) return;
            int rowH = Math.Min(30, (Height - 8 - TopPad) / items.Count);
            for (int i = 0; i < items.Count; i++)
            {
                int y = 4 + TopPad + i * rowH;
                var vf = UI.F(12f, FontStyle.Bold); int vw = TextRenderer.MeasureText(g, items[i][1], vf, new Size(999, 40), TextFormatFlags.NoPadding).Width;
                TextRenderer.DrawText(g, items[i][0], UI.F(9.5f, FontStyle.Regular), new Rectangle(16, y, Math.Max(20, Width - 32 - vw - 12), rowH), UI.T.Muted, TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPadding);
                TextRenderer.DrawText(g, items[i][1], vf, new Rectangle(16, y, Width - 32, rowH), UI.T.Fg, TextFormatFlags.Right | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
                if (i < items.Count - 1) using (var pen = new Pen(Color.FromArgb(110, UI.T.Line))) g.DrawLine(pen, 14, y + rowH, Width - 16, y + rowH);
            }
        }
    }

    // Paints a theme on a window and all of its controls. A control chooses its color with its Tag:
    // "bar", "subbar", "line", "panel", "muted", "ok", "warn", "bad", "chip", "doc" and "consolebg".
    static class Skin
    {
        [DllImport("dwmapi.dll")] static extern int DwmSetWindowAttribute(IntPtr hwnd, int attr, ref int value, int size);
        [DllImport("uxtheme.dll", CharSet = CharSet.Unicode)] static extern int SetWindowTheme(IntPtr hwnd, string app, string idlist);
        static readonly HashSet<Control> scrollHooked = new HashSet<Control>();

        public static void Window(Form form, Theme t)
        {
            UI.T = t; form.BackColor = t.Win;
            if (form.IsHandleCreated)
            {
                int dark = t == Theme.Dark ? 1 : 0;
                if (DwmSetWindowAttribute(form.Handle, 20, ref dark, 4) != 0) DwmSetWindowAttribute(form.Handle, 19, ref dark, 4);
            }
            Apply(form);
        }

        public static void Apply(Control c)
        {
            string tag = c.Tag as string;
            if (tag == "bar") { c.BackColor = UI.T.Bar; foreach (Control k in c.Controls) k.Invalidate(); return; }
            if (tag == "subbar") { c.BackColor = UI.T.SubBar; foreach (Control k in c.Controls) k.Invalidate(); return; }
            if (tag == "line") { c.BackColor = UI.T.Line; return; }
            if (c is TextField || c is SearchBox) { c.BackColor = UI.T.Win; }
            else if (c is TextBox) { c.BackColor = c.Enabled ? UI.T.Panel : UI.T.Chip; c.ForeColor = tag == "hint" && c.Text == "Search…" ? UI.T.Muted : UI.T.Fg; }
            else if (c is RichTextBox && tag == "doc") { c.BackColor = UI.T.Panel; c.ForeColor = UI.T.Fg; }
            else if (c is RichTextBox) { c.BackColor = UI.T.Console; c.ForeColor = UI.T.ConsoleFg; }
            else if (c is DataGridView) Grid((DataGridView)c);
            else if (c is Label && tag == "chip") { c.BackColor = UI.T.Chip; c.ForeColor = UI.T.Fg; }
            else if (c is Label) { c.BackColor = Color.Transparent; c.ForeColor = tag == "muted" ? UI.T.Muted : tag == "ok" ? UI.T.Ok : tag == "warn" ? UI.T.Warn : tag == "bad" ? UI.T.Bad : UI.T.Fg; }
            else if (c is Panel && tag == "consolebg") c.BackColor = UI.T.Console;
            else if (c is Surface) c.BackColor = UI.T.Panel;
            else if (c is TabControl || c is TabPage) c.BackColor = UI.T.Win;
            else if (c is FlowLayoutPanel || c is Panel || c is TableLayoutPanel) c.BackColor = tag == "panel" ? UI.T.Panel : UI.T.Win;
            Scroll(c);
            foreach (Control k in c.Controls) Apply(k);
            c.Invalidate();
        }

        // Thin modern scrollbars of Windows 10, in dark mode too.
        static void Scroll(Control c)
        {
            bool scrolls = c is RichTextBox || c is DataGridView || (c is Panel && ((Panel)c).AutoScroll);
            if (!scrolls) return;
            if (scrollHooked.Add(c)) c.HandleCreated += delegate { SetWindowTheme(c.Handle, UI.T == Theme.Dark ? "DarkMode_Explorer" : "Explorer", null); };
            if (c.IsHandleCreated) SetWindowTheme(c.Handle, UI.T == Theme.Dark ? "DarkMode_Explorer" : "Explorer", null);
        }

        public static void Grid(DataGridView g)
        {
            g.BackgroundColor = UI.T.Panel; g.GridColor = UI.T.Line; g.ForeColor = UI.T.Fg;
            g.DefaultCellStyle.BackColor = UI.T.Panel; g.DefaultCellStyle.ForeColor = UI.T.Fg;
            g.DefaultCellStyle.SelectionBackColor = UI.T.Chip; g.DefaultCellStyle.SelectionForeColor = UI.T.Fg;
            g.ColumnHeadersDefaultCellStyle.BackColor = UI.T.Panel; g.ColumnHeadersDefaultCellStyle.ForeColor = UI.T.Muted;
            g.ColumnHeadersDefaultCellStyle.SelectionBackColor = UI.T.Panel;
        }
    }
}
