![Quickref](preview.png)

# Quickref

A small Qt Quick Markdown reference for Omarchy. Press **Super + Ё / `** to drop it down over your work, then press the same keys to hide it.

Notes are ordinary `.md` files in `~/Notes/cheatsheets`. Subfolders are indexed recursively. The sidebar searches filenames, headings and full text; clicking a result opens a tab. Code blocks have syntax accents and a copy button. Open tabs and scroll positions survive hiding and restarting the app. Editing or adding files updates the library automatically.

The app is a viewer. **Open in editor** uses your desktop's Markdown file association; **Notes folder** opens the file manager. Originals are never rewritten. Session state lives in `~/.local/share/quickref/session.json` and is saved atomically.

## Build and install

Requires Qt 6 base, declarative and Wayland, qmake6, make and a C++17 compiler.

```sh
bin/build
bin/test
bin/install
```

`bin/install` builds and installs an Arch package. Alternatively, `bin/install-user` installs the built binary and launcher under `~/.local` without root; this is the installation used on this computer. To build the package without installing, run `makepkg -f` in `pkgbuild/`.

## Omarchy integration

Copy `integration/quickref.lua` to `~/.config/hypr/quickref.lua` and add this after the default modules in `~/.config/hypr/hyprland.lua`:

```lua
require("hypr.quickref")
```

The integration launches `~/.local/bin/quickref`; for a package-only install, change that command to `quickref open`.

Then run `hyprctl reload` and `hyprctl configerrors`.

This replaces the original **Super + grave** binding (the Ё key on a Russian keyboard). It uses its own `special:quickref` workspace and Omarchy's existing slide animation. The original `qconsole` and its agent are preserved; agents in normal tiling remain available on Super+Ctrl+Shift+A. Remove the `require` line and reload to restore the original hotkey. Back up config files before changing them.

The panel covers 65% of the usable screen height and 86% of its width. It follows the monitor used to open it. Change the proportions in `quickref.lua` if needed.

Create your notes folder and copy any examples you want from `examples/`. Existing files should never be overwritten when seeding examples.

## Keyboard

| Shortcut | Action |
| --- | --- |
| Super + Ё / grave | Show / hide (Omarchy integration) |
| Ctrl + K | Search all notes |
| Enter in search | Open first result |
| Down in search, then Enter | Select a result with keyboard |
| Ctrl + Tab / Ctrl + Shift + Tab | Next / previous tab |
| Ctrl + W | Close tab |
| Ctrl + E | Open note in external editor |
| Ctrl + O | Open notes folder |
| Space / PageDown / PageUp | Scroll when the reader has focus |
| F5 | Reload notes |
| ? | Shortcut help |
| Q | Quit when search is not focused |

## Command line

```sh
quickref open
quickref --notes /path/to/notes --state /path/to/session.json
```

For an isolated preview:

```sh
QT_QPA_PLATFORMTHEME=basic QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  build/quickref --notes examples --state /tmp/quickref-session.json \
  --screenshot /tmp/quickref.png
```

The interface follows the accent and rounding of Omarchy and watches its language setting live. English, Russian and Danish catalogs are bundled; Russian is the default if Omarchy has no language file. Missing translations fall back to English. Images and raw HTML in notes are not rendered; links open in the browser only when clicked. Syntax coloring is intentionally lightweight, not a full language parser. Very large libraries are indexed in memory.

Theme and translation support is adapted from Omarchy's MIT-licensed app templates; see LICENSE.
