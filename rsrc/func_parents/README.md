func_parents

- Purpose: JSON snapshots of which parent functions (module + fnStart RVA) called each hooked child.
- Source: Generated at runtime by the developer’s local debug builds only.
- Location at runtime: written next to the game as C:\users\<user>\Soldier of Fortune\sof_buddy\func_parents\*.json (Wine paths map accordingly).
- Behavior: the recorder de-duplicates across runs and flushes once on shutdown in debug builds.
- Not shipped: these files are NOT produced or included by release builds.
- Usage: if you want to keep a reference in the repo, copy selected JSONs from the game directory into this folder manually.
