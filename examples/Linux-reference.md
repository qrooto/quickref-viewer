# Linux reference

## Disk usage

```sh
# Free space on mounted filesystems.
df -h
# Total size of the current directory.
du -sh .
```

## Services

```sh
# List failed services.
systemctl --failed
# Read the current user's journal from this boot.
journalctl --user -b
```
