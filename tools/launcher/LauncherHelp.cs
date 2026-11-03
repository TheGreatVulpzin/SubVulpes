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

        static readonly string[][] HelpDocs = {
            new[] { "Getting Started", "# Getting Started\nThis launcher starts and watches your SubVulpes server.\n## First start\n- Put the launcher in the server folder, next to `subvulpes_server.exe`.\n- Open **Settings**. Set a name and an admin password.\n- Go to **Server** and press **Start**.\n## Join the game\nPlayers join with the address of your computer and the port in **Settings**. Forward the UDP port on your router.\n## Admin password\nIn the game, type `/admin` and the password in the chat. Keep the password private.\n" },
            new[] { "The Server Page", "# The Server Page\nThe top row shows the state of the server and the buttons.\n- **Start** starts the server.\n- **Stop** asks the server to save and close.\n- **Restart** stops the server and starts it again.\n- **Kill** ends the server at once. Use it only if Stop does not work.\n## Tick rate\nThe server runs about 60 ticks per second. A much lower number means lag.\n## Customize the panel\nSelect the sliders icon to choose which values and graph the panel shows. Drag the line below the panel to change its height.\n## Console\nThe filter shows only Info, Warnings or Errors. The search box shows only the lines with your text. Type a server command in the box at the bottom and press **Enter**.\n" },
            new[] { "Settings", "# Settings\n## Server Settings\nSelect **Save** to write the changes to `config.txt`. **Revert changes** brings back the saved values. **Reset to defaults** sets the numbers back to their default values.\n## Requires restart\nThe launcher saves the setting. The server uses it after the next start.\n## Launcher Settings\nThese settings apply at once. Choose the theme and the accent color here. Each run of the server in Debug Mode saves its own log. The **Past Logs** list shows them.\n## Advanced Settings\nThis page shows when **Debug Mode** is on. Use it to record more data about a run.\n" },
            new[] { "Plugins", "# Plugins\nPlugins add features to the server.\n## Risk\n- **low**: reads data or keeps its own files.\n- **high**: changes the game.\n- **highest**: runs inside the server with full access to it.\nTurn on only plugins that you trust.\n## Install\nUse `Install from File…` for a `.svplugin` file. The launcher shows what the plugin can do before it installs.\n## Approval\nA plugin needs your approval before it runs. **Review approval…** shows the review again. Stop the server first.\n## Settings of a plugin\nSelect a plugin, open the **Settings** tab and select **Save**. A setting that says restart needed applies at the next start.\n" },
            new[] { "Logs", "# Logs\n## The console log\nThe file `logs\\server.log` has all lines of the last run. Use the save icon in the console to copy it with the password removed.\n## Past Logs\nIn Debug Mode, each run makes its own folder in `logs`. Open **Launcher Settings** to see the list. Write a note for a log, then pack it into one zip to send it.\n## Crash reports\nThe server can save a dump when it crashes or freezes. Turn it on in **Advanced Settings**. Send the dump with the log.\n" },
            new[] { "Problems", "# Problems\n## The server does not start\nRead the last lines in the console. Make sure that no other server uses the same port. Check that the folder has `data`.\n## The server freezes\nThe launcher shows Frozen when no speed line comes for a long time. Use **Kill** and start again.\n## The launcher cannot update\nCheck your internet connection. You can also install an update from a `.zip` file in **Launcher Settings**.\n" } };

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
