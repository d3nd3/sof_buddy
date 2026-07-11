# SofVault XP updater mirror

XP builds fetch updates over HTTP from `sofvault.org/sof_buddy/releases/` (see `src/core/update_command.cpp`).

## Server setup (cron pull)

Install `sync_from_github.sh` on the sofvault host. It polls GitHub’s latest release API, downloads `release_windows_xp.zip` when the tag changes, and writes `latest.json` for in-game updater checks.

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
  latest.json          # manifest (tag_name, html_url, zip_url)
  latest               # plain tag text
  .synced_tag          # last downloaded tag
  files/
    release_windows_xp.zip
```

No SSH push from GitHub Actions is required; the server pulls assets like the sof1maps mirror.
