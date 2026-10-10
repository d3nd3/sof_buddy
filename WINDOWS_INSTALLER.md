# Windows installer and universal build

The Windows installer packages one `sof_buddy.dll` built with every selectable
feature. It does not compile a DLL during installation. The installer writes the
selected runtime feature set to:

```text
<SoF directory>\sof_buddy\features.cfg
```

The initial selections mirror `features/FEATURES.txt`: uncommented feature names
are enabled and commented feature names are disabled. The custom setup page keeps
the recommended/tested and un-recommended/unstable split, then groups features
by type (for example Security Fixes, Bug Fixes, Graphics, Input, and
Networking). Each type is also a checkbox, while individual features remain
selectable below it. The recommended setup uses the repository defaults; the
custom setup lets the user choose categories or individual features.

## Runtime feature selection

When the universal DLL is present, the F12 menu has a **Features** tab. Each
feature is represented by an archived `_sofbuddy_feature_<name>` cvar with
`Off`, `On`, and `Unavailable` states. The cvars are saved in:

```text
<SoF basedir>\base\sofbuddy.cfg
```

Feature hooks and low-level patches are registered during startup, so changing a
feature cannot safely take effect in the current process. The tab explicitly
shows **Restart SoF to apply changes.**

On startup, before feature registration:

1. The DLL loads its installed `sof_buddy/features.cfg`.
2. It reads saved `_sofbuddy_feature_*` selections from `base/sofbuddy.cfg`.
3. Saved selections override the installed defaults and are written back to
   `sof_buddy/features.cfg`.
4. Feature registration uses that resulting `features.cfg` selection.

Thus `features.cfg` is the early-startup source of truth. The archived cvars are
the persistent user-facing selections that regenerate it. If no feature cvars
have been saved yet, an existing `features.cfg` (or the compiled defaults) is
kept.

In a non-universal DLL, features that were not compiled into that DLL display as
**Unavailable** and are forced back to the unavailable value if selected. They
cannot be enabled by editing either configuration file; installing the universal
DLL is required.

## Updates

The release workflow publishes separate channels:

| Channel | Windows | Linux/Wine |
| --- | --- | --- |
| Compile-time/default | `release_windows.zip` | `release_linux_wine.zip` |
| Universal | `release_windows_universal.zip` | `release_linux_wine_universal.zip` |

The updater is compiled with the channel identity. A normal build selects the
compile-time/default asset; a universal build selects the universal asset. It
never uses the universal asset as a fallback for a normal build, so updating a
compile-time installation does not silently turn it into a universal one.

Builds published before the channel split have no channel marker. Upgrade those
installations manually with the matching package once; subsequent updater runs
will use the correct channel.

The installer payload and the Windows universal package are the same universal
channel. Universal packages and the installer payload bundle both Windows (`.cmd`, `.ps1`)
and Linux/Wine (`.sh`) helper scripts, ensuring users have native scripts available whether
running directly on Windows or under Wine/Proton. The updater does not compile a new DLL.

Existing `base/sofbuddy.cfg` is retained during a normal update. On the next
launch, a universal DLL reads its archived feature cvars and regenerates
`sof_buddy/features.cfg`, so a universal update does not silently reset a user's
feature choices. The updater's optional **fresh settings** prompt removes saved
`sofbuddy.cfg`; after that, the selected package's defaults are used.

The update ZIP is applied while SoF is closed. If `features.cfg` is edited
manually, it is used until saved feature cvars are present; those cvars take
precedence at the next startup by design.

## Automatic DLL activation

On Windows, Setup runs `sof_buddy/enable_sofplus_and_buddy.cmd` against the
selected `SoF.exe`. Under Wine, it applies the same patch natively instead of
requiring Windows PowerShell. The original executable is preserved as
`SoF.exe.bak` before patching; an existing backup is never overwritten. If
activation fails, run the same script manually from the SoF folder.

The Windows 10+ compatibility fix is skipped under Wine because it only applies
to Windows' application-compatibility database.

## Installation options

### Windows 10+ Application Compatibility Fix

The checked-by-default **Apply Windows 10+ Application Compatibility fix**
option on the installer's separate **Select Additional Tasks** page runs
`sof_buddy/patch_windows_compat.ps1` after installation. The
script validates that `SoF.exe` is a 32-bit PE with image base `0x20000000`,
maps virtual address `0x2015F1C0` to the file, verifies the expected
`Raven Software` string, and zeroes that string. This prevents the Windows
Application Compatibility Toolkit database match used by the GOG-style fix.

The original executable is saved as `SoF.exe.sofbuddy.bak`. Unexpected PE
layouts or bytes are rejected without modifying the executable. The script is
idempotent for an already-patched executable, but the installer does not
automatically restore the backup when uninstalled.

### Full violence

The checked-by-default **Unlock full violence** option on the installer's
separate **Select Additional Tasks** page writes the current user's SoF
parental-control registry values under:

```text
HKCU\Software\Raven Software\SoF
```

It is highly recommended when not using SoFPlus's `spcl.dll`. The option derives the
values from the volume serial of the selected installation drive and uses the
password `sof`. The operation is independent of DLL feature selection and is
not removed by uninstall.

## Building and packaging

From the repository root:

```bash
make BUILD=release all
make BUILD=release FEATURE_SET=all universal
python3 tools/generate_installer.py
```

The compile-time/default output is `bin/sof_buddy.dll`; the universal output is
`bin/sof_buddy-universal.dll`. The installer payload must contain the latter as
`sof_buddy.dll`, the `sof_buddy/` runtime files, and the generated
`installer/sof_buddy.iss`. Compile the Inno Setup script on Windows with
`ISCC.exe`; the output is `installer/output/sof_buddy_setup.exe`.

The generated feature list, installer components, and default runtime selection
all come from `features/FEATURES.txt`. If that file changes, regenerate the
installer and rebuild both the normal and universal configurations.
