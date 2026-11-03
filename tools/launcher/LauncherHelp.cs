// The Help page: short topics, drawn in the launcher. The text uses a small part of Markdown:
// "# " and "## " headings, "- " list items, **bold**, `code` and fenced code blocks.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Windows.Forms;
using SubVulpes.Ui;

namespace SubVulpes
{
    partial class LauncherForm
    {
        RichTextBox helpDoc;
        int helpIndex;
        readonly List<NavItem> helpNav = new List<NavItem>();

        // The topics come from tools/launcher/help.md, built into the program. Each "# " line starts a topic.
        static readonly string[][] HelpDocs = LoadHelp();

        static string[][] LoadHelp()
        {
            var topics = new List<string[]>();
            string title = null;
            var body = new System.Text.StringBuilder();
            bool code = false;
            using (var stream = typeof(LauncherForm).Assembly.GetManifestResourceStream("SubVulpes.Help")) {
                if (stream == null) return new[] { new[] { "Help", "# Help\nThe help text is missing from this launcher." } };
                using (var reader = new System.IO.StreamReader(stream, System.Text.Encoding.UTF8))
                    for (string line = reader.ReadLine(); line != null; line = reader.ReadLine()) {
                        if (line.StartsWith("```")) code = !code;
                        if (!code && line.StartsWith("# ")) {
                            if (title != null) topics.Add(new[] { title, body.ToString() });
                            title = line.Substring(2).Trim();
                            body.Length = 0;
                        }
                        body.Append(line).Append('\n');
                    }
            }
            if (title != null) topics.Add(new[] { title, body.ToString() });
            return topics.ToArray();
        }

        Control HelpPage()
        {
            var page = new Panel { Tag = "panel" };
            var nav = new Panel { Dock = DockStyle.Left, Width = 240, Padding = new Padding(12), Tag = "panel" };
            var docHolder = new Panel { Dock = DockStyle.Fill, Padding = new Padding(36, 24, 36, 16), Tag = "panel" };
            helpDoc = new RichTextBox { Dock = DockStyle.Fill, ReadOnly = true, BorderStyle = BorderStyle.None, Tag = "doc", Cursor = Cursors.Default, DetectUrls = false };
            docHolder.Controls.Add(helpDoc);
            var search = new SearchBox(214) { Dock = DockStyle.Top };
            var navList = new Panel { Dock = DockStyle.Fill, Tag = "panel" };
            for (int i = HelpDocs.Length - 1; i >= 0; i--) {
                int index = i;
                var item = new NavItem(HelpDocs[i][0]);
                helpNav.Insert(0, item);
                item.Click += delegate { helpIndex = index; RenderHelp(); };
                navList.Controls.Add(item);
            }
            search.TextChanged += delegate
            {
                string term = search.Text.Trim().ToLowerInvariant();
                for (int i = 0; i < helpNav.Count; i++)
                    helpNav[i].Visible = term.Length == 0 || HelpDocs[i][0].ToLowerInvariant().Contains(term) || HelpDocs[i][1].ToLowerInvariant().Contains(term);
            };
            var gap = new Panel { Dock = DockStyle.Top, Height = 10, Tag = "panel" };
            nav.Controls.Add(navList);
            nav.Controls.Add(gap);
            nav.Controls.Add(search);
            var divider = new Panel { Dock = DockStyle.Left, Width = 1, Tag = "line" };
            page.Controls.Add(docHolder);
            page.Controls.Add(divider);
            page.Controls.Add(nav);
            docHolder.BringToFront();
            RenderHelp();
            return page;
        }

        void RenderHelp()
        {
            if (helpDoc == null) return;
            for (int i = 0; i < helpNav.Count; i++) { helpNav[i].Selected = i == helpIndex; helpNav[i].Invalidate(); }
            var box = helpDoc;
            box.Clear();
            bool code = false;
            foreach (string raw in HelpDocs[helpIndex][1].Split('\n')) {
                string line = raw.TrimEnd();
                if (line.StartsWith("```")) { code = !code; continue; }
                if (line.Length == 0) continue;
                if (code) { HelpPut(box, "    " + line + "\n", "Consolas", 10f, FontStyle.Regular, UI.T.Fg, UI.T.Chip, 0); continue; }
                if (line.StartsWith("# ")) HelpPut(box, line.Substring(2) + "\n", "Segoe UI", 20f, FontStyle.Bold, UI.T.Fg, Color.Empty, 8);
                else if (line.StartsWith("## ")) {
                    HelpPut(box, "\n", "Segoe UI", 6f, FontStyle.Regular, UI.T.Fg, Color.Empty, 0);
                    HelpPut(box, line.Substring(3) + "\n", "Segoe UI", 13f, FontStyle.Bold, UI.T.Fg, Color.Empty, 4);
                }
                else if (line.StartsWith("- ")) HelpInline(box, "•  " + line.Substring(2) + "\n", 6, 16);
                else HelpInline(box, line + "\n", 6, 0);
            }
            box.SelectionStart = 0;
            box.SelectionLength = 0;
        }

        static void HelpPut(RichTextBox box, string text, string font, float size, FontStyle style, Color fg, Color bg, int after)
        {
            box.SelectionStart = box.TextLength;
            box.SelectionLength = 0;
            box.SelectionFont = new Font(font, size, style, GraphicsUnit.Point);
            box.SelectionColor = fg;
            box.SelectionBackColor = bg.IsEmpty ? box.BackColor : bg;
            box.SelectionIndent = 0;
            box.SelectionHangingIndent = 0;
            box.AppendText(text);
            if (after > 0) {
                box.SelectionStart = box.TextLength;
                box.SelectionFont = new Font(font, after, FontStyle.Regular, GraphicsUnit.Point);
                box.AppendText("\n");
            }
        }

        // Writes text with **bold** and `code` marks.
        static void HelpInline(RichTextBox box, string text, int after, int indent)
        {
            bool bold = false, code = false;
            var part = new System.Text.StringBuilder();
            Action flush = delegate
            {
                if (part.Length == 0) return;
                box.SelectionStart = box.TextLength;
                box.SelectionLength = 0;
                box.SelectionIndent = indent;
                box.SelectionHangingIndent = indent > 0 ? 14 : 0;
                box.SelectionFont = code ? new Font("Consolas", 9.5f, FontStyle.Regular) : UI.F(10.5f, bold ? FontStyle.Bold : FontStyle.Regular);
                box.SelectionColor = UI.T.Fg;
                box.SelectionBackColor = code ? UI.T.Chip : box.BackColor;
                box.AppendText(part.ToString());
                part.Length = 0;
            };
            for (int i = 0; i < text.Length; i++) {
                if (text[i] == '*' && i + 1 < text.Length && text[i + 1] == '*') { flush(); bold = !bold; i++; }
                else if (text[i] == '`') { flush(); code = !code; }
                else part.Append(text[i]);
            }
            flush();
        }
    }
}
