# master_server_fallback

Provides a known-host selector for the SoFPlus GameSpy master server.

## Runtime behavior

- F12 → Network exposes a `<list>` bound directly to `_sp_cl_master_gamespy`.
- The default is `sof1master.sofvault.org` (registered at **PreCvarInit** so it wins over
  SoFPlus/spcl’s MegaLag factory default; `spm_settings.cfg` still restores a saved choice).
- SoFPlus persists the selected hostname in `User/sofplus/data/spm_settings.cfg`.
- SoF Buddy does not probe, replace, or rotate the selected master.

Known hosts:

```text
sof1master.sofvault.org
sof1master.megalag.org
master.333networks.com
gsm.qtracker.com
master2.qtracker.com
```

