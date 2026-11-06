# hash_lookup

Turns the engine's cvar, command and alias dictionaries into hash maps.

**On by default.** Read once at `PostCvarInit`. `+set _sofbuddy_hashmap 0` to
skip. The runtime feature switch (`_sofbuddy_feature_hash_lookup`) also gates
it; changing either takes effect on the next launch because the patches are
installed once, before play.

## Cvars

**Settings** (you set these):

| cvar | default | what it does |
|---|---|---|
| `_sofbuddy_hashmap` | 1 | Load-time. `+set 0` to skip. No gauges. |

## Why

SoF inherits Quake 2's dictionaries-as-linked-lists. Every cvar read, every
cvar write and every console command walks a list and `strcmp`s each node:

| function | scans |
|---|---|
| `Cvar_Get`, `Cvar_Set2`, `Cvar_FullSet`, `Cvar_VariableValue`, `Cvar_VariableString` | all cvars |
| `Cmd_ExecuteString` | all commands, then all aliases, then (via `Cvar_Command`) all cvars |

That is fine for a client with a couple of hundred cvars and a human at the
keyboard. It is not fine for a client running SoF Plus scripting, where the
script engine is the one typing: each script variable is a cvar, each script
line goes through `Cmd_ExecuteString`, and a command that is neither a command
nor an alias falls all the way through to a full cvar scan. The lists grow with
the script set, so the per-lookup cost grows with it.

This module is a port of `sof_buddy-server`'s `hash_lookup` feature. The
engine functions and list layout are shared between the server images and the
retail client `SoF.exe` (ImageBase `0x20000000`). Every RVA and expected byte
string below was checked against the client image before the feature was
enabled by default.

## How: patch the search, keep the function

There is no callable `Cvar_FindVar` in `SoF.exe` - it is **inlined** into each
of its callers. Rather than re-implement those functions (`Cvar_Set2` alone is
~1KB of latch / noset / info-validation / callback logic, and silent drift there
would be worse than the CPU it saves), this feature replaces only the *search*.

Every one of the scans has the same shape: a loop that walks a list and leaves
through one of two labels, with the matched node in a fixed register. The loop
head is overwritten with a `jmp` to a small assembly stub that does the O(1)
lookup and enters **that same function's own label** with **that same
register** set. Everything before and after the search is untouched engine
code, byte for byte.

```
Cvar_Get @ 0x20021AE0
  0x20021B54  mov ebp, cvar_vars   <-- overwritten with jmp hl_stub_cvarget
  0x20021B5E  ...scan loop...          (now unreachable)
  0x20021BE0  found:     expects ebp = cvar_t*
  0x20021BC4  not found:
```

### Patch sites

| function | patch RVA | len | key at | found (reg) | not found |
|---|---|---|---|---|---|
| `Cvar_VariableValue` | `0x216DE` | 6 | `[esp+0x14]` | `0x2174D` (edx) | `0x21742` |
| `Cvar_VariableString` | `0x2176E` | 6 | `[esp+0x14]` | `0x217DC` (edx) | `0x217D2` |
| `Cvar_Get` | `0x21B54` | 6 | `[esp+0x98]` | `0x21BE0` (ebp) | `0x21BC4` |
| `Cvar_Set2` | `0x21D87` | 7 | `[esp+0x98]` | `0x21E11` (ebp) | `0x21DF1` |
| `Cvar_FullSet` | `0x221C7` | 7 | `[esp+0x98]` | `0x22253` (ebp) | `0x22231` |
| `Cvar_Command` | `0x224CF` | 5 | `[esp+0x10]` | `0x2253C` (ebp) | `0x22532` |
| `Cmd_ExecuteString` | `0x1950C` | 6 | `cmd_argv[0]` | `0x19531` cmd / `0x1959E` alias (esi) | `0x1958C` |
| `Cmd_RemoveCommand` | `0x191E8` | 5 | — | resumes at `0x191ED` | — |

Register contract at every stub entry: `esp` is exactly what the engine had
(the surrounding function still reads its arguments through `[esp+N]`), and
`ebx`/`esi`/`edi`/`ebp` hold whatever the engine put there — the cdecl call
preserves them, and each stub sets only the one register its re-entry label
expects. `eax`/`ecx`/`edx` are scratch: the loop being replaced clobbered them
too, and every re-entry label writes them before reading. The one exception is
`Cmd_RemoveCommand`, whose `edx` still holds the name its not-found path
prints, so that stub saves it across the call.

Bytes past the 5-byte `jmp` are padded `0x90`. They are dead either way — the
only entry into those loops was the head that is now a jump.

### Safety

`Install()` verifies the original bytes at all eight sites **before touching
any of them**; one mismatch and nothing is patched at all. So an unrecognised
engine build, or another mod that got to the same bytes first, degrades to
"feature off" rather than to a wild jump. The expected bytes were verified
against two shipped server engines (sofplus 2012 `SoF-spsv.exe` and sofplus
2019 `sof-spsv16.exe`) and against the retail client `SoF.exe`.

`HashLookup_Shutdown()` runs from `DllMain`'s detach path and puts the original
bytes back. This is **mandatory**, not hygiene: SoF unloads/reloads modules
between games, and the jumps point into this image.

## Keeping the index in sync with the list

### The list stays the source of truth

The table does not replace the engine's linked list — it is a **derived index
over it**. Nothing here relinks, reorders, copies or frees anything the engine
owns. The table holds `name -> node`, borrowing the engine's own pointers, and
the list is left exactly as the engine built it.

That is deliberate, because plenty of code still walks the list directly and
must keep working untouched: `Cvar_GetLatchedVars`, `writeconfig`, `cvarlist_f`,
`Cvar_Info`, `SG_WriteLatched` — and SoF Plus itself, which walks `cvar_vars`
directly.

### The anchor

Each index remembers two pointers. `last_head` is the raw list head as of the
last sync, and is purely the fast-path compare. `anchor` is the first node
actually *indexed*, and is where the walk stops.

They are different on purpose. `anchor` must be a node nothing can free,
unlink or reorder — see "SoF Plus temporaries" below, where the real head very
often is not such a node. Because nodes are only ever added at the head,
everything from the current head down to `anchor` is exactly the set added
since, and everything from `anchor` onward is already in the table.

```
                       anchor  (head as of the last sync)
                          |
  head ->  N7 -> N6 -> [ N5 -> N4 -> N3 -> ... -> N0 -> NULL ]
           \___________/   \_________________________________/
            added since        already indexed; never revisited
            the last sync
```

So every lookup begins with:

```c
void* h = *head;
if (h == last_head)
    return;              // steady state: one load, one compare, done
// else: walk h .. anchor, indexing each eligible node;
//       last_head = h, anchor = first node indexed on this walk
```

- **Nothing changed** — one load and one compare, then straight to the hash
  lookup. This is the case essentially every time.
- **Nodes were added** — walk only the new prefix. Each node is inserted
  exactly once over the life of the process, so the amortised cost of keeping
  the index current is one insert per cvar/command/alias ever created.

This is why nothing needs to hook the creation paths. A cvar created by
`Cvar_Get`, by `Cvar_Set2` falling through to it, by the engine at startup, or
by SoF Plus, is discovered identically: the next lookup notices the head moved
and picks it up. The index never has to be told.

The sync runs *before* the table is consulted, which is what makes a miss
trustworthy: "not in the table" means "not in the list", so the stub can safely
take the engine's not-found branch.

### What the anchor compare can and cannot see

| what happens | head moves? | anchor compare notices? | consequence |
|---|---|---|---|
| any lookup / read | no | n/a | **cannot** stale the table |
| node prepended | yes | **yes** — self-healing | new prefix walked in |
| node relinked / list reordered | no | **no** | table would keep serving a stale position |
| node unlinked mid-list | no | **no** | table would serve a node no longer in the list |
| node freed | no | **no** | table would serve a **dangling pointer** |
| `name` freed or repointed | no | **no** | borrowed key dangles; lookups mismatch |

The mechanism is self-healing for exactly **one** kind of mutation: a prepend.
Everything below that line is invisible to it — the head pointer is unchanged,
so the fast path returns immediately and the table goes on answering with a
node that has moved, left the list, or been freed.

### SoF Plus temporaries: why the engine audit was not enough

The audit covers **SoF.exe**. That is not the whole program. SoF Plus caches
`&cvar_vars` and a pointer to the engine's `Z_Free`, and it uses both to
implement "temporary" cvars — the `~`-prefixed ones — by destroying them itself:

| SoF Plus | what it does |
|---|---|
| `0x10005750` | walks `cvar_vars`; for every node with `name[0] == '~'`, `Z_Free`s the **name**, `string`, `latched_string` and the **node**, unlinking it mid-list (`0x100057C3`) |
| `0x10005800` | unlinks every `~` node into a saved scope list, without freeing |
| `0x10005890` | splices a saved scope list back on and rewrites the head |

That is an unlink, a free, and a relink — all three of the rows a head-pointer
comparison cannot see — and the freed `name` is the exact string the table
borrows as its key. Indexing those nodes is a use-after-free.

**So `~`-prefixed cvars are never indexed.** They are skipped during sync and
routed to the exact linear walk on lookup, which is cheap because SoF Plus keeps
temporaries at the front of the list. Everything else — the several hundred
permanent engine and SoF Plus cvars, plus all commands and aliases — is indexed
as before, and `anchor` is pinned to the first *permanent* node so that freeing
a head temporary can never dangle it or force a full re-index.

**Limitation, stated plainly:** this rests on SoF Plus's `~` naming contract.
A different mod that removes or reorders cvars *not* prefixed with `~` would
reintroduce exactly this class of bug, and nothing here would detect it.

### Removal: the one case that needs telling

`Cmd_RemoveCommand` unlinks mid-list and `Z_Free`s the node, and the anchor
compare cannot see either. Its stub therefore **drops the whole command index**
before the engine frees anything; the next lookup finds `anchor == NULL` and
rebuilds by walking the list once.

A wholesale rebuild rather than a surgical erase is deliberate: it cannot leave
a freed node reachable no matter which node went, or how many, or whether the
name was ambiguous in case. Removals are rare — a rebuild costs one walk of a
few hundred nodes, against the certainty of never calling through a dangling
function pointer.

### Matching the engine's comparisons exactly

- **Cvars** compare with `strcmp` — case-sensitive.
- **Commands and aliases** are matched by `Cmd_ExecuteString` with `stricmp`,
  so both those indexes fold case. Note `Cmd_AddCommand`'s duplicate check is
  case-*sensitive*, which is why that scan is deliberately left unpatched: an
  accelerated case-insensitive check there would start rejecting `Foo` when
  `foo` exists. It only runs on registration, and its `Cvar_VariableString`
  call is accelerated anyway.
- Because a case-differing duplicate *can* therefore exist, the sync walks
  newest-first and never lets an older node overwrite a newer one under the
  same folded key — the engine's scan starts at the head, so the newest wins
  there too.
- Keys containing bytes `>= 0x80` take a linear walk instead of the table:
  our ASCII folding matches msvcrt's `stricmp` in the C locale, but not
  necessarily beyond it, and a wrong *miss* on `Cmd_ExecuteString` would
  silently drop a command.

### The `matrix` quirk

Every inlined copy of the cvar search carries the same oddity: after the name
compare fails, it *also* matches when the requested name is exactly `"matrix"`
and the node's name is exactly `"timescale"`.

```asm
0x20021BA7  mov edi, offset "matrix"     ; vs the requested name, 7 bytes
0x20021BA9  jnz  next
0x20021BAD  mov edi, offset "timescale"  ; vs this node's name, 10 bytes
0x20021BBB  jz   found
```

So `Cvar_FindVar("matrix")` returns whichever of the `matrix` and `timescale`
cvars sits nearer the head. It is a single-name special case, so the hash path
is bypassed for that one name and it takes the exact linear walk.

## Performance

The engine's scan was replicated instruction-faithfully and timed against this
feature's index over the same list (host-native 32-bit build, Intel i5-3350P @
3.1 GHz). These measurements come from the server port; the client uses the
same functions and list layout.

| cvars in list | engine scan, average hit | engine scan, miss | hash index | speedup (hit) |
|---:|---:|---:|---:|---:|
| 200 | 2.5 µs | ~5 µs | 65 ns | 38× |
| 400 | 4.9 µs | ~10 µs | 65 ns | 75× |
| 700 | 8.8 µs | 17.0 µs | 71 ns | 124× |
| 1200 | 15.2 µs | 29.4 µs | 78 ns | 196× |
| 2000 | 25.0 µs | ~50 µs | 71 ns | 351× |

Break-even is around **three nodes**. Any list longer than a handful wins.

The worst case is a script line that is neither a command nor an alias: it
scans all commands, then all aliases, then every cvar via `Cvar_Command`. At
~300 commands and 1000 cvars that is ~1300 nodes for a single console line.
On a live server with ~300 commands / ~700+ cvars the same patch measured
3–6× faster whole console lines; the isolated lookup was 38–350× faster.

## Testing

`index.cpp` is portable and was exercised host-side in the server project,
built 32-bit so pointer size and node offsets match, against a synthetic engine
image built with the same prepend discipline — bulk insert, incremental sync
across table growth, the `matrix` quirk with and without a real `matrix` cvar,
case-insensitive command/alias matching, command-shadows-alias ordering,
case-differing duplicates, non-ASCII fallback, and stale-entry-after-removal.

The stubs' machine code and every indirect jump target were checked in the
linked object. The client patch bytes are verified at runtime on every install;
a mismatch logs a `[hash_lookup]` `PrintOut(PRINT_BAD, ...)` line and leaves the
engine fully unpatched.

To disable without rebuilding, use the runtime feature switch or set
`_sofbuddy_hashmap 0` in the config/command line and restart SoF.
