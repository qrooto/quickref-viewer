---
name: quickref
description: Create Markdown notes for the user's Quickref drop-down reference.
---

Quickref stores ordinary Markdown files in ~/Notes/cheatsheets. Read existing notes directly. Create a note without a display using:

```sh
quickref create --name 'Note title' --stdin --json <<'MARKDOWN'
# Note title

Useful explanation.
MARKDOWN
```

Use --notes FOLDER to select another library. Existing files are never overwritten. The command exits 0 on success, 1 on error, with JSON on stdout or stderr. Keep notes concise, explain command flags, and preserve the user's existing files. The GUI is `quickref open` and Super+grave toggles it when the Omarchy integration is installed.
