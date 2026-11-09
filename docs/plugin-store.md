# Plugin store plan

This page plans the plugin store: the `SubVulpes-Workshop` repository, the catalog index and the launcher client. The Workshop holds three kinds of content: plugins, mods and bundles (see Kinds below).
Status on 2026-11-03: planned, not started. The launcher already has the Workshop tab and reads a local `catalog.json`.

## Decisions of the owner

1. The repository is `SubVulpes-Workshop`. It holds content (plugins, mods and bundles), never the host or the SDK. Their source stays in `SubVulpes`.
2. GitHub builds the packages. A person never uploads a `.svplugin` or a hash by hand.
3. The launcher gets the catalog from the repository. It shows thumbnails from the repository and keeps a cache.
4. The icons of the official plugins ship inside the launcher. Other icons come from the repository.
5. The format has room for Lua from the start. The launcher accepts `runtime` values `native`, `lua` and `wasm` already.
6. A plugin from the repository installs with one click. An official plugin gets no review window. A plugin of the community gets a short card with its capabilities and an Install button.
7. A file that the owner loads from outside the repository keeps the full review window. That window gets the new look.
8. The server keeps checking approvals. The launcher writes the approval by itself when the hash of the package matches the index.
9. The launcher never restarts the server by itself. An install while the server runs waits, and it applies at the next start that the owner makes by hand.
10. Two seals: `official/` holds the work of the owner and Murilo, and `community/` holds the work of everyone else.

## Scope

Only plugins exist now. The format and the code reserve `mod` and `bundle` so that they do not change later. The launcher lists the `plugin` kind only, until the first mod exists.
A mod changes the client, and the client lives on the computer of each player. So the launcher cannot install a mod on the server. The card of a mod will offer Download. OverRosa will support downloading mods to the computer of the player.

## Custom mods and delivery to players (future)

This part waits for OverRosa. The original client cannot load mods, so nothing here works with it.

1. **Custom mod.** The owner adds a mod that is not in the Workshop. The launcher has a button for it. The owner chooses the files. The launcher makes a `.svmod` with a small manifest (id, version, the list of files with their SHA-256) and keeps it in the folder `mods/` of the server.
2. **Mod list of the server.** The server keeps a list of the mods that it uses. Each entry has the id, the version, the SHA-256, the size and a mark: `required` or `optional`. An entry from the Workshop also has its address.
3. **At join.** The server sends the list to OverRosa. The client checks its own cache by SHA-256. It downloads only what is missing.
4. **Where the client downloads from.** First from the address of the Workshop (or any address in the entry, such as GitHub). If the entry has no address, the client downloads from the server.
5. **Download from the server.** It uses its own channel, in chunks, with a limit of speed and a limit of size. It must not use the shared upload limit of the game (`servermaxbytespersecond`), or one new player slows the game for everyone. The launcher setting says the limit. A side port with HTTP is the simple way. The protocol of the game is the hard way.
6. **Checks.** The client verifies the SHA-256 of each file. A mod holds data only (models, maps, sounds, textures), never code. The client refuses other file types and files over a size limit. A required mod that the player refuses means that the player cannot join. A big download asks the player first.

### Rules for mods on the client

- **Limits (start values, measure with real assets).** At most 16 MB for a file, 64 MB for a mod and 256 MB for all the mods of one server. Allowed types: the data types that the game reads (models, textures, sounds, map files), the manifest and, later, `.lua` scripts. The client refuses executables, libraries, archives inside archives and any other type. It checks the type by the content and not only by the name.
- **Paths.** A file path is relative and has no `..`, no drive letter, no link and no reserved Windows name. The client refuses the whole mod if one path is wrong.
- **A mod may replace a file of the base game, for a session only.** The client does not write into the game folder. It keeps the mod files in its own cache, by hash, and puts them in a layer above the base files while the player is on that server. When the player leaves, the layer goes away and the game is as before. If two mods replace the same file, the order of the server list decides, and the client warns the player.
- **Mod from the Workshop or custom.** A Workshop mod has a seal (Official or Community). A custom mod of a server shows "From this server" and has no seal.

### Scripts on the client (later, after the server Lua)

A mod may carry Lua scripts for advanced things. A server owner is a stranger to the player, so the script is untrusted code. The rules:

1. Data-only mods come first. Client scripts come in a later version, after the Lua of the server (stage S4) has proven its sandbox.
2. The script runs in a sandbox. It has no `io`, `os`, `package`, `require`, `load` or `debug`, and no network. It sees only the API that the client exposes.
3. The API uses capabilities, as the server plugins do. Examples: draw on the screen (HUD), play a sound, read the events of the game. No capability reads the files of the player, the clipboard, other windows or the keys outside the game. No capability takes a screenshot.
4. The client limits the time and the memory of a script. A script that goes over the limit stops, and the client says so.
5. The player sees the capabilities of a server mod before the first join, and can refuse the scripts and still play with the data.
6. A client setting turns all scripts off. The default asks.

### Map mods

What the server code shows (2026-11-03, from `docs/game/level.md`, `docs/game/network.md` and `src/game/level.c`):

- The level has two sources. The terrain and the track come from a **seed**. The server sends the seed to the client in the state packet (type 5). The city comes from the **editor data**: tiles, streets, intersections and building entries, stored in `data/city.sbc` (version 6).
- So the client does not receive the map. It builds the same level from the seed and from its own copy of the data files.

This means that a map mod is possible. It replaces the data files (`city.sbc` and the building files) on the client, for the session, with the same files that the server uses. The SHA-256 in the mod list of the server proves that both sides have the same files. A map mod is always `required`.

Not verified: the client side. This list comes from the server code. We must confirm that the client reads `city.sbc` the same way and builds the level in the same order. OverRosa work will confirm it.
Open: how the saved world of the server reacts to a different map. A different map needs a different saved world, so a server treats it as a new world.

## Kinds

| Kind | What it is |
| --- | --- |
| Plugin | Everything that changes the server. |
| Mod | Everything that changes the client: models, maps, sounds, textures and the like. |
| Bundle | A plugin and a mod together, as one item. |

## Repository layout

```
SubVulpes-Workshop/
  official/plugins/<id>/     official/mods/<id>/     official/bundles/<id>/
  community/plugins/<id>/    community/mods/<id>/    community/bundles/<id>/
  tools/                     the build script and the index generator
  .github/workflows/         the Action that builds and publishes
  .github/CODEOWNERS         only the owner and Murilo approve changes in official/
  CONTRIBUTING.md            how to add content by a pull request
```

Each folder of content holds its manifest, its source or files, `icon.png`, `screenshots/` and `README.md`.
The seal (Official or Community) comes from the top folder. It never comes from the id.

The Action publishes the result to the branch `dist`. It replaces that branch at each build, so the history does not grow with binaries.

```
dist/
  index.json               the catalog
  packages/<id>-<version>.svplugin
  thumbs/<id>-<n>.jpg      small pictures for the list
  images/<id>-<n>.png      full pictures for the plugin page
  icons/<id>.png
```

The Action reads `plugins/<id>/plugin.ini`, builds the package, computes the SHA-256 and writes `index.json`. A pull request builds the same files and does not publish them.

## Catalog index, format 1 (draft)

```json
{
  "format": 1,
  "generated": "2026-11-03T12:00:00Z",
  "plugins": [
    {
      "id": "sv.bans",
      "name": "Bans",
      "description": "Bans players by account or by address.",
      "author": "Vulpzin",
      "version": "0.1.0",
      "kind": "plugin",
      "source": "Official",
      "license": "GPL-3.0",
      "runtime": "native",
      "capabilities": ["read", "write", "storage"],
      "min_api": 1,
      "needs": ["sv.permissions"],
      "package": { "file": "packages/sv.bans-0.1.0.svplugin", "sha256": "<64 hex digits>", "size": 48213 },
      "icon": "icons/sv.bans.png",
      "thumbs": ["thumbs/sv.bans-1.jpg"],
      "images": ["images/sv.bans-1.png"]
    }
  ]
}
```

| Field | Meaning |
| --- | --- |
| `format` | The number of this format. The launcher refuses a number that it does not know. |
| `id`, `name`, `description`, `author`, `version` | As in `plugin.ini`. `author` may be empty. |
| `kind` | `plugin`, `mod` or `bundle`. The launcher filters the Workshop tab by it. |
| `source` | `Official` or `Community`. The Action sets it from the top folder. |
| `license` | The license of the content. The CI refuses content without one. |
| `runtime` | For a plugin only: `native`, `lua` or `wasm`. |
| `capabilities` | For a plugin: the capabilities that it asks for. The launcher shows them as chips. |
| `min_api` | The lowest API version that the content needs. The launcher hides content that the server or the client cannot use. |
| `needs` | Ids of content that must be installed first. |
| `package` | The file, its SHA-256 and its size. The launcher checks both after the download. |
| `icon`, `thumbs`, `images` | Paths in `dist`. A thumbnail is small and light. An image loads only when the user opens the plugin page. |

### The list of versions

An entry may carry an optional list `versions`. The fields at the top of the entry (`version`, `package`, `server_min`, `channel`) describe the newest version, as before, so a launcher that does not know the list still works and shows the newest version.

```json
"versions": [
  { "version": "0.2.0", "file": "packages/sv.bans-0.2.0.svplugin", "sha256": "<64 hex digits>", "size": 48213, "server_min": "0.3.0", "min_api": 1, "channel": "stable", "changelog": "Bans last after a restart.
New /unban command.", "capabilities": ["read", "write", "storage"] },
  { "version": "0.1.0", "file": "packages/sv.bans-0.1.0.svplugin", "sha256": "<64 hex digits>", "size": 47100 }
]
```

| Field | Rule |
| --- | --- |
| `version` | Three groups of digits, with an optional suffix such as `-rc.1`. |
| `file`, `sha256`, `size` | A path under `packages/`, the SHA-256 and the size of the package, at most 70 MiB. The launcher checks both after the download. |
| `server_min` | Optional. Digits and dots, as in the manifest. A server that is older than this does not see the version. A suffix of the server version does not count, so `0.3.0-rc.1` meets `0.3.0`. A server with no readable version accepts any. |
| `min_api` | Optional. A whole number from 1 to 99, and 1 when it is absent. The launcher knows API 1. It hides a version that needs a higher number. |
| `channel` | Optional: `stable` (the default), `beta` or `experimental`. The launcher hides the others unless Include Beta Versions is on. |
| `changelog` | Optional. Plain text of what changed, at most 1200 characters. The install window shows it. |
| `breaking` | Optional. One line, at most 300 characters, of what can break in an old server or an old config. The install window shows it as a warning. The automatic install of an official plugin skips a version that has it and tells the user. |
| `capabilities`, `runtime` | Optional. What this version can do, and where it runs. When absent, the launcher uses the fields at the top of the entry. |

The launcher reads at most 24 versions of an entry and skips an item that breaks a rule. It lists them newest first. The top level wins when the same number appears twice. An entry with no list has one version, the top level.

## What the launcher does with the versions

- **Install and Update** use the newest version that the running server can run. If none can run, Install is off and says so.
- **One window.** Install, Update and Change Version (a button in the My Plugins detail) open the same window. It has a version picker, the changelog of the chosen version and what that version can do. The Workshop list has no version control. The main button says Install, Update or Go back. Going back keeps the data of the plugin in `user\plugins`. Update is off when the Workshop has no newer version, and the status of the plugin reads "(New Update)" when it has one. The window does not change the approval system: an official package installs with no review window, and a community package still gets its review card after the download.
- **The check.** At the start, and then at most once in 24 hours, the launcher reads the index in the background. A failed network is silent, and the time of the check is kept only after a good read (`plugin_check` in `launcher.ini`). A number on the Plugins tab shows how many installed plugins have an update.
- **Launcher update.** The window of a launcher update shows the notes of the release before the download.
- **Automatic install.** The setting "Autoupdate Standard Official Plugins" (`auto_official` in `launcher.ini`) is on by default. When it is on, an official plugin installs its update by itself. A running server never restarts: the update waits for its next start. A community plugin never installs by itself.

The local `catalog.json` of today is the same list without the new fields. The launcher uses the downloaded index as the file that it caches. `file` becomes `package.file`.

## What the launcher does

1. At start, and when the user opens the Workshop tab, it downloads `index.json` from `dist`. It keeps the last good copy in a cache folder. Without a network, it shows the cache.
2. The list shows the icon and the thumbnails from the cache. It downloads a missing thumbnail once.
3. The plugin page loads the full images when the user opens it, and caches them.
4. Install downloads the package and checks the size and the SHA-256. With the server stopped, it runs the installer script and writes the approval, because the hash matches the index. With the server running, it keeps the package as a pending install and tells the user that the plugin applies at the next start. The launcher runs the pending installs just before the next start. It never restarts the server.
5. The Workshop tab marks a plugin that has a newer version than the installed one, and offers Update.
6. The launcher reuses the update code that exists (`ReleaseUpdate`: the request, the download and the checksum).

## Install tiers

| Where the package comes from | What the user sees |
| --- | --- |
| Catalog, source Official | One click. A short line says what the plugin can do. |
| Catalog, source Community | A short card with the author and the capabilities, and an Install button. |
| A file chosen by the owner (`Install from File…`) | The full review window, with the new look. The owner confirms. |

The server stays the last check. It loads a plugin only when its approval matches the package hash.

## Integrity of the catalog

A native plugin runs code with full access. So the catalog must be hard to fake. Requirements:

- The Action builds each package from the source in the repository. The hash comes from that build.
- The launcher downloads only over HTTPS from the one address of the repository.
- The main branch needs a pull request. The owner and Murilo approve it, and both use two-factor sign-in.
- The index is signed. The Action seals `index.json` with a secret key and writes `index.json.sig`. The launcher holds the public part of the key. If the file changes, the seal does not match and the launcher refuses the catalog. If the key is lost, a new launcher version brings a new public key. Decided on 2026-11-03: the index is signed from format 1.

## Order of work

1. Close the launcher (the owner tests it, we fix what breaks).
2. Confirm this format with the owner and the plugin developer who joins as a contributor.
3. Renew the manifest (`plugin.ini`) together with the plugin API renewal, before the store: icon, author, homepage, license and the minimum API version. The server lists the new fields and the launcher reads them. Old plugins keep working.
4. Make the repository: one example plugin, the build script, the index generator, the signature and the Action.
5. Make the launcher client: download, cache, thumbnails, the plugin page from the index, one-click install, pending installs.
6. Add the Uninstall action to the launcher (delete the folder, remove the approval, warn the plugins that depend on it). The Plugins page has no way to uninstall today.
7. Give the review window the new look. Keep it for files from outside the repository.
8. Move the optional plugins to the repository. `sv.permissions` and `sv.admcmd` stay in the server zip.
9. Make the tag and the release of the next minor version (Kit) only after steps 4 to 6 and the SDK update are done. The release page shows the Kit Bodega picture.

## Workshop repository (built on 2026-11-03)

https://github.com/TheGreatVulpzin/SubVulpes-Workshop is public. Done: the layout, `CONTRIBUTING.md`, `CODEOWNERS`, `tools/build_index.py` with 18 tests, the signing key, and the Action. The Action tests the script, refuses committed binaries, builds each plugin with its `build.ps1`, asks the server to check each manifest, packs, writes `index.json`, signs it and replaces the branch `dist`. The branch `main` needs a pull request, one approval from a code owner and a passing build. The owner can still bypass it. Murilo has a pending invitation. The first content is `official/plugins/sv.jukebox`. Its source stays in the main repository, where its tests run. Its `build.ps1` downloads the source at a pinned commit, checks the SHA-256 of each file against `source.sha256`, and builds with the SDK. Until a server release knows the new manifest keys (`license`, `tags`), the Action fails at the manifest check, and `dist` keeps the last good index. Not done: the thumbnails with Pillow (tested only without it locally) and real icons.

## Launcher client (built on 2026-11-03)

The Workshop tab reads the Workshop. It was named Catalog before. Done:

- The launcher downloads `index.json` and `index.json.sig` from the branch `dist` over HTTPS and checks the RSA signature with the key inside the program (`tools/launcher/index-public.pem`, a resource). A catalog that fails the check never shows. The launcher keeps the last good copy in `workshop-cache\` next to the launcher, and checks it again at each load. Without a network, the saved catalog shows.
- The list shows the icons, which load once in the background. The plugin page loads its large pictures when the user opens it. The thumbnails of the index are not used yet.
- Install downloads the package, checks the size and the SHA-256 against the signed index, and runs the existing installer. An official plugin installs without the review window, because the hash of the package must equal the hash of the signed index. The installer checks that hash again. A plugin of the community still shows the full review window. The short card is not built.
- A newer version in the catalog shows an Update button. A beta or experimental plugin shows only with Include Beta Versions.
- With the server running, an install waits. It is saved in `workshop-cache\pending.txt` and runs just before the next start that the user makes. The launcher never restarts the server.
- A `catalog.json` in the server folder still works. It is for developers and for tests, and its entries come first.
- The Plugins page has an Uninstall action (the table menu, the Summary tab and the cards of the Workshop tab). It needs a stopped server. It refuses a required plugin, and a plugin that another installed plugin lists in `dependencies=`. It deletes the plugin folder, its line in `plugins.ini` and its approvals. The data that the plugin saved in `user\plugins` stays.
- The plugin list reads again each time its page opens.

Tests: signature (good, changed data, changed signature, another key, bad key), the index rules, the package check, the needed-by check and the removal, in `tests/launcher_update_test.cs`.

Not done: moving `sv.jukebox` to the Workshop. The jukebox builds from the SDK with CMake, and its tests live in the main repository, so a move needs its own plan. The Workshop has no content yet. The short install card for the community and the unseen screens (the Workshop pages with real data) wait for the first content.
## Manifest renewal (server, installer, pack and launcher done on 2026-11-03)

Done: the server reads and checks the new keys, the installer and `pack.ps1` carry the icon, the tests cover them, and the launcher shows the icon of an installed plugin. Not done: the official manifests (author, license and icon, which change their hashes), the real icons, the `entry` runtime, and the Uninstall action.

The manifest (`docs/plugin-manifest.md`) already has `name`, `description`, `author` and `homepage`. The store needs these new keys. No other key changes. All of them are optional.

| New key | Required | Rule |
| --- | --- | --- |
| `icon` | no | A file name in the plugin folder, such as `icon.png`. A PNG with a valid signature, at most 128 KB, from 64 by 64 to 512 by 512 pixels, and square. |
| `license` | no | An SPDX identifier, such as `GPL-3.0-only`, 1 to 32 characters from `A-Z a-z 0-9 . + -`. The Workshop CI makes it required for all content. The server only checks the form. |
| `kind` | no | `plugin` (the default), `mod` or `bundle`. The server accepts `plugin` only. The other values are reserved. |
| `side` | no | `server` (the default), `client` or `both`. The server accepts `server` only. Reserved for mods and bundles. |
| `server_min` | no | The lowest server version that the plugin needs, such as `0.1.1`. Digits and dots, up to 23 characters. The host refuses a plugin that needs a newer server, with a fixed reason. A build with no version number skips the check. |
| `tags` | no | Up to 5 tags separated by commas, with no spaces. Each tag has 2 to 20 characters from `a-z 0-9 -`. The catalog uses them to filter and search. |
| `channel` | no | `stable` (the default), `beta` or `experimental`. The launcher hides `beta` and `experimental` unless the user turns on Include Beta Versions. |
| `entry` | no | The main file of a script plugin. Reserved for Lua and WASM. A native plugin keeps `library`. When script runtimes exist, a manifest has `library` or `entry`, never both. The server refuses `entry` until then. |

**Already covered.** `dependencies` with a lowest version (`sv.lib@1.2`), `conflicts`, `role` and `live` exist. The `needs` field of the index copies `dependencies`.

**Compatibility.** The ABI key `api` stays as it is. It already says which API the plugin was built for. The index of the store copies it, and the launcher hides a plugin that the server cannot run. A manifest-only change does not raise `api`.

**What changes in each part**

1. **Server.** The manifest reader accepts and checks the new keys. The rules stay in the server, and the installer and the launcher keep asking it. The check of the PNG (signature, size and the dimensions in the header) runs where the folder is validated. The rows of `--plugins-list` and `--plugin-inspect` keep their format. They already carry the author and the homepage.
2. **Installer.** It copies `icon` with the other files. It refuses a package whose `icon` file is missing or fails the check.
3. **Launcher.** It reads `plugins/<id>/icon.png` of an installed plugin, with no new request to the server. The table, the detail panel and the catalog card show it in place of the letter. A plugin without an icon shows the letter, as now.
4. **Official plugins.** Each manifest gets `author=Vulpzin`, `license=GPL-3.0-only` and `icon=icon.png`, with a real icon. This changes their hashes, so the hash list of the package is made again.
5. **Docs.** `plugin-manifest.md` and `plugin-sdk.md` list the new keys.
6. **Tests.** New cases in the manifest group (a good icon, a missing file, a file that is not a PNG, a file too big or not square, a bad license, a kind or a side that is not accepted, a bad `server_min`, too many tags, a bad `channel`, and `entry` before Lua exists). The package tests check that the icon is in the package.

Old plugins keep working. A manifest without the new keys stays valid.

## Official plugins outside the launcher (plan of 2026-11-08)

The owner wants the official plugins to ship and update apart from the launcher and the server. The launcher finds a newer version and installs it. The user can choose a version.

**Decisions.**

- The server zip keeps `sv.permissions` and `sv.admcmd`. A server with no admin commands is not usable, and the first start must not need a network. The release workflow puts the newest signed package of each one into the zip, so the copy in the zip is current on the day of the release.
- `sv.status`, the jukebox and the test room leave the zip. The launcher offers `sv.status` the first time it opens the Plugins page.
- After the first install, every official plugin is a normal Workshop plugin. The launcher updates all of them in the same way.
- Version 1.0 promises that ABI 1 does not break (see the version policy). Until then an update can break a native plugin. The fields `server_min` and `min_api` are the guard.

**What exists (read from the code on 2026-11-08).**

- An official package from the Workshop installs and gets its approval with no review window (`InstallPlugin` in `Launcher.cs`). The hash of the signed index is the proof. An official update does not show Approval Needed.
- The launcher reads the index only when the Plugins page opens for the first time, and when the user presses reload (`RefreshStore`). It does not check at start.
- The index has one version for each plugin. `ParseStoreIndex` reads one `version` and one `package` for an id.
- The launcher does not read `min_api` or `server_min` from the index. The host refuses the plugin when it loads, but the launcher offers an install that cannot run.

**Work.**

1. **Index.** Add an optional list `versions` to each entry: version, package file, hash, size, `server_min`, `min_api` and channel. The fields of format 1 stay, so an older launcher still works and shows the newest version. `tools/build_index.py` in the Workshop repository writes the list from the releases of each plugin that it can find, and keeps the last good packages.
2. **Launcher.** Read `versions`, hide a version that the server cannot use, and add a version choice beside Update. A choice of an older version is a downgrade. It asks first, and it keeps the data of the plugin in `user\plugins`.
3. **Launcher.** Check the index at start in the background, once each day, and when the user presses reload. Show a number on the Plugins tab and an Update button on the row. Never install by itself without the user's choice. A setting turns the automatic install of official updates on, off by default.
4. **Workshop repository.** Move the source of `sv.permissions`, `sv.admcmd` and `sv.status` to `official/plugins/` with a `build.ps1`, as the jukebox does. The main repository keeps their tests.
5. **Release workflow.** `release.yml` downloads the newest signed packages of the two core plugins from `dist`, checks the seal and the hash, and puts them in the zip with `plugins/native-approvals.ini`. If `dist` has no valid package, it builds them from the source of the tag, as it does today. A release never waits for the Workshop.
6. **Order of builds.** The Workshop Action checks each manifest with the newest server release. A new manifest key therefore needs the server release first, and the plugin after it. State this in the Workshop `CONTRIBUTING.md`.

**Risks.**

- A bad plugin update reaches every server that checks. The seal stops a forged package. It does not stop a bug. Keep the older versions in the index, so a user can go back.
- The two core plugins in the zip and in the Workshop must not differ in a way that breaks the approval file. The workflow checks the hash of the packed DLL against the approval line, as `make_package.ps1` does now.

## The source of the official plugins moves to the Workshop (plan of 2026-11-09)

The owner chose to take the source and the tests of the official plugins out of this repository. This replaces item 4 of the plan of 2026-11-08, where the source stayed here and the Workshop built it from a pinned commit. Items 1 to 3 (the index with `versions`, the launcher) stand as written.

**Decisions.**

- **Home.** The Workshop repository holds each plugin in `official/plugins/ID/`: source, `plugin.ini`, `build.ps1` and `tests/`. Code that several plugins share (`official.c`, `permissions.c`) sits in `official/common/`. A header that one plugin offers to another (`sv_permissions.h`, `sv_patchouli.h`, `sv_ayalog.h`) stays with the plugin that offers it.
- **Which plugins.** `sv.permissions`, `sv.patchouli`, `sv.ayalog` and `sv.admcmd` go in the server zip. `sv.status`, `sv.jukebox` and `sv.testroom` are Workshop only, and the launcher offers `sv.status` at the first visit of the Plugins page.
- **What stays here.** The core, the public headers `src/plugins/sv_*.h`, the tests of the API and of the plugin host, the SDK, the test runner and the probe plugin, and the launcher. The SDK keeps its own copy of the `status` example.
- **Tests.** The Workshop CI builds each plugin with the SDK and runs the scenarios of the plugin with the runner of that SDK. The tests of this repository no longer need a plugin of the Workshop. A small fixture plugin takes the place of `sv.status` in them.
- **The server zip is built from a pinned Workshop commit.** The file `tools/release/embedded-plugins.txt` names the commit and the plugins. `release.yml` builds the server and the SDK from the tag, downloads the source at that commit, checks it against the hashes in the file, builds the four plugins with the new SDK, runs their scenarios with the new runner, and writes `plugins/native-approvals.ini`. The zip therefore matches the server of the same release, the first start needs no network, and the release does not wait for a signed package.
- **After the first install,** every official plugin is a normal Workshop plugin that the launcher updates. The signed packages in `dist` are for the launcher. The zip does not read them.

**The order, and why.** The newest plugins call API that the published server and SDK do not have yet (for example `account_number` and `human_remove`). The Workshop CI builds with the newest published release, so it fails until a release carries that API.

1. This repository: this plan, then the launcher work (items 1 to 3). Nothing here depends on a release.
2. This repository: a release candidate of the server and the SDK (a tag, on the owner's request). It still builds the plugins from the local source.
3. Workshop: a pull request that adds the source and the tests of the plugins. Its CI now passes against the candidate. The merge publishes the first signed packages.
4. This repository: a pull request that removes `plugins/` and its tests, adds the fixture plugin, and switches `release.yml` to the pinned build. It lands only after step 3.
5. The release itself (a tag, on the owner's request).

**Risks.**

- The pinned commit is the only link between the two repositories. A wrong commit puts a wrong plugin in the zip. The hashes in `embedded-plugins.txt` and the approval line check it.
- The Workshop gets the right to change what a server zip holds. Only the owner and Murilo approve a pull request there, and a bump of the pinned commit is a normal pull request here.
- The tests of the plugins and the tests of the API are in two repositories. A change of the API needs a pull request in each. State this in `CONTRIBUTING.md` of both.

## Answers of 2026-11-03

- **License.** The repository and the official content use GPL-3.0, as the project does. A community item declares its license in the manifest (`license=`). The CI refuses an item without one.
- **Game files.** The Workshop may hold game data and assets in mods and bundles, as the server zip already does. It never holds the original game executable. The owner accepts this, and `CLAUDE.md` says the same. Content that is not yours needs the permission of its owner.
- **Server zip.** It keeps `sv.status`, `sv.permissions` and `sv.admcmd`. `sv.jukebox` and `sv.testroom` move to the Workshop as Official. This replaces decision 10 of the plan.
- **Install while the server runs.** No restart by the launcher. See decision 9 above.
- **Community rules.** The owner or Murilo approves each pull request. The CI checks the manifest, the license, the size and a build from source. A native plugin of the community enters only after a person reads its C code. A Lua plugin is the easy way.
- **Name.** `SubVulpes-Workshop`.
- **Signature.** The index is signed from format 1 (`index.json.sig`, RSA 3072 with SHA-256). The algorithm is RSA because .NET Framework 4, which the launcher uses, has no Ed25519. The secret key lives only in the secret `INDEX_SIGNING_KEY` of the GitHub repository. No offline copy exists. If the key is lost, a new launcher version brings a new public key. The public key is `tools/index-public.pem` in the Workshop repository, and it will be built into the launcher.

## Open questions

- The manifest of a mod and of a bundle: what they list. A bundle lists its plugin part and its mod part. Decide when the first mod comes.
- How the launcher installs a mod. A plugin goes to `plugins/`. A mod goes to the client, so the launcher needs to know where the client lives (or the mod is a download for the player).
