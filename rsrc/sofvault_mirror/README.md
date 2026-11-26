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
  list.json            # GitHub release list (stable + unstable channels on XP)
  latest               # plain tag text (newest)
  .synced_tag          # last downloaded tag
  by_tag/              # per-tag manifests for install-version picker
    v6.6-build170.json
  files/
    release_windows_xp.zip   # only the latest XP zip is mirrored
```

XP **Refresh Release List** reads `list.json`; stable is the newest non-prerelease entry and unstable is the newest prerelease. **Check/download** for a specific tag uses `by_tag/<tag>.json`. Only the current latest tag has a `zip_url` on the mirror; older tags can be selected for version comparison but cannot be downloaded over HTTP until their zip is archived on sofvault.

The XP updater also uses `latest.json` directly when its tag belongs to the requested channel (stable or unstable). That keeps the startup check working even if the full release list is temporarily unavailable.

## nginx: keep XP endpoints on plain HTTP

Windows XP WinHTTP cannot negotiate modern TLS/SNI, so every path above must return **200 over `http://`** without redirecting to `https://`. If the site-wide HTTP server block has a global redirect, add an exception for the updater tree, for example:

```nginx
# In the port-80 server block, before the global HTTP -> HTTPS redirect:
location ^~ /sof_buddy/releases/ {
    root /var/www/html/sofvault.org;
    try_files $uri =404;
}
```

This covers `latest.json`, `list.json`, `by_tag/`, `latest`, and `files/`. If the redirect is implemented per-location with `return 301 https://...`, add the same exception before those locations. Do not add HSTS on this tree: XP ignores it, but the redirect itself is what breaks the updater.

Verify from any host after changing nginx:

```bash
chmod +x check_http.sh
./check_http.sh
# or against a staging vhost:
SOFVAULT_BASE=http://127.0.0.1/sof_buddy/releases ./check_http.sh
```

The script fails on any 3xx/4xx/5xx and prints the redirect target. Run it before publishing a release so an HTTPS redirect cannot silently break XP updates again.

No SSH push from GitHub Actions is required; the server pulls assets like the sof1maps mirror.
