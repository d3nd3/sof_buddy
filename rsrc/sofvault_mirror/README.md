# SofVault XP updater mirror

Windows XP builds fetch updates over plain HTTP from
`sofvault.org/sof_buddy/releases/` (see `src/core/update_command.cpp`). XP's
WinHTTP cannot negotiate modern TLS/SNI, so the updater must never be sent
through an HTTP → HTTPS redirect.

## Why the `files/` tree matters

The live host currently exempts only these paths from its global redirect:

```
/sof_buddy/releases/latest.json
/sof_buddy/releases/latest
/sof_buddy/releases/files/...
```

Everything else, including `list.json` and `by_tag/`, answers with a 301 to
`https://` and fails on XP with `win32 error 12175`. The sync script therefore
publishes every manifest the updater needs under `files/`, and the client
prefers those copies before falling back to the legacy root paths.

## Server setup (cron pull)

Install `sync_from_github.sh` on the sofvault host. It polls GitHub’s release
APIs, downloads the current stable and unstable XP packages when their tags
change, and writes the HTTP manifests.

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
  latest.json                 # newest stable release (legacy, HTTP-clean)
  latest                      # plain stable tag text (legacy, HTTP-clean)
  list.json                   # legacy compact list (root path redirects)
  by_tag/<tag>.json           # legacy per-tag manifests (root path redirects)
  .synced_tag                 # last stable package tag
  .synced_unstable_tag        # last unstable package tag
  files/
    latest.json               # same stable manifest, HTTP-clean
    stable.json               # stable channel manifest
    unstable.json             # newest prerelease channel manifest
    list.json                 # compact install-version list
    by_tag/<tag>.json         # per-tag manifests, HTTP-clean
    release_windows_xp.zip             # current stable XP package
    release_windows_xp_unstable.zip    # current unstable XP package
```

Only the two channel packages are archived. Older tags appear in the list for
version comparison but have an empty `zip_url`; the XP updater reports that the
package is not mirrored instead of downloading the current build under an older
tag.

Channel behaviour:

- **Stable** = GitHub’s newest non-prerelease release (`/releases/latest`).
- **Unstable** = the newest prerelease in the release list.
- The startup check uses `latest.json` directly when its tag belongs to the
  selected channel; `stable.json`/`unstable.json` cover the other channel.

## nginx: keep XP endpoints on plain HTTP

If the site-wide port-80 server block redirects to HTTPS, keep an exception for
the whole updater tree **before** the redirect. The `files/` subtree is the
minimum required for current builds:

```nginx
# In the port-80 server block, before the global HTTP -> HTTPS redirect:
location ^~ /sof_buddy/releases/ {
    root /var/www/html/sofvault.org;
    try_files $uri =404;
}
```

If the redirect is implemented per-location, do not add HSTS or a redirect for
`/sof_buddy/releases/files/`, `latest.json`, or `latest`.

Verify from any host after changing nginx:

```bash
chmod +x check_http.sh
./check_http.sh
# or against a staging vhost:
SOFVAULT_BASE=http://127.0.0.1/sof_buddy/releases ./check_http.sh
```

The script fails on any XP-critical 3xx/4xx/5xx and prints the redirect target.
Legacy root `list.json`/`by_tag` redirects are printed as warnings because the
current client reads the `files/` copies instead.

No SSH push from GitHub Actions is required; the server pulls assets like the
sof1maps mirror.
