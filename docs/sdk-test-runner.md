# Plugin test runner

The runner runs native plugins.

The runner tests a plugin without a game client and without game data. It links the same game sources as the server, builds a small world with the real constructors, loads your plugin, and runs a scenario file.

## Run a scenario

```text
tools/run_plugin_test.ps1 -Plugin PATH -Scenario FILE.svtest -ServerExe subvulpes_server.exe
```

`PATH` is a plugin folder (with `plugin.ini` and the library), or a `.svplugin` or `.zip` package.
The script does these steps:

1. It checks the plugin with the rules of the server, and it stops on a problem.
2. It makes a clean test root in the temporary folder, with your plugin and the probe plugin `sv.probe`.
3. It approves the two DLLs in that root only. This approval does not touch your server.
4. It runs `subvulpes_plugin_test.exe` in the root, and it removes the root at the end.

The exit code is 0 when every step holds and 1 when a step fails. The runner prints a line for each step, and the log lines of the plugins. A failure prints `FAIL`, the file, the step number and the cause.
The random seed is fixed unless the scenario sets it, so a run repeats.

## The scenario file

A scenario is a text file with one step on each line. A line that starts with `#` is a comment.

```text
seed 1
spawn player Ana team=1 admin=1
tick 5
console hello
expect log "Hello from"
expect prop human.health Ana 100
expect no-failure
```

| Step | Meaning |
| --- | --- |
| `seed N` | Fixes the random seed. |
| `spawn player NAME [team=N] [admin=0\|1]` | Makes a player with a body. The name has 1 to 31 characters and no space. At most 16 players. |
| `tick N` | Runs N ticks of the plugin host, from 1 to 100000. The game clock and the physics do not move. |
| `say PLAYER TEXT` | A chat text of the player, at most 63 characters. A text that starts with `/` can be a plugin command. |
| `console TEXT` | A command of the local operator. A plugin must handle it, or the step fails. |
| `give TYPE PLAYER` | Makes an item of the type and puts it in a hand of the player. |
| `set PROP PLAYER VALUE` | Sets a property of the property table. The call checks the range, as the API does. |
| `expect log "TEXT"` | Some log line has the text. |
| `expect no-log "TEXT"` | No log line has the text. |
| `expect count N "TEXT"` | Exactly N log lines have the text. |
| `expect prop PROP PLAYER VALUE` | The property has the value. A number matches within 0.001. |
| `expect entities KIND N` | The game has N `players`, `humans`, `items` or `objects`. |
| `expect failed` / `expect no-failure` | A plugin failed to run, or no plugin did. |

Properties are the names of the property table, such as `human.health`. The list is in [the game API page](plugin-game-api.md).

## The probe plugin

The steps `set`, `give`, `expect prop` and `expect entities` use the plugin `sv.probe`. It is a normal plugin that calls the public API. A scenario therefore meets the same capability and range checks as your plugin. The probe answers with log lines that start with `PROBE`, which you can match with `expect log`.
Your plugin must not use the command name `probe`.

## What the runner does not do

- It does not move the game clock or the physics. `tick` calls the plugin host only.
- It tests native plugins. A plugin with `dependencies` needs its libraries in the same root, and the script does not copy them yet.
- It does not show what the original client shows. Use the test room in the game for that.

## Session and login fixtures

The runner accepts `leave PLAYER`. A later `spawn player NAME` with the same name reuses that fixture account.
The public fixture password is `svtest`. It exists only in the runner and never changes a server profile.
These steps test session expiry and password hooks without game data or a client.

The normal item and bullet setup runs in the fixture, so magazines fit the real weapon types.
The fixture also runs normal body setup and sets the cached root position that a normal simulation tick supplies.
