# SofVault XP updater mirror

XP builds fetch updates over HTTP from `sofvault.org/sof_buddy/releases/` (see `src/core/update_command.cpp`).

## Server setup (cron pull)

Install `sync_from_github.sh` on the sofvault host. It polls GitHub’s release APIs, downloads `release_windows_xp.zip` when the tag changes, and writes HTTP manifests for in-game updater checks.

Requires: `curl`, `jq`

```bash
chmod +x sync_from_github.sh
SOFVAULT_ROOT=/var/www/html/sofvault.org/sof_buddy/releases ./sync_from_github.sh
```

Example crontab:

```
*/30 * * * * /path/to/sync_from_github.sh >> /var/log/sofvault-buddy-sync.log 2>&1
```

Output layout:

```
$SOFVAULT_ROOT/
  latest.json          # newest GitHub release (updates every sync)
  stable.json          # blessed stable release (only tools/set_stable_release.py)
  list.json            # GitHub /releases?per_page=100 (Refresh Release List on XP)
  latest               # plain tag text (newest)
  stable               # plain tag text (stable pointer)
  .synced_tag          # last downloaded tag
  by_tag/              # per-tag manifests for install-version picker
    v6.6-build170.json
  files/
    release_windows_xp.zip   # only the latest XP zip is mirrored
```

XP **Refresh Release List** reads `list.json`. **Check/download** for a specific tag uses `by_tag/<tag>.json`. Only the current latest tag has a `zip_url` on the mirror; older tags can be selected for version comparison but cannot be downloaded over HTTP until their zip is archived on sofvault.

No SSH push from GitHub Actions is required; the server pulls assets like the sof1maps mirror.
