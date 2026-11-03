# Plugin store plan

This page plans the plugin store: the `SubVulpes-Plugins` repository, the catalog index and the launcher client.
Status on 2026-11-03: planned, not started. The launcher already has the Catalog tab and reads a local `catalog.json`.

## Decisions of the owner

1. The repository is `SubVulpes-Plugins`. It holds plugins only. The source of the host and the SDK stays in `SubVulpes`.
2. GitHub builds the packages. A person never uploads a `.svplugin` or a hash by hand.
3. The launcher gets the catalog from the repository. It shows thumbnails from the repository and keeps a cache.
4. The icons of the official plugins ship inside the launcher. Other icons come from the repository.
5. The format has room for Lua from the start. The launcher accepts `runtime` values `native`, `lua` and `wasm` already.
6. A plugin from the repository installs with one click. An official plugin gets no review window. A plugin of the community gets a short card with its capabilities and an Install button.
7. A file that the owner loads from outside the repository keeps the full review window. That window gets the new look.
8. The server keeps checking approvals. The launcher writes the approval by itself when the hash of the package matches the index.

## Repository layout

```
SubVulpes-Plugins/
  plugins/<id>/            source, plugin.ini, icon.png, screenshots/, README.md
  tools/                   the build script and the index generator
  .github/workflows/       the Action that builds and publishes
  CONTRIBUTING.md          how to add a plugin by a pull request
```

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
      "source": "Official",
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
| `source` | `Official` or `Community`. The Action sets it from the folder of the plugin, never from the id. |
| `runtime` | `native`, `lua` or `wasm`. |
| `capabilities` | The capabilities that the plugin asks for. The launcher shows them as chips. |
| `min_api` | The lowest plugin API version that the plugin needs. The launcher hides a plugin that the server cannot run. |
| `needs` | Ids of plugins that must be installed first. |
| `package` | The file, its SHA-256 and its size. The launcher checks both after the download. |
| `icon`, `thumbs`, `images` | Paths in `dist`. A thumbnail is small and light. An image loads only when the user opens the plugin page. |

The local `catalog.json` of today is the same list without the new fields. The launcher uses the downloaded index as the file that it caches. `file` becomes `package.file`.

## What the launcher does

1. At start, and when the user opens the Catalog tab, it downloads `index.json` from `dist`. It keeps the last good copy in a cache folder. Without a network, it shows the cache.
2. The list shows the icon and the thumbnails from the cache. It downloads a missing thumbnail once.
3. The plugin page loads the full images when the user opens it, and caches them.
4. Install downloads the package, checks the size and the SHA-256, and runs the installer script. The approval is written when the hash matches the index.
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
- The main branch needs a pull request and the owner approves it. The owners use two-factor sign-in.
- Open decision: sign `index.json` with a key whose public part is built into the launcher. This also protects the catalog if the repository is taken over. Decide before the first public release of the catalog.

## Order of work

1. Close the launcher (the owner tests it, we fix what breaks).
2. Confirm this format with the owner and the plugin developer who joins as a contributor.
3. Make the repository: one example plugin, the build script, the index generator and the Action.
4. Make the launcher client: download, cache, thumbnails, the plugin page from the index, install by one click.
5. Renew the manifest (`plugin.ini`): icon, author, homepage and the minimum API version. The server lists the new fields and the launcher reads them. Old plugins keep working.
6. Give the review window the new look. Keep it for files from outside the repository.
7. Move the optional plugins to the repository. `sv.permissions` and `sv.admcmd` stay in the server zip.

## Open questions

- The license of the plugins repository, and the license of each plugin.
- Does `sv.status` stay in the server zip? Do `sv.jukebox` and `sv.testroom` move to the store?
- Do we sign the index? See the integrity part above.
- Does a plugin install while the server runs? Today the launcher stops the server first (see the idea in `docs/ideas.md`).
- Rules for a community plugin: who reviews the pull request, and what the CI checks (the manifest, the capabilities, a build from source).
