# Plugin store plan

This page plans the plugin store: the `SubVulpes-Workshop` repository, the catalog index and the launcher client. The Workshop holds three kinds of content: plugins (server), packages (game data) and mods (client, OverRosa).
Status on 2026-11-03: planned, not started. The launcher already has the Catalog tab and reads a local `catalog.json`.

## Decisions of the owner

1. The repository is `SubVulpes-Workshop`. It holds content (plugins, packages and mods), never the host or the SDK. Their source stays in `SubVulpes`.
2. GitHub builds the packages. A person never uploads a `.svplugin` or a hash by hand.
3. The launcher gets the catalog from the repository. It shows thumbnails from the repository and keeps a cache.
4. The icons of the official plugins ship inside the launcher. Other icons come from the repository.
5. The format has room for Lua from the start. The launcher accepts `runtime` values `native`, `lua` and `wasm` already.
6. A plugin from the repository installs with one click. An official plugin gets no review window. A plugin of the community gets a short card with its capabilities and an Install button.
7. A file that the owner loads from outside the repository keeps the full review window. That window gets the new look.
8. The server keeps checking approvals. The launcher writes the approval by itself when the hash of the package matches the index.
9. The launcher never restarts the server by itself. An install while the server runs waits, and it applies at the next start that the owner makes by hand.
10. Two seals: `official/` holds the work of the owner and Murilo, and `community/` holds the work of everyone else.

## Repository layout

```
SubVulpes-Workshop/
  official/plugins/<id>/     official/packages/<id>/     official/mods/<id>/
  community/plugins/<id>/    community/packages/<id>/    community/mods/<id>/
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
| `kind` | `plugin`, `package` or `mod`. The launcher filters the Catalog tab by it. |
| `source` | `Official` or `Community`. The Action sets it from the top folder. |
| `license` | The license of the content. The CI refuses content without one. |
| `runtime` | For a plugin only: `native`, `lua` or `wasm`. |
| `capabilities` | For a plugin: the capabilities that it asks for. The launcher shows them as chips. |
| `min_api` | The lowest API version that the content needs. The launcher hides content that the server or the client cannot use. |
| `needs` | Ids of content that must be installed first. |
| `package` | The file, its SHA-256 and its size. The launcher checks both after the download. |
| `icon`, `thumbs`, `images` | Paths in `dist`. A thumbnail is small and light. An image loads only when the user opens the plugin page. |

The local `catalog.json` of today is the same list without the new fields. The launcher uses the downloaded index as the file that it caches. `file` becomes `package.file`.

## What the launcher does

1. At start, and when the user opens the Catalog tab, it downloads `index.json` from `dist`. It keeps the last good copy in a cache folder. Without a network, it shows the cache.
2. The list shows the icon and the thumbnails from the cache. It downloads a missing thumbnail once.
3. The plugin page loads the full images when the user opens it, and caches them.
4. Install downloads the package and checks the size and the SHA-256. With the server stopped, it runs the installer script and writes the approval, because the hash matches the index. With the server running, it keeps the package as a pending install and tells the user that the plugin applies at the next start. The launcher runs the pending installs just before the next start. It never restarts the server.
5. The Catalog tab marks a plugin that has a newer version than the installed one, and offers Update.
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
3. Make the repository: one example plugin, the build script, the index generator and the Action.
4. Make the launcher client: download, cache, thumbnails, the plugin page from the index, install by one click.
5. Renew the manifest (`plugin.ini`): icon, author, homepage and the minimum API version. The server lists the new fields and the launcher reads them. Old plugins keep working.
6. Give the review window the new look. Keep it for files from outside the repository.
7. Move the optional plugins to the repository. `sv.permissions` and `sv.admcmd` stay in the server zip.

## Answers of 2026-11-03

- **License.** The repository and the official content use GPL-3.0, as the project does. A community item declares its license in the manifest (`license=`). The CI refuses an item without one.
- **Game files.** The Workshop may hold game data and assets in packages and mods, as the server zip already does. It never holds a game executable. The owner accepts this. Content that is not yours needs the permission of its owner.
- **Server zip.** It keeps `sv.status`, `sv.permissions` and `sv.admcmd`. `sv.jukebox` and `sv.testroom` move to the Workshop as Official. This replaces decision 10 of the plan.
- **Install while the server runs.** No restart by the launcher. See decision 9 above.
- **Community rules.** The owner or Murilo approves each pull request. The CI checks the manifest, the license, the size and a build from source. A native plugin of the community enters only after a person reads its C code. A Lua plugin is the easy way.
- **Name.** `SubVulpes-Workshop`.
- **Signature.** The index is signed from format 1 (`index.json.sig`, ed25519). The secret key lives in the secrets of the GitHub repository. The public key is built into the launcher.

## Open questions

- The manifest of a package and of a mod: what they list. Decide when the first package or mod comes.
- How the launcher installs a package and a mod. A plugin goes to `plugins/`. A package and a mod need their own folders and rules.
