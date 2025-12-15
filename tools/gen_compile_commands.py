#!/usr/bin/env python3
import json, os, sys
if len(sys.argv) < 5: sys.exit(1)
cc, inc, cflags = sys.argv[1], sys.argv[2], sys.argv[3]
sources = sys.argv[4:]
root = os.getcwd()
entries = []
for s in sources:
    if not s.endswith(".cpp"): continue
    if not s.startswith("src/"): continue
    obj = "obj/" + s[4:-4] + ".o"
    cmd = f"{cc} -c {inc} -o {obj} {s} {cflags}"
    entries.append({"directory": root, "command": cmd, "file": os.path.join(root, s)})
if os.path.exists("build/generated_detours.cpp"):
    s = "build/generated_detours.cpp"
    obj = "obj/generated_detours.o"
    cmd = f"{cc} -c {inc} -o {obj} {s} {cflags}"
    entries.append({"directory": root, "command": cmd, "file": os.path.join(root, s)})
json.dump(entries, sys.stdout, indent=4)








