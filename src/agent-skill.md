---
name: quickref-viewer
description: Create Markdown notes for the Quickref Viewer application.
---

Notes are plain Markdown files in ~/Notes/quickref-viewer. Preserve existing files.
From the plugin repository, create a note without opening a window:

```sh
./build/quickref-viewer create --name 'Note title' --stdin --json <<'MARKDOWN'
# Note title

Useful explanation.
MARKDOWN
```

Use --notes FOLDER to choose another library. Existing files are never overwritten.
The command exits 0 on success, 1 on error. Toggle the drop-down with ./bin/launch after the Hyprland integration is installed.
