# Publish a plugin to the Workshop

The [SubVulpes Workshop](https://github.com/TheGreatVulpzin/SubVulpes-Workshop) is the catalog of plugins. The launcher reads it.
A plugin enters the Workshop by a pull request. A maintainer reads the code before it merges.

## The plugin folder

Make the folder `community/plugins/<id>/` in a fork of the Workshop. The folder name is the plugin id.

| File | Purpose |
| --- | --- |
| `plugin.ini` | The manifest. It needs `license`. Add `name`, `description`, `author` and `tags`. |
| `*.c` | The source of the plugin. |
| `build.ps1` | Builds the library that `plugin.ini` names. Copy `template\build.ps1` from this SDK. |
| `icon.png` | Optional. A square PNG. Use 256 by 256 pixels. The limits are 64 to 512 pixels and 128 KiB. Name it in `icon=`. |
| `screenshots/` | Optional. Up to 8 PNG or JPEG files, at most 2 MiB each. The plugin page shows them. |
| `README.md` | What the plugin does. |

Do not add a DLL, a ZIP or any other binary file. The Action builds the library from your source.

## The manifest for the catalog

```ini
[plugin]
id=my.hello
version=0.1.0
library=hello.dll
api=1
live=1
capabilities=read
name=Hello
description=A command that writes the server name and the player count to the log.
author=Your name
license=GPL-3.0-only
icon=icon.png
tags=example,chat
```

The catalog reads `name`, `description`, `author`, `license`, `icon`, `tags`, `channel`, `homepage` and `capabilities`.
See [the manifest rules](plugin-manifest.md). Content without `license` does not enter the Workshop.

## Build on your computer

`build.ps1` compiles every `.c` file next to it. Point it to this SDK, then run it:

```powershell
$env:SUBVULPES_SDK = 'C:\path\to\SubVulpes-sdk-VERSION'
.\build.ps1
```

The library appears in the plugin folder. Do not commit it.
The Action does the same step. It sets `SUBVULPES_SDK` itself.

## What the Action checks

1. The folder name equals the id, and the manifest has a license.
2. The icon and the pictures follow the limits above.
3. The server program checks the manifest with the same rules as the launcher.
4. The Action packs the plugin and writes the hash into the signed index.

A pull request builds and checks. It publishes nothing. The index changes only after a maintainer merges.

## What the user sees

A plugin of the community shows a short card with the author and the capabilities. The user selects Install.
The server stays the last check. It loads a plugin only when the approval matches the hash of the package.
Native code has full access to the server process. A maintainer reads the C code before a native plugin enters.
