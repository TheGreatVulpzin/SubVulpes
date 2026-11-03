# Getting Started
This launcher starts and watches your SubVulpes server.
## First start
- Put the launcher in the server folder, next to `subvulpes_server.exe`.
- Open **Settings**. Set a name and an admin password.
- Go to **Server** and press **Start**.
## Join the game
Players join with the address of your computer and the port in **Settings**. Forward the UDP port on your router.
## Admin password
In the game, type `/admin` and the password in the chat. Keep the password private.

# The Server Page
The top row shows the state of the server and the buttons.
- **Start** starts the server.
- **Stop** asks the server to save and close.
- **Restart** stops the server and starts it again.
- **Kill** ends the server at once. Use it only if Stop does not work.
## Tick rate
The server runs about 60 ticks per second. A much lower number means lag.
## Customize the panel
Select the sliders icon to choose which values and graph the panel shows. Drag the line below the panel to change its height. The launcher remembers your choice.
## Console
The filter shows only Info, Warnings or Errors. The search box shows only the lines with your text. Type a server command in the box at the bottom and press **Enter**.

# Settings
## Server Settings
Select **Save** to write the changes to `config.txt`. **Revert changes** brings back the saved values. **Reset to defaults** sets the numbers back to their default values.
## Requires restart
The launcher saves the setting. The server uses it after the next start.
## Launcher Settings
These settings apply at once. Choose the theme and the accent color here. 
## Past Logs
Each run of the server in Debug Mode saves its own log. This page lists them. Select a log to read or write its notes. **Pack the Selected Log** makes one zip to send.
## Advanced Settings
This page shows when **Debug Mode** is on. Use it to record more data about a run.

# Plugins
Plugins add features to the server.
## Risk
- **low**: reads data or keeps its own files.
- **high**: changes the game.
- **highest**: runs inside the server with full access to it.
Turn on only plugins that you trust.
## The table
Select a column title to sort. Right-click a title to choose the columns. Risk and Source are hidden at first.
## Right-click a plugin
The menu turns the plugin on or off, opens the approval review, copies the id and uninstalls the plugin.
## Workshop
The Workshop tab lists the plugins of the SubVulpes Workshop. The launcher checks the signature of the list, and keeps a saved copy for when you are offline. Select **Refresh** to read it again. Select **Install** to install a plugin, or select a card to see its page and its pictures. An official plugin installs at once. A plugin of the community shows its review first. When the server runs, the install waits and runs at the next start that you make. **Update** shows when the catalog has a newer version.
## Uninstall
**Uninstall…** deletes a plugin: its folder and its approval. The data that the plugin saved stays. Stop the server first. A required plugin, or a plugin that another plugin needs, stays.
## Install
Use `Install from File…` for a `.svplugin` file. The launcher shows what the plugin can do before it installs.
## Approval
A plugin needs your approval before it runs. **Review approval…** shows the review again. Stop the server first.
## Settings of a plugin
Select a plugin, open the **Settings** tab and select **Save**. A setting that says restart needed applies at the next start.

# Logs
## The console log
The file `logs\server.log` has all lines of the last run. Use the save icon in the console to copy it with the password removed.
## Past Logs
In Debug Mode, each run makes its own folder in `logs`. Open **Settings**, then **Past Logs**. Write a note for a log, then pack it into one zip to send it.
## Crash reports
The server can save a dump when it crashes or freezes. Turn it on in **Advanced Settings**. Send the dump with the log.

# Problems
## The server does not start
Read the last lines in the console. Make sure that no other server uses the same port. Check that the folder has `data`.
## The server freezes
The launcher shows Frozen when no speed line comes for a long time. Use **Kill** and start again.
## The launcher cannot update
Check your internet connection. You can also install an update from a `.zip` file in **Launcher Settings**.

# Keyboard
- **F5** reads the plugins again.
- **Ctrl+L** clears the console.
- **Ctrl+F** goes to the console search.
- **Ctrl+S** saves the server settings.
- **Ctrl+Shift+D** turns Debug Mode on or off.
