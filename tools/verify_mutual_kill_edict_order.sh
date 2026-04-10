#!/usr/bin/env bash
# Verifies from SDK sources: clients processed in ascending edict index in G_RunFrame,
# and ClientBeginServerFrame skips runWeapon when deadflag is set — so in a same-tick
# mutual lethal, the lower-index client fires first and can kill the higher before
# the higher's runWeapon runs.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
G_MAIN="$ROOT/SDK/sof-sdk/Source/Game/gamecpp/g_main.cpp"
P_CLIENT="$ROOT/SDK/sof-sdk/Source/Game/gamecpp/p_client.cpp"

for f in "$G_MAIN" "$P_CLIENT"; do
  test -f "$f" || { echo "missing: $f"; exit 1; }
done

# G_RunFrame: ascending loop, ClientBeginServerFrame(ent) for each client edict
grep -q 'for (i=0 ; i<globals.num_edicts' "$G_MAIN" || { echo "fail: expected edict loop in g_main.cpp"; exit 1; }
grep -q 'ClientBeginServerFrame (ent)' "$G_MAIN" || { echo "fail: ClientBeginServerFrame call"; exit 1; }

# ClientBeginServerFrame: deadflag before runWeapon
awk '
  /void ClientBeginServerFrame/ { infn=1 }
  infn && /if \(ent->deadflag\)/ { dead=NR }
  infn && /runWeapon\(ent\)/ { weap=NR; exit }
  END {
    if (!dead || !weap) { print "fail: could not find deadflag vs runWeapon"; exit 1 }
    if (dead > weap) { print "fail: runWeapon before deadflag check (dead=" dead " weap=" weap ")"; exit 1 }
    print "ok: deadflag check at line", dead, "before runWeapon at line", weap
  }
' "$P_CLIENT"

echo "verify_mutual_kill_edict_order: all checks passed"
