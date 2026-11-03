// The Plugins page: the table of plugins, the detail panel with its three tabs, and the catalog.
using System;
using System.Collections.Generic;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using SubVulpes.Ui;

namespace SubVulpes
{
    partial class LauncherForm
    {
        readonly List<PluginMetadata> plugins = new List<PluginMetadata>();
        DataGridView pluginGrid;
        Panel pluginDetail;
        SearchBox pluginSearch;
        Switch nativeBypass;
        bool rebuilding;
        int pluginTab, sortColumn = -1;
        bool sortAscending = true;
        string selectedPluginId;
        readonly List<Control> pluginSubPages = new List<Control>();
        readonly List<TabLabel> pluginSubTabs = new List<TabLabel>();

        static readonly Dictionary<string, string> CapText = new Dictionary<string, string> {
            { "read", "Read game data" }, { "write", "Change the game" }, { "storage", "Save its own files" }, { "props", "Change props and objects" },
            { "config", "Read and change server settings" }, { "plugins", "Control other plugins" }, { "ai", "Control the AI characters" },
            { "economy", "Change money" }, { "mission", "Control missions" }, { "settings", "Change the game rules" }, { "engine", "Change engine behavior" },
            { "shop", "Change the shops" }, { "network", "Send and receive network data" } };

        // ------------------------------------------------------------ data

        // The risk follows the rules of the review: native code is the highest, a plugin that only reads or keeps its own data is low.
        static string RiskOf(PluginMetadata m)
        {
            if (m.Runtime == "native") return "highest";
            foreach (string c in m.Capabilities.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries))
                if (c != "read" && c != "storage" && c != "config") return "high";
            return "low";
        }

        // A plugin whose id starts with "sv." comes from the SubVulpes project.
        static string SourceOf(PluginMetadata m)
        {
            return m.Id.StartsWith("sv.", StringComparison.Ordinal) ? "Official" : "Installed";
        }

        // The author comes from the manifest. Without one, the name stays empty.
        static string AuthorOf(PluginMetadata m)
        {
            return m.Author;
        }

        static string ByLine(string author, string version)
        {
            string by = author.Length > 0 ? "by " + author : "";
            string v = version.Length > 0 ? "v" + version : "";
            return by.Length > 0 && v.Length > 0 ? by + " · " + v : by + v;
        }

        string SourceOfPlugin(PluginMetadata m)
        {
            // The catalog knows best. Without it, the id decides.
            var entry = catalog.FirstOrDefault(c => c.Id == m.Id);
            return entry != null && entry.Source.Length > 0 ? entry.Source : SourceOf(m);
        }

        static string StateText(string state)
        {
            switch (state) {
            case "active": return "Active";
            case "disabled": return "Disabled";
            case "failed": return "Failed";
            case "restart": return "Restart needed";
            case "ready": return "Ready";
            case "approval": return "Needs approval";
            default: return state ?? "";
            }
        }

        static Color StateColor(string state)
        {
            return state == "active" || state == "ready" ? UI.T.Ok : state == "restart" || state == "approval" ? UI.T.Warn : state == "failed" ? UI.T.Bad : UI.T.Muted;
        }

        static Color RiskColor(string risk)
        {
            return risk == "low" ? UI.T.Ok : risk == "high" ? UI.T.Warn : UI.T.Bad;
        }

        PluginMetadata FindPlugin(string id)
        {
            return plugins.FirstOrDefault(p => p.Id == id);
        }

        void ReadPluginLine(string line)
        {
            PluginMetadata metadata = ParsePluginRow(line, false);
            if (metadata == null) return;
            int at = plugins.FindIndex(p => p.Id == metadata.Id);
            if (at < 0) plugins.Add(metadata);
            else plugins[at] = metadata;
            RebuildPlugins();
        }

        void ClearPlugins()
        {
            plugins.Clear();
            RebuildPlugins();
        }

        string SelectedPlugin()
        {
            if (selectedPluginId == null || FindPlugin(selectedPluginId) == null) {
                message.Text = "Select a plugin in the list first.";
                return null;
            }
            return selectedPluginId;
        }

        string SortKey(PluginMetadata p, int column)
        {
            return column == 0 ? p.DisplayName : column == 1 ? p.Version : column == 2 ? StateText(p.State) : column == 4 ? SourceOfPlugin(p) : column == 5 ? AuthorOf(p) : RiskOf(p) == "low" ? "1" : RiskOf(p) == "high" ? "2" : "3";
        }

        void RebuildPlugins()
        {
            if (pluginGrid == null) return;
            rebuilding = true;
            var shown = new List<PluginMetadata>();
            string term = pluginSearch == null ? "" : pluginSearch.Text.Trim();
            foreach (var p in plugins)
                if (term.Length == 0 || p.DisplayName.IndexOf(term, StringComparison.OrdinalIgnoreCase) >= 0 || p.Id.IndexOf(term, StringComparison.OrdinalIgnoreCase) >= 0) shown.Add(p);
            if (sortColumn >= 0 && sortColumn <= 5)
                shown.Sort((a, b) => (sortAscending ? 1 : -1) * string.Compare(SortKey(a, sortColumn), SortKey(b, sortColumn), StringComparison.OrdinalIgnoreCase));
            pluginGrid.Rows.Clear();
            foreach (var p in shown) {
                int r = pluginGrid.Rows.Add(p.DisplayName, p.Version, StateText(p.State), RiskOf(p), SourceOfPlugin(p), AuthorOf(p), "");
                pluginGrid.Rows[r].Tag = p;
                if (p.Id == selectedPluginId) pluginGrid.CurrentCell = pluginGrid.Rows[r].Cells[0];
            }
            foreach (DataGridViewColumn col in pluginGrid.Columns)
                col.HeaderCell.SortGlyphDirection = col.Index == sortColumn ? (sortAscending ? SortOrder.Ascending : SortOrder.Descending) : SortOrder.None;
            rebuilding = false;
            if (selectedPluginId == null || FindPlugin(selectedPluginId) == null) {
                var first = shown.FirstOrDefault();
                selectedPluginId = first == null ? null : first.Id;
                if (first != null) pluginGrid.CurrentCell = pluginGrid.Rows[0].Cells[0];
            }
            RenderPluginDetail();
        }

        // ------------------------------------------------------------ the page

        Control PluginsHost()
        {
            var host = new TableLayoutPanel { ColumnCount = 1, RowCount = 2, Dock = DockStyle.Fill };
            host.ColumnStyles.Add(new ColumnStyle(SizeType.Percent, 100));
            host.RowStyles.Add(new RowStyle(SizeType.Absolute, 40));
            host.RowStyles.Add(new RowStyle(SizeType.Percent, 100));
            var strip = new Panel { Dock = DockStyle.Fill, Padding = new Padding(8, 0, 8, 0), Tag = "subbar", Margin = new Padding(0) };
            var area = new Panel { Dock = DockStyle.Fill, Padding = new Padding(16, 8, 16, 10) };
            string[] names = { "My Plugins", "Catalog" };
            Control[] subs = { PluginsPage(), CatalogPage() };
            for (int i = names.Length - 1; i >= 0; i--) {
                var tab = new TabLabel(names[i]) { Selected = i == 0, Sub = true };
                int index = i;
                tab.Click += delegate { ShowPluginSub(index); };
                pluginSubTabs.Insert(0, tab);
                strip.Controls.Add(tab);
                subs[i].Dock = DockStyle.Fill;
                subs[i].Visible = i == 0;
                area.Controls.Add(subs[i]);
                pluginSubPages.Insert(0, subs[i]);
            }
            host.Controls.Add(strip, 0, 0);
            host.Controls.Add(area, 0, 1);
            return host;
        }

        void ShowPluginSub(int k)
        {
            if (k == 1) { LoadCatalog(); RenderCatalog(); }
            for (int n = 0; n < pluginSubPages.Count; n++) {
                pluginSubPages[n].Visible = n == k;
                pluginSubTabs[n].Selected = n == k;
                pluginSubTabs[n].Invalidate();
            }
        }

        // ------------------------------------------------------------ catalog

        // One plugin of catalog.json. Icon and Images are picture files, File is the package. A path is in the server folder, or absolute.
        sealed class CatalogEntry
        {
            public string Id = "", Name = "", Description = "", Author = "", Version = "", Source = "", File = "", Icon = "";
            public readonly List<string> Images = new List<string>();
        }

        readonly List<CatalogEntry> catalog = new List<CatalogEntry>();
        readonly Dictionary<string, Image> catalogPictures = new Dictionary<string, Image>();
        Panel catalogList, catalogDetail, catalogBar;
        SearchBox catalogSearch;
        Dropdown catalogKind;
        string catalogProblem = "";

        static string CatalogText(Dictionary<string, object> d, string key, int maximum)
        {
            object v;
            if (!d.TryGetValue(key, out v) || !(v is string)) return "";
            string text = ((string)v).Trim();
            foreach (char c in text) if (c < ' ') return "";
            return text.Length > maximum ? text.Substring(0, maximum) : text;
        }

        // Reads catalog.json from the server folder: a list of objects with id, name, description, author, version, source, file, icon and images.
        void LoadCatalog()
        {
            catalog.Clear();
            foreach (Image picture in catalogPictures.Values) if (picture != null) picture.Dispose();
            catalogPictures.Clear();
            catalogProblem = "";
            string path = Path.Combine(folder, "catalog.json");
            if (!System.IO.File.Exists(path)) return;
            try {
                var items = new JavaScriptSerializer().DeserializeObject(System.IO.File.ReadAllText(path, Encoding.UTF8)) as object[];
                if (items == null) throw new IOException("The file must hold a list.");
                foreach (object item in items) {
                    var d = item as Dictionary<string, object>;
                    if (d == null) continue;
                    var entry = new CatalogEntry { Id = CatalogText(d, "id", 47) };
                    if (!PluginId(entry.Id)) continue;
                    entry.Name = CatalogText(d, "name", 48);
                    if (entry.Name.Length == 0) entry.Name = entry.Id;
                    entry.Description = CatalogText(d, "description", 600);
                    entry.Author = CatalogText(d, "author", 48);
                    entry.Version = CatalogText(d, "version", 23);
                    entry.Source = CatalogText(d, "source", 16);
                    entry.File = CatalogText(d, "file", 260);
                    entry.Icon = CatalogText(d, "icon", 260);
                    object images;
                    if (d.TryGetValue("images", out images) && images is object[])
                        foreach (object image in (object[])images)
                            if (image is string && entry.Images.Count < 8) entry.Images.Add((string)image);
                    catalog.Add(entry);
                }
            } catch (Exception ex) {
                catalog.Clear();
                catalogProblem = "Cannot read catalog.json: " + ex.Message;
            }
        }

        string CatalogPath(string given)
        {
            return Path.IsPathRooted(given) ? given : Path.Combine(folder, given);
        }

        string CatalogPackage(CatalogEntry entry)
        {
            if (entry.File.Length == 0) return null;
            string full = CatalogPath(entry.File);
            return System.IO.File.Exists(full) ? full : null;
        }

        // A picture of the catalog. A file that is too big or not a PNG or JPEG gives nothing.
        Image CatalogPicture(string given)
        {
            if (string.IsNullOrEmpty(given)) return null;
            string full = CatalogPath(given);
            Image picture;
            if (catalogPictures.TryGetValue(full, out picture)) return picture;
            try {
                var info = new FileInfo(full);
                string extension = info.Extension.ToLowerInvariant();
                if (info.Exists && info.Length <= 5 * 1024 * 1024 && (extension == ".png" || extension == ".jpg" || extension == ".jpeg"))
                    using (var stream = System.IO.File.OpenRead(full))
                    using (var raw = Image.FromStream(stream))
                        picture = new Bitmap(raw);
            } catch (Exception) { picture = null; }
            catalogPictures[full] = picture;
            return picture;
        }

        Control CatalogPage()
        {
            var page = new Panel();
            catalogBar = new Panel { Dock = DockStyle.Top, Height = 44 };
            catalogSearch = new SearchBox(260) { Location = new Point(0, 8) };
            catalogKind = new Dropdown(150) { Location = new Point(272, 8) };
            catalogKind.Items.AddRange(new object[] { "All plugins", "Official", "Community" });
            catalogKind.SelectedIndex = 0;
            catalogSearch.TextChanged += delegate { RenderCatalog(); };
            catalogKind.SelectedIndexChanged += delegate { RenderCatalog(); };
            catalogBar.Controls.Add(catalogSearch);
            catalogBar.Controls.Add(catalogKind);
            catalogList = new Panel { Dock = DockStyle.Fill, AutoScroll = true };
            catalogDetail = new Panel { Dock = DockStyle.Fill, AutoScroll = true, Visible = false };
            page.Controls.Add(catalogList);
            page.Controls.Add(catalogDetail);
            page.Controls.Add(catalogBar);
            catalogBar.SendToBack();
            catalogList.BringToFront();
            return page;
        }

        void RenderCatalog()
        {
            if (catalogList == null) return;
            // The list shows again. A plugin page closes.
            catalogDetail.Visible = false;
            catalogList.Visible = true;
            catalogBar.Visible = true;
            catalogList.SuspendLayout();
            catalogList.Controls.Clear();
            string term = catalogSearch.Text.Trim();
            string kind = catalogKind.SelectedIndex == 1 ? "Official" : catalogKind.SelectedIndex == 2 ? "Community" : null;
            var shown = catalog.Where(c => (kind == null || c.Source == kind) &&
                (term.Length == 0 || c.Name.IndexOf(term, StringComparison.OrdinalIgnoreCase) >= 0 || c.Description.IndexOf(term, StringComparison.OrdinalIgnoreCase) >= 0)).ToList();
            if (shown.Count == 0) catalogList.Controls.Add(EmptyCatalog());
            // Each card docks at the top, so the first plugin is added last.
            for (int i = shown.Count - 1; i >= 0; i--) catalogList.Controls.Add(CatalogCard(shown[i]));
            Skin.Apply(catalogList);
            catalogList.ResumeLayout();
        }

        Control EmptyCatalog()
        {
            var card = new Surface { Dock = DockStyle.Top, Height = 150 };
            string why = catalogProblem.Length > 0 ? catalogProblem :
                catalog.Count > 0 ? "No plugin matches the search." :
                "Put a file named catalog.json in the server folder to list plugins here. Each plugin has an id, a name, a description, a version, a source and a package file. Or install a plugin from a file.";
            var title = L(catalog.Count > 0 ? "Nothing to show" : "The catalog is empty", true, false);
            title.Font = UI.F(11f, FontStyle.Bold);
            title.Location = new Point(24, 22);
            var text = L(why, false, true);
            text.MaximumSize = new Size(620, 0);
            text.Location = new Point(24, 54);
            var install = new FlatButton("Install from File…") { Glyph = "download", Location = new Point(24, 100) };
            install.Click += async delegate { await InstallPlugin(); };
            card.Controls.AddRange(new Control[] { title, text, install });
            return card;
        }

        // The Install button, or the mark of a plugin that is installed already.
        Control CatalogAction(CatalogEntry c)
        {
            if (plugins.Any(p => p.Id == c.Id)) {
                var done = L("✓ Installed", false, false);
                done.Tag = "ok";
                return done;
            }
            string package = CatalogPackage(c);
            var install = new FlatButton("Install") { Glyph = "download", Enabled = package != null };
            if (package == null) UI.Tips.SetToolTip(install, "The package file of this plugin is not on this computer.");
            install.Click += async delegate { await InstallPlugin(package); };
            return install;
        }

        Control CatalogCard(CatalogEntry c)
        {
            var wrap = new Panel { Dock = DockStyle.Top, Height = 92, Padding = new Padding(0, 0, 0, 10) };
            var card = new Surface { Dock = DockStyle.Fill, Cursor = Cursors.Hand };
            var avatar = new Avatar(c.Name) { Location = new Point(16, 16), Picture = CatalogPicture(c.Icon) };
            var name = L(c.Name, true, false);
            name.Font = UI.F(10.5f, FontStyle.Bold);
            name.Location = new Point(76, 14);
            var desc = L(c.Description.Length > 0 ? c.Description : "No description.", false, true);
            desc.AutoSize = false;
            desc.AutoEllipsis = true;
            desc.Size = new Size(420, 20);
            desc.Location = new Point(76, 42);
            var meta = L(ByLine(c.Author, c.Version), false, true);
            meta.Location = new Point(76, 62);
            var parts = new List<Control> { avatar, name, desc, meta };
            if (c.Source.Length > 0) {
                var chip = new Label { Text = c.Source, AutoSize = true, Padding = new Padding(8, 3, 8, 3), Font = UI.F(9f, FontStyle.Regular), Tag = "chip" };
                chip.Location = new Point(76 + name.PreferredWidth + 10, 12);
                parts.Add(chip);
            }
            card.Controls.AddRange(parts.ToArray());
            // A click on the card opens the page of the plugin.
            card.Click += delegate { ShowCatalogDetail(c); };
            foreach (Control part in parts) { part.Cursor = Cursors.Hand; part.Click += delegate { ShowCatalogDetail(c); }; }
            Control action = CatalogAction(c);
            card.Controls.Add(action);
            Action place = delegate { action.Location = new Point(card.Width - action.Width - 18, (card.Height - action.Height) / 2); };
            card.Resize += delegate { place(); };
            action.SizeChanged += delegate { place(); };
            wrap.Controls.Add(card);
            return wrap;
        }

        // The page of one plugin: icon, author, description, a large picture and a row of small ones, like a mod site.
        void ShowCatalogDetail(CatalogEntry c)
        {
            catalogBar.Visible = false;
            catalogList.Visible = false;
            catalogDetail.SuspendLayout();
            catalogDetail.Controls.Clear();
            var back = new FlatButton("Back") { Location = new Point(0, 4), Height = 28 };
            back.Click += delegate { RenderCatalog(); };
            var avatar = new Avatar(c.Name, 72) { Location = new Point(0, 48), Picture = CatalogPicture(c.Icon) };
            var name = L(c.Name, true, false);
            name.Font = UI.F(16f, FontStyle.Bold);
            name.Location = new Point(88, 50);
            var meta = L(ByLine(c.Author, c.Version), false, true);
            meta.Location = new Point(88, 86);
            var parts = new List<Control> { back, avatar, name, meta };
            if (c.Source.Length > 0) {
                var chip = new Label { Text = c.Source, AutoSize = true, Padding = new Padding(8, 3, 8, 3), Font = UI.F(9f, FontStyle.Regular), Tag = "chip" };
                chip.Location = new Point(88 + name.PreferredWidth + 12, 56);
                parts.Add(chip);
            }
            var desc = L(c.Description.Length > 0 ? c.Description : "No description.", false, c.Description.Length == 0);
            desc.MaximumSize = new Size(720, 0);
            desc.Location = new Point(0, 140);
            parts.Add(desc);
            int y = 140 + desc.PreferredSize.Height + 20;
            var pictures = new List<Image>();
            foreach (string given in c.Images) { Image picture = CatalogPicture(given); if (picture != null) pictures.Add(picture); }
            if (pictures.Count == 0) {
                var none = L("This plugin has no pictures.", false, true);
                none.Location = new Point(0, y);
                parts.Add(none);
            } else {
                var big = new ImageBox { Location = new Point(0, y), Size = new Size(640, 360), Picture = pictures[0] };
                parts.Add(big);
                var thumbs = new List<Thumb>();
                for (int i = 0; i < pictures.Count; i++) {
                    var thumb = new Thumb { Picture = pictures[i], Selected = i == 0, Location = new Point(i * 120, y + 372) };
                    thumb.Click += delegate
                    {
                        big.Picture = thumb.Picture;
                        foreach (Thumb other in thumbs) { other.Selected = other == thumb; other.Invalidate(); }
                        big.Invalidate();
                    };
                    thumbs.Add(thumb);
                    parts.Add(thumb);
                }
            }
            catalogDetail.Controls.AddRange(parts.ToArray());
            Control action = CatalogAction(c);
            catalogDetail.Controls.Add(action);
            Action place = delegate { action.Location = new Point(Math.Max(400, catalogDetail.ClientSize.Width - action.Width - 8), 62); };
            catalogDetail.Resize += delegate { place(); };
            action.SizeChanged += delegate { place(); };
            place();
            Skin.Apply(catalogDetail);
            catalogDetail.Visible = true;
            catalogDetail.ResumeLayout();
        }

        Control PluginsPage()
        {
            var page = new Panel();
            var bar = new Panel { Dock = DockStyle.Top, Height = 44 };
            var buttons = new FlowLayoutPanel { Dock = DockStyle.Right, AutoSize = true, WrapContents = false, FlowDirection = FlowDirection.RightToLeft, Padding = new Padding(0, 4, 0, 0) };
            var refresh = new FlatButton("Refresh") { Margin = new Padding(0), Glyph = "refresh" };
            var install = new FlatButton("Install from File…") { Glyph = "download" };
            refresh.Click += async delegate { await RefreshPlugins(); };
            install.Click += async delegate { await InstallPlugin(); };
            buttons.Controls.Add(refresh);
            buttons.Controls.Add(install);
            pluginSearch = new SearchBox(260) { Location = new Point(0, 9) };
            pluginSearch.TextChanged += delegate { RebuildPlugins(); };
            bar.Controls.Add(buttons);
            bar.Controls.Add(pluginSearch);

            pluginDetail = new Panel { Dock = DockStyle.Right, Width = 380, Padding = new Padding(16), Tag = "panel", AutoScroll = true };
            var gap = new Panel { Dock = DockStyle.Right, Width = 12 };
            var danger = new Panel { Dock = DockStyle.Bottom, Height = 50 };
            nativeBypass = new Switch { Location = new Point(0, 14) };
            nativeBypass.Changed += delegate
            {
                if (nativeBypass.On && MessageBox.Show(this,
                    "Danger!!! This will skip the content approval process. A DLL plugin has full access to the server and your system. Only use this for plugins that you trust completely.\r\n\r\n" +
                    "Are you sure you want to allow unapproved DLL plugins for this launcher session?",
                    "NATIVE PLUGIN BYPASS", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) != DialogResult.Yes)
                    nativeBypass.On = false;
            };
            var dangerText = L("Allow unapproved DLLs temporarily", false, false);
            dangerText.Tag = "warn";
            dangerText.Location = new Point(50, 16);
            UI.Tips.SetToolTip(nativeBypass, "Skips the content approval for this launcher session. Use it only for plugins that you trust.");
            danger.Controls.Add(nativeBypass);
            danger.Controls.Add(dangerText);

            pluginGrid = Table();
            pluginGrid.Columns.Add("n", "Name");
            pluginGrid.Columns.Add("v", "Version");
            pluginGrid.Columns.Add("s", "State");
            pluginGrid.Columns.Add("r", "Risk");
            pluginGrid.Columns.Add("src", "Source");
            pluginGrid.Columns.Add("au", "Author");
            pluginGrid.Columns.Add("o", "On");
            pluginGrid.Columns[0].FillWeight = 170;
            pluginGrid.Columns[1].FillWeight = 70;
            pluginGrid.Columns[2].FillWeight = 120;
            pluginGrid.Columns[3].FillWeight = 80;
            pluginGrid.Columns[4].FillWeight = 80;
            pluginGrid.Columns[5].FillWeight = 80;
            pluginGrid.Columns[6].FillWeight = 50;
            foreach (DataGridViewColumn col in pluginGrid.Columns) col.SortMode = DataGridViewColumnSortMode.Programmatic;
            // The columns the user hid stay hidden. The setting holds the names of the hidden columns.
            string hidden = "," + settings.Get("plugin_hidden", "3,4,5") + ",";
            for (int i = 1; i <= 5; i++) pluginGrid.Columns[i].Visible = !hidden.Contains("," + i + ",");
            pluginGrid.ColumnHeaderMouseClick += delegate (object s, DataGridViewCellMouseEventArgs e)
            {
                if (e.Button == MouseButtons.Left)
                {
                    if (e.ColumnIndex > 5) return;
                    sortAscending = sortColumn == e.ColumnIndex ? !sortAscending : true;
                    sortColumn = e.ColumnIndex;
                    RebuildPlugins();
                    return;
                }
                PluginColumnMenu(e.ColumnIndex);
            };
            pluginGrid.CellPainting += PaintPluginCell;
            pluginGrid.CellClick += delegate (object s, DataGridViewCellEventArgs e)
            {
                if (e.RowIndex < 0) return;
                var p = pluginGrid.Rows[e.RowIndex].Tag as PluginMetadata;
                if (p == null) return;
                selectedPluginId = p.Id;
                if (e.ColumnIndex == 6) ToggleFromTable(p);
                else RenderPluginDetail();
            };
            pluginGrid.CellMouseClick += delegate (object s, DataGridViewCellMouseEventArgs e)
            {
                if (e.Button != MouseButtons.Right || e.RowIndex < 0) return;
                var p = pluginGrid.Rows[e.RowIndex].Tag as PluginMetadata;
                if (p == null) return;
                selectedPluginId = p.Id;
                pluginGrid.CurrentCell = pluginGrid.Rows[e.RowIndex].Cells[0];
                RenderPluginDetail();
                PluginRowMenu(p);
            };
            pluginGrid.SelectionChanged += delegate
            {
                if (rebuilding) return;
                var p = pluginGrid.CurrentRow == null ? null : pluginGrid.CurrentRow.Tag as PluginMetadata;
                if (p != null && p.Id != selectedPluginId) { selectedPluginId = p.Id; pluginTab = Math.Min(pluginTab, 2); RenderPluginDetail(); }
            };
            page.Controls.Add(pluginGrid);
            page.Controls.Add(danger);
            page.Controls.Add(gap);
            page.Controls.Add(pluginDetail);
            page.Controls.Add(bar);
            bar.SendToBack();
            pluginGrid.BringToFront();
            return page;
        }

        // Right click on a header: choose the columns and the order, like in Windows Explorer.
        void PluginColumnMenu(int clicked)
        {
            var menu = Dropdown.Menu();
            menu.ShowCheckMargin = true;
            for (int i = 1; i <= 5; i++) {
                var col = pluginGrid.Columns[i];
                var item = new ToolStripMenuItem(col.HeaderText) { Checked = col.Visible, ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) };
                item.Click += delegate
                {
                    col.Visible = !col.Visible;
                    var hiddenNow = new List<string>();
                    for (int k = 1; k <= 5; k++) if (!pluginGrid.Columns[k].Visible) hiddenNow.Add(k.ToString());
                    settings.Set("plugin_hidden", string.Join(",", hiddenNow.ToArray()));
                    settings.Save();
                };
                menu.Items.Add(item);
            }
            if (clicked >= 0 && clicked <= 5) {
                menu.Items.Add(new ToolStripSeparator());
                var up = new ToolStripMenuItem("Sort ascending") { ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) };
                var down = new ToolStripMenuItem("Sort descending") { ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) };
                up.Click += delegate { sortColumn = clicked; sortAscending = true; RebuildPlugins(); };
                down.Click += delegate { sortColumn = clicked; sortAscending = false; RebuildPlugins(); };
                menu.Items.Add(up);
                menu.Items.Add(down);
            }
            menu.Show(Cursor.Position);
        }

        // Right click on a plugin: the same actions as the switch and the Summary tab.
        void PluginRowMenu(PluginMetadata p)
        {
            var menu = Dropdown.Menu();
            string id = p.Id;
            var toggle = new ToolStripMenuItem(p.State == "disabled" ? "Turn on" : "Turn off") { ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) };
            var review = new ToolStripMenuItem("Review approval…") { ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) };
            var copy = new ToolStripMenuItem("Copy ID") { ForeColor = UI.T.Fg, Padding = new Padding(2, 4, 2, 4) };
            toggle.Click += delegate { ToggleFromTable(p); };
            review.Click += async delegate { await ApprovePlugin(id); };
            copy.Click += delegate { Clipboard.SetText(id); message.Text = "Copied: " + id; };
            menu.Items.Add(toggle);
            menu.Items.Add(review);
            menu.Items.Add(new ToolStripSeparator());
            menu.Items.Add(copy);
            menu.Show(Cursor.Position);
        }

        void ToggleFromTable(PluginMetadata p)
        {
            if (p.Required == "1") { message.Text = p.DisplayName + " is required. It cannot be turned off."; return; }
            ChangePlugin(p.Id, p.State == "disabled").ContinueWith(t => { });
        }

        void PaintPluginCell(object s, DataGridViewCellPaintingEventArgs e)
        {
            if (e.RowIndex < 0 || (e.ColumnIndex != 2 && e.ColumnIndex != 3 && e.ColumnIndex != 6)) return;
            var p = pluginGrid.Rows[e.RowIndex].Tag as PluginMetadata;
            if (p == null) return;
            e.Paint(e.ClipBounds, DataGridViewPaintParts.Background | DataGridViewPaintParts.SelectionBackground | DataGridViewPaintParts.Border);
            var g = e.Graphics;
            g.SmoothingMode = SmoothingMode.AntiAlias;
            int my = e.CellBounds.Y + e.CellBounds.Height / 2;
            if (e.ColumnIndex == 2) {
                using (var b = new SolidBrush(StateColor(p.State))) g.FillEllipse(b, e.CellBounds.X + 6, my - 4, 8, 8);
                TextRenderer.DrawText(g, StateText(p.State), UI.F(9.5f, FontStyle.Regular), new Rectangle(e.CellBounds.X + 20, e.CellBounds.Y, e.CellBounds.Width - 20, e.CellBounds.Height),
                    UI.T.Fg, TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding | TextFormatFlags.EndEllipsis);
            } else if (e.ColumnIndex == 3) {
                string risk = RiskOf(p);
                Color rc = RiskColor(risk);
                var rf = UI.F(9f, FontStyle.Regular);
                int rw = TextRenderer.MeasureText(g, risk, rf, new Size(999, 30), TextFormatFlags.NoPadding).Width + 20;
                var pill = new Rectangle(e.CellBounds.X + 6, my - 11, rw, 22);
                using (var path = UI.Round(pill, 11)) using (var b = new SolidBrush(Icons.Blend(UI.T.Panel, rc, 0.16))) g.FillPath(b, path);
                TextRenderer.DrawText(g, risk, rf, pill, rc, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPadding);
            } else if (e.ColumnIndex == 6) {
                Switch.Draw(g, new Rectangle(e.CellBounds.X + 6, e.CellBounds.Y + (e.CellBounds.Height - 20) / 2, 36, 20), p.State != "disabled");
            }
            e.Handled = true;
        }

        // ------------------------------------------------------------ detail panel

        void RenderPluginDetail()
        {
            if (pluginDetail == null) return;
            var p = selectedPluginId == null ? null : FindPlugin(selectedPluginId);
            pluginDetail.SuspendLayout();
            pluginDetail.Controls.Clear();
            if (p == null) {
                var none = L("No plugin selected.", false, true);
                none.Location = new Point(16, 16);
                pluginDetail.Controls.Add(none);
                Skin.Apply(pluginDetail);
                pluginDetail.ResumeLayout();
                return;
            }
            var flow = new FlowLayoutPanel { Dock = DockStyle.Top, AutoSize = true, FlowDirection = FlowDirection.TopDown, WrapContents = false, Width = 340, Padding = new Padding(0, 14, 0, 0), Tag = "panel" };
            var header = new Panel { Dock = DockStyle.Top, Height = 58, Tag = "panel" };
            var title = L(p.DisplayName, true, false);
            title.Font = UI.F(13f, FontStyle.Bold);
            title.Location = new Point(56, 5);
            var sub = L(p.Version + (AuthorOf(p).Length > 0 ? " · " + AuthorOf(p) : ""), false, true);
            sub.Location = new Point(56, 31);
            header.Controls.Add(new Avatar(p.DisplayName) { Location = new Point(0, 6) });
            header.Controls.Add(title);
            header.Controls.Add(sub);
            if (pluginTab == 0) SummaryTab(flow, p);
            else if (pluginTab == 1) InfoTab(flow, p);
            else SettingsTab(flow, p);
            pluginDetail.Controls.Add(flow);
            var strip = new Panel { Dock = DockStyle.Top, Height = 38, Tag = "panel" };
            string[] tabNames = { "Summary", "Info", "Settings" };
            for (int i = tabNames.Length - 1; i >= 0; i--) {
                var tab = new TabLabel(tabNames[i]) { Sub = true, Selected = i == pluginTab };
                int index = i;
                tab.Click += delegate { pluginTab = index; RenderPluginDetail(); };
                strip.Controls.Add(tab);
            }
            pluginDetail.Controls.Add(strip);
            pluginDetail.Controls.Add(header);
            Skin.Apply(pluginDetail);
            pluginDetail.ResumeLayout();
        }

        void SummaryTab(FlowLayoutPanel flow, PluginMetadata p)
        {
            var d = L(p.Description.Length > 0 ? p.Description : "This plugin has no description.", false, p.Description.Length == 0);
            d.MaximumSize = new Size(330, 0);
            d.Margin = new Padding(0, 0, 0, 12);
            flow.Controls.Add(d);
            var state = L("State: " + StateText(p.State) + (p.Reason.Length > 0 ? ". " + p.Reason : ""), false, true);
            state.MaximumSize = new Size(330, 0);
            state.Margin = new Padding(0, 0, 0, 12);
            flow.Controls.Add(state);
            if (p.State == "restart") {
                var restart = new FlatButton("Restart server") { Glyph = "restart", Enabled = server != null && !stopping, Margin = new Padding(0, 0, 0, 12) };
                restart.Click += async delegate { if (server != null && await StopServer()) StartServer(); };
                flow.Controls.Add(restart);
            }
            if (RiskOf(p) == "highest") {
                var n = L("This plugin has full access to the server. It can read private data, freeze it or crash it.", false, false);
                n.MaximumSize = new Size(330, 0);
                n.Tag = "warn";
                n.Margin = new Padding(0, 0, 0, 12);
                flow.Controls.Add(n);
            }
            var acts = new FlowLayoutPanel { AutoSize = true, Tag = "panel", Margin = new Padding(0, 4, 0, 0) };
            var update = new FlatButton("Update…");
            update.Click += async delegate { await InstallPlugin(); };
            var review = new FlatButton("Review approval…") { Margin = new Padding(0) };
            string id = p.Id;
            review.Click += async delegate { await ApprovePlugin(id); };
            acts.Controls.Add(update);
            acts.Controls.Add(review);
            flow.Controls.Add(acts);
        }

        void InfoTab(FlowLayoutPanel flow, PluginMetadata p)
        {
            var tbl = new TableLayoutPanel { AutoSize = true, ColumnCount = 2, Tag = "panel", Margin = new Padding(0, 0, 0, 12) };
            tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 100));
            tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 230));
            string[][] info = {
                new[] { "ID", p.Id }, new[] { "Version", p.Version }, new[] { "Author", AuthorOf(p).Length > 0 ? AuthorOf(p) : "Not stated" },
                new[] { "Runtime", p.Runtime }, new[] { "Risk", RiskOf(p) }, new[] { "Required", p.Required == "1" ? "Yes" : "No" } };
            foreach (var kv in info) {
                int row = tbl.RowCount++;
                tbl.RowStyles.Add(new RowStyle(SizeType.Absolute, 28));
                var k = L(kv[0], false, true);
                k.Anchor = AnchorStyles.Left;
                tbl.Controls.Add(k, 0, row);
                var v = L(kv[1], false, false);
                v.Anchor = AnchorStyles.Left;
                tbl.Controls.Add(v, 1, row);
            }
            flow.Controls.Add(tbl);
            var caps = p.Capabilities.Split(new[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
            var cl = L("This plugin can", true, false);
            cl.Margin = new Padding(0, 4, 0, 6);
            flow.Controls.Add(cl);
            if (caps.Length == 0 && p.Runtime != "native") flow.Controls.Add(L("Nothing beyond its own code.", false, true));
            foreach (string c in caps) {
                string what;
                if (!CapText.TryGetValue(c, out what)) what = c;
                var line = L("•  " + what, false, true);
                line.MaximumSize = new Size(330, 0);
                line.Margin = new Padding(0, 0, 0, 3);
                flow.Controls.Add(line);
            }
            if (p.Runtime == "native") {
                var line = L("•  Run inside the server with full access", false, true);
                line.Margin = new Padding(0, 0, 0, 3);
                flow.Controls.Add(line);
            }
        }

        // ------------------------------------------------------------ plugin settings

        // One setting of a plugin, as the server lists it. The server checks every value again, so this form only helps.
        sealed class PluginSetting
        {
            public string Key, Type, Value, Default, Min, Max, Label, Help;
            public bool Live, Admin, Pending, Reset;
            public Control Editor;
        }

        // The listing of the server: SETTING, id, key, type, value, default, min, max (or choices), live, admin, pending, label, help.
        static List<PluginSetting> ParsePluginSettings(string id, string output)
        {
            var result = new List<PluginSetting>();
            foreach (string line in output.Replace("\r", "").Split('\n')) {
                if (!line.StartsWith("SETTING\t", StringComparison.Ordinal)) continue;
                string[] f = line.Split('\t');
                if (f.Length != 13 || f[1] != id || !Regex.IsMatch(f[2], @"\A[a-z][a-z0-9_]{0,31}\z")) continue;
                if (Array.IndexOf(new[] { "flag", "int", "float", "text", "choice", "secret" }, f[3]) < 0) continue;
                result.Add(new PluginSetting {
                    Key = f[2], Type = f[3], Value = f[4], Default = f[5], Min = f[6], Max = f[7],
                    Live = f[8] == "1", Admin = f[9] == "1", Pending = f[10] == "1", Label = f[11], Help = f[12]
                });
            }
            return result;
        }

        static Control SettingEditor(PluginSetting s)
        {
            int number;
            decimal low, high;
            switch (s.Type) {
            case "flag":
                return new Switch { On = s.Value == "1" };
            case "int":
                if (int.TryParse(s.Min, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out number)) low = number; else low = int.MinValue;
                if (int.TryParse(s.Max, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out number)) high = number; else high = int.MaxValue;
                decimal start;
                if (!decimal.TryParse(s.Value, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out start)) start = low;
                var field = new NumberField(140) { Minimum = low, Maximum = high };
                field.Value = start;
                return field;
            case "choice":
                var box = new Dropdown(180);
                foreach (string name in s.Max.Split(',')) box.Items.Add(name);
                box.SelectedItem = s.Value;
                return box;
            case "secret":
                // A secret shows no value. The field stays empty until the administrator types a new one.
                var secret = new TextField(180, false);
                secret.Box.UseSystemPasswordChar = true;
                secret.Box.MaxLength = 63;
                return secret;
            default:
                int most;
                if (!int.TryParse(s.Max, out most) || most < 1 || most > 63) most = 63;
                var text = new TextField(180, false);
                text.Box.MaxLength = most;
                text.Text = s.Value;
                return text;
            }
        }

        static string SettingEditorValue(PluginSetting s)
        {
            if (s.Type == "flag") return ((Switch)s.Editor).On ? "1" : "0";
            if (s.Type == "int") return ((NumberField)s.Editor).Value.ToString(CultureInfo.InvariantCulture);
            if (s.Type == "choice") return ((Dropdown)s.Editor).SelectedItem as string ?? s.Value;
            return ((TextField)s.Editor).Text.Trim();
        }

        static void SetEditorDefault(PluginSetting s)
        {
            if (s.Type == "flag") ((Switch)s.Editor).On = s.Default == "1";
            else if (s.Type == "secret") ((TextField)s.Editor).Text = "";
            else if (s.Type == "choice") ((Dropdown)s.Editor).SelectedItem = s.Default;
            else if (s.Type == "int") {
                decimal d;
                if (decimal.TryParse(s.Default, NumberStyles.AllowLeadingSign, CultureInfo.InvariantCulture, out d)) ((NumberField)s.Editor).Value = d;
            } else ((TextField)s.Editor).Text = s.Default;
        }

        // The settings of the selected plugin. The listing comes from the server tool (no DLL runs), so the tab fills a moment after it opens.
        void SettingsTab(FlowLayoutPanel flow, PluginMetadata p)
        {
            var wait = L("Reading the settings…", false, true);
            flow.Controls.Add(wait);
            LoadPluginSettings(flow, wait, p.Id);
        }

        async void LoadPluginSettings(FlowLayoutPanel flow, Label wait, string id)
        {
            if (pluginBusy || updating) { wait.Text = "Try again in a moment."; return; }
            pluginBusy = true;
            SetState(serverState);
            string selectedFolder = folder;
            List<PluginSetting> rows;
            try {
                string output = await Task.Run(() => RunPluginTool(selectedFolder, "--plugin-config " + id));
                rows = ParsePluginSettings(id, output);
            } catch (Exception ex) {
                wait.Text = "Cannot read the settings: " + ex.Message;
                return;
            } finally { pluginBusy = false; SetState(serverState); }
            // The user may have chosen another plugin or tab while the tool ran.
            if (flow.IsDisposed || !pluginDetail.Controls.Contains(flow) || selectedPluginId != id || pluginTab != 2) return;
            if (rows.Count == 0) { wait.Text = "This plugin has no settings."; return; }
            flow.Controls.Remove(wait);
            var tbl = new TableLayoutPanel { AutoSize = true, ColumnCount = 3, Tag = "panel", Margin = new Padding(0, 0, 0, 12) };
            tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 130));
            tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 150));
            tbl.ColumnStyles.Add(new ColumnStyle(SizeType.Absolute, 60));
            var save = new FlatButton("Save") { Kind = "primary", Enabled = false, Margin = new Padding(0, 0, 8, 0) };
            var revert = new FlatButton("Revert") { Enabled = false, Margin = new Padding(0) };
            EventHandler changed = delegate { save.Enabled = true; revert.Enabled = true; save.Invalidate(); revert.Invalidate(); };
            foreach (PluginSetting s in rows) {
                int row = tbl.RowCount++;
                tbl.RowStyles.Add(new RowStyle(SizeType.Absolute, 44));
                var lab = L(s.Label + (s.Live ? "" : " (restart needed)") + (s.Pending ? " *" : ""), false, false);
                lab.MaximumSize = new Size(124, 0);
                lab.Anchor = AnchorStyles.Left;
                tbl.Controls.Add(lab, 0, row);
                s.Editor = SettingEditor(s);
                s.Editor.Anchor = AnchorStyles.Left;
                if (s.Help.Length > 0) { UI.Tips.SetToolTip(lab, s.Help); UI.Tips.SetToolTip(s.Editor, s.Help); }
                tbl.Controls.Add(s.Editor, 1, row);
                PluginSetting current = s;
                var reset = new FlatButton("Default") { Height = 28, Width = 56, Margin = new Padding(0), Anchor = AnchorStyles.Left };
                reset.Click += delegate { current.Reset = true; SetEditorDefault(current); changed(null, EventArgs.Empty); };
                tbl.Controls.Add(reset, 2, row);
                if (s.Editor is Switch) ((Switch)s.Editor).Changed += changed;
                else if (s.Editor is Dropdown) ((Dropdown)s.Editor).SelectedIndexChanged += changed;
                else ((TextField)s.Editor).TextChanged += changed;
            }
            flow.Controls.Add(tbl);
            var done = L("", false, false);
            done.Margin = new Padding(12, 8, 0, 0);
            save.Click += async delegate
            {
                save.Enabled = false; revert.Enabled = false;
                var changes = new List<PluginSetting>();
                foreach (PluginSetting s in rows) {
                    string now = SettingEditorValue(s);
                    // A secret changes only when the administrator typed something, or asked for the default.
                    if (s.Reset || (s.Type == "secret" ? now.Length > 0 : now != s.Value)) { s.Value = now; changes.Add(s); }
                }
                await SavePluginSettings(id, selectedFolder, changes);
                done.Text = "Saved ✓";
                done.Tag = "ok";
                done.ForeColor = UI.T.Ok;
            };
            revert.Click += delegate { RenderPluginDetail(); };
            var bar = new FlowLayoutPanel { AutoSize = true, WrapContents = false, Tag = "panel", Margin = new Padding(0) };
            bar.Controls.Add(save);
            bar.Controls.Add(revert);
            bar.Controls.Add(done);
            flow.Controls.Add(bar);
            Skin.Apply(flow);
        }

        // Saves each changed value with the file command when the server is stopped, and with a local command when the server runs.
        async Task SavePluginSettings(string id, string selectedFolder, List<PluginSetting> changes)
        {
            if (changes.Count == 0) { message.Text = "No changes."; return; }
            int saved = 0;
            var failures = new List<string>();
            foreach (PluginSetting s in changes) {
                // No value means the default. Text cannot be empty, so an empty text asks for the default too.
                bool useDefault = s.Reset || s.Value.Length == 0;
                if (!Regex.IsMatch(s.Value, @"\A[\x20-\x7e]*\z") || s.Value.IndexOf('"') >= 0 || s.Value.IndexOf(';') >= 0) {
                    failures.Add(s.Key);
                    continue;
                }
                if (server != null) {
                    SendPluginCommand("plugins config " + id + " " + s.Key + (useDefault ? "" : " " + s.Value));
                    saved++;
                    continue;
                }
                try {
                    string arguments = "--plugin-config " + id + " " + s.Key + (useDefault ? "" : " " + QuoteProcessArgument(s.Value));
                    await Task.Run(() => RunPluginTool(selectedFolder, arguments));
                    saved++;
                } catch (Exception ex) { failures.Add(s.Key + ": " + ex.Message); }
            }
            message.Text = saved + " setting(s) sent." + (server != null ? " Check the console for the result." : " Changes apply at the next server start.") +
                (failures.Count > 0 ? " Refused: " + string.Join(", ", failures.ToArray()) : "");
        }
    }
}
