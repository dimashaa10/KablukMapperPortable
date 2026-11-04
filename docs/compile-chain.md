# Compile chain: .map -> qbsp -> light -> vis

`START` (F2 on the desktop) opens the compile window. `COMPILE MAP` runs the
whole chain in the editor process; the log window shows every step.

```
kabluk.map  --qbsp-->  kabluk.bsp  --light-->  kabluk.bsp  --vis-->  kabluk.bsp
            (BSP)                   (lightmaps)            (PVS)
```

Files written next to the `.map` in the output folder:

| file | produced by | meaning |
| --- | --- | --- |
| `kabluk.map` | the editor | the map itself, plain text |
| `kabluk.bsp` | qbsp, then light and vis rewrite it | the level Quake loads |
| `kabluk.prt` | qbsp | portal file, vis reads it |
| `kabluk.log` | qbsp | qbsp's own log |

## How it is wired

`compile.c` runs the exporter in small chunks (one chunk per frame), then hands
over to `bspchain.c`, which calls one tool per frame:

| step | what happens | progress |
| --- | --- | --- |
| 0 | count blocks and entities | 5 % |
| 1 | say what the exporter adds and what it does not | 10 % |
| 2 | floor and ceiling slabs | 15 % |
| 3 | open the `.map`, write worldspawn (name + minlight) and both slabs | 20 % |
| 4 | merge solid cells into brushes, write them (64 per frame) | 20–80 % |
| 5 | point entities, each with the keys set in the property panel | 80 % |
| 6 | close the file, hand it to the chain | 82 % |
| 7 | run `QBSP`, `LIGHT`, `VIS` — one tool per frame | 82–99 % |
| 8 | report `BSP READY: <bytes>` | 100 % |

The status line names the tool that is running (`QBSP 82%`, `LIGHT 87%`,
`VIS 92%`), and every line the tools print lands in the log.

If a tool reports a fatal error, `bspchain.c` catches it (the tools'
`exit(1)` became `longjmp`) and the job ends as `FAILED` — the editor keeps
running, the log tells you what went wrong.

## Why in-process and not "run bsp.exe"

Spawning a process is easy on Windows and impossible on the PSP, and the PSP
is the whole point of this project. The tools are plain C with a `main()`, so
they are linked straight into the editor and entered through
`tyr_qbsp_main()` / `tyr_light_main()` / `tyr_vis_main()`. See
`vendor/tyrutils/README.kabluk.md` for the small patch set that makes that
possible.

## What the exporter adds, and what it leaves alone

The editor starts with an empty map and the exporter adds exactly three things
on its own:

* the floor slab (`z -16..0`) and the ceiling slab (`z 128..144`), both
  covering the whole map, with the floor / ceiling textures from the settings;
* nothing else.

It used to wall off every **empty** border cell too (`z -16..144`, one brush
per run), so any map sealed itself. That put geometry the user never painted
into the level, so the walls are gone: the border is yours. A map is as closed
as the ring you paint around it, and until that ring exists the level leaks.

The log says so on every compile:

```
FLOOR + CEILING SLABS ADDED
NO BORDER WALLS - THE MAP SEALS ITSELF
```

### A leak is not the end of the compile

`qbsp` floods the map from the outside to decide which leaf is solid. If the
flood reaches an entity, the map leaked: qbsp prints the warning, writes
`<name>.pts` (the leak line, for whoever wants to look) and **deletes the
`.prt` file**. `vis` cannot run without the `.prt`, and failing the whole
compile over a missing PVS would throw away a `.bsp` that plays fine - Quake
treats a level without visdata as "everything is visible", which costs frame
rate and nothing else. So the chain skips vis and says why:

```
THE MAP LEAKED - NO .PRT, VIS SKIPPED
BSP READY: 17656 BYTES
```

Paint the ring (or a proper wall) and the next compile runs vis as usual.

## Numbers on a desktop (max size grid map, 4096 x 4096 units, 145 brushes)

The map is 32 x 32 cells of 128 units, so world coordinates run from **-2048**
to **+2048** on both axes; the `.map` the editor writes has exactly that extent
(x/y -2048..2048, z -16..144).

| stage | peak RSS | time |
| --- | --- | --- |
| qbsp | 2.3 MB | 0.05 s |
| light | 4.6 MB (with its lightmap lump) | 0.05 s |
| vis | 5.4 MB | 0.02 s |
| whole chain inside the editor | 22 MB | ~0.2 s |
| the `.bsp` it writes | 443 458 B, of which 313 257 B lightmaps | — |
| the same map with no blocks at all | 426 924 B (the two slabs) | — |

The lightmap lump is still the big one: ~2000 faces, ~200 lumels each. `light`
sizes that buffer from the faces instead of reserving a fixed maximum (see
`vendor/tyrutils/README.kabluk.md`, step 9), so the memory follows the map
instead of a guess, and no `.lit` space is reserved unless `-lit` is asked for.
That 16 MB used to be the floor of the chain's peak, because `vis` asked for
the whole `MAX_MAP_VISIBILITY` ceiling up front and `memset` it before it knew
what the map needed - on a PSP that one `malloc` is most of the heap, taken by
the last stage of the chain. It is sized from the map now (twice the
uncompressed visibility total plus four bytes per leaf, which is what
`CompressRow()` can at worst produce), and the old ceiling still applies to a
map that really needs it. For the map above the buffer went from 16 777 216
bytes to a few kilobytes; the `.bsp` is byte for byte the same:

```
---- vismap: 36 bytes, ceiling 16777216 ----
```

Memory still grows with every compile in the same session (the tools keep
their pools, upstream assumes one run per process): about 2 MB per extra
compile of a map this size. For the PSP the remaining work is: free the tools'
pools between runs more aggressively, and run the chain in a worker thread so
the editor keeps drawing while vis runs.

## Textures actually reach the `.bsp`

`qbsp` resolves the worldspawn `wad` key relative to the directory of the
`.map`, but the editor keeps the wad where the user picked it. The chain
therefore calls `qbsp -wadpath <folder of the wad>`, which is what puts the
miptexes into the level — before that every texture came back as
`Texture ... not found` and the `.bsp` had none.

## A note on lighting

There is no light entity in the map. `worldspawn` carries the brightness
instead:

```
"light" "200"
```

`light` reads that key as the map wide **minlight**: every surface gets at
least that much, so the level is visible everywhere without a single lamp.
The log says so:

```
WORLDSPAWN LIGHT: 200 (MINLIGHT)
```

and the tool confirms it:

```
using minlight value 200 from worldspawn.
1 entities read, 0 are lights.
```

The value lives in `compile.h` (`DEFAULT_MINLIGHT`) as a seed: the editor puts
it into worldspawn's `light` row when the map is new, and from then on the row
is yours to edit (`docs/properties.md`). 200 is the usual "bright but still
shaded" Quake minlight. Add your own `light` entities from the FGD
when you want pools of real light on top of that floor — the exporter counts
them (`LIGHT ENTITIES: n`) but never invents one.

## What the tools get wrong, and what this port does about it

The things in the vendored tools that turned up while chasing the report "the
console switches off during `light`". They are listed in
`vendor/tyrutils/README.kabluk.md` (changes 14 - 20); the short version:

* **`AddLump()` read past the lump buffer.** The BSP writer asked for
  `(size + 3) & ~3` bytes from a buffer that is only `size` bytes long, and
  then padded again. It happens on every compile because the entities lump is
  never a multiple of four. Fixed: exactly `size` bytes, then padding.
* **`light` dereferenced unchecked allocations.** `modelinfo` and `tnodes` are
  `malloc`ed and written to on the next line, so a console that ran out of
  memory died in the middle of the light stage with nothing in the log. They
  now `Error()`, and `bspchain.c` turns that into a failed stage with the line
  in the compile log.
* **`qbsp` did not understand `ms0:/`.** `IsAbsolutePath()` accepted `/` and
  `C:` only, so the PSP wad path counted as relative and was glued onto
  `-wadpath` twice: the wad was never opened and every texture disappeared
  from the `.bsp` (the picker still showed them, because the editor reads the
  wad itself). A `:` before the first separator is absolute now, and the log
  says `Opened WAD: ms0:/...` when it worked.
* **`vis` indexed the texture lump with -1** for faces that have no texture -
  and so did the `dataofs[]` lookup: `-1` means "this miptex has no data", so
  `base - 1` was read by the `strncasecmp()` sound-ambient check. AddressSanitizer
  called it a one-byte heap underflow on a map without a wad; it is skipped now.
* **A cut-off `.bsp` was copied lump by lump unchecked.** `LoadBSPFile()` trusts
  `fileofs`/`filelen` from the header, so a bsp that a full memory stick cut
  short made `CopyLump()` `memcpy()` from past the buffer it had just
  `malloc`ed - on the PSP an exception, not a message. Every lump is now
  checked against the size of the file `LoadFile()` actually read, and
  `Sys_filelength()`/`SafeRead()` refuse a negative length instead of turning
  it into a four-gigabyte read.

## Sizing the stage: what the log prints

Every stage now logs the free heap in front of it and how long it took:

```
STAGE LIGHT - FREE 18342 KB
  LIGHT DONE IN 74 S
```

`FREE` comes from `sceKernelMaxFreeMemSize()` (PSP only; the line is simply
shorter on the desktop) and, as the section above explains, it sits at 512 KB
for the whole session - it is the reserve pspdev leaves outside the malloc
heap, not the room a stage has. `qbsp` is the stage that grows - about 7 MB of
peak for a 28 x 28 map on the host, all of it handed back before the next
stage - while `light` adds a few hundred KB. The editor also drops every
preview texture and picker thumbnail before the chain starts, so what the
console's 32 MB are spent on is the tools and the wad.

**If the screen goes dark during `LIGHT`** it is usually not a crash: light is
the slow stage (tens of seconds to minutes on a PSP) and the backlight timer
only sees button presses. Press a button, or turn Backlight Auto-Off off in
the PSP's power save settings. The `LIGHT DONE IN n S` line afterwards says
the stage finished.

## When the console dies mid compile: read the files

The compile window's text lives in RAM, so a console that powers off loses it.
Three files on the memory stick survive it and say how far it got:

* **`<out dir>/compile.log`** - a copy of the window's log, one line per
  `compile_log_add()` call, flushed every line. The editor's own view of the
  chain: which stage started, which tool line was last, the free heap in
  front of each stage, and `COMPILE ABORTED` plus the reason if a tool returned
  an error. The window shows its path as `LOG: <path>` in the second line.
* **`light.log`** - light's own log, next to the `.bsp` (the working directory
  of the app). light prints a breadcrumb around every step it takes, so the
  last line names the call it was in when the console died:

```
---- light / TyrUtils v0.17-kabluk ----
3 option(s), 38 byte path
---- timer started ----
1 thread(s), loading ms0:/PSP/GAME/KABLUK/maps/kabluk.bsp
ms0:/PSP/GAME/KABLUK/maps/kabluk.bsp is 17648 bytes, 512 KB free
---- allocating 17649 bytes ----
---- allocated, reading ----
---- read 17648 of 17648 bytes (512 KB free) ----
---- closed ----
---- loaded ----
BSP is version 29
---- bsp loaded ----
---- bsp2 conversion done ----
---- entities loaded ----
---- bsp tree copied ----
---- lighting 70 faces ----
0....1....2....3....4....5....6....7....8....9..\\

Lighting Completed.

lightdatasize: 12149
---- lightmaps written ----
0 switchable light styles
Writing BSP version 29
  0.0 seconds elapsed
```

A log that stops at `---- timer started ----` died in `strcpy()`/`StripExtension()`
or in the first `open()`; one that stops at `... is N bytes` died before the
header was read, and the six lines after it name the call: `---- allocating`,
`---- allocated, reading ----`, `---- read N of M bytes ----`, `---- closed`,
`---- loaded`, then `BSP is version 29`. One that stops at `BSP is version 29`
died inside the lump copy; one that stops at `---- lighting N faces ----` died
in the lighting itself.

Careful with the last one of those: a log always ends at the last line that was
*fully written*, so "stops after X" means "died in X or in the call after X".
The phase file below is what separates the two. Every one of those has a message in front of it when
the cause is something the tools can see (a missing file, a full memory stick,
a cut-off bsp), so a log cut off *without* a message is the signature of an
exception or of a kill.

### The read is not stdio any more

`LoadFile()` reads with `open` / `lseek` / `read` / `close` instead of
`fopen` / `fread` / `fclose`. The reason is in `light-3.log` (a PSP run): the
log ended between `---- read, closing ----` and `BSP is version 29`, which on
that path leaves only the `fclose()` and the two statements after it - nothing
that can fail with a message, and everything that a `free()` inside the
stream close could take down. A plain handle has no stream to set up, no
buffer to give back, and each call leaves its own line behind, so the same
read cannot stop in silence; if it stops elsewhere the log now says which.

The read also loops: one `read()` on the memory stick can come back short, and
the line reports `N of M bytes` so a half read is visible instead of being
noticed only by the `fread()` count check.

### `N KB free` is the reserve, not the budget

`sceKernelMaxFreeMemSize()` reads 512 KB on the PSP and stays there for the
whole session. That is not a shortage: pspdev's `malloc` asks for **one**
partition block, sized from what happens to be free the first time anything
allocates and leaving a fixed 512 KB threshold behind, so after startup the
partition has 512 KB left over no matter how much of the heap is in use. The
number is still worth looking at - if it reads like tens of megabytes the heap
block was never created and every `malloc()` fails - but the budget the
editor and the tools share is printed once, by the editor, into its own log:

```
INFO: partition free at startup: 512 KB
```

Read it as a warning light, not as the budget: 512 there is normal - newlib's
own startup allocates before `main`, so the heap block already exists and what
is left over is the reserve. The number is only interesting when it reads like
the whole partition (tens of megabytes): that means the heap block could not be
created and every `malloc()` is going to fail. A user EBOOT cannot query the
size of the block it got.

Both logs are written per line, and the tool's own log is written **before**
the editor's hook (see `logvprint_locked__()` in `common/log.c`). So if the two
disagree - `light.log` has a line that `<out dir>/compile.log` does not - the
console died inside the editor's log hook, not inside the tool.

### A third file: `kabluk-phase.txt` gets the tools' steps too

Both of those files are written *by the logging*, so a console that dies inside
a log line leaves them looking exactly like a console that died just after one.
The phase file does not have that problem: every line is a separate open,
append, flush and close, so it survives whatever the logging is doing.

The tools therefore have a breadcrumb channel of their own
(`tyr_phase_hook`, pointed at `crash_step()`), and it names each call they
cannot reason about:

```
compile step 7
load: open
load: alloc
load: read
load: read ok
load: close
load: closed
load: closed logged
load: terminated
load: done
bsp: loaded
bsp: version logged
light: light world
light: lightmaps done
light: write bsp
light: done
```

Read the last line and the log together:

| phase ends at | the log has | what died |
| --- | --- | --- |
| `load: read` | `---- allocated, reading ----` | the `read()` itself |
| `load: read ok` | `---- read N of M bytes ----` | the log line for the read |
| `load: close` | the same | the `close()` of the file |
| `load: closed` | the same | the log line after the close |
| `bsp: loaded` | `---- read ... ----` | the log line, again |
| `bsp: version logged` | `BSP is version 29` | the lump copy |
| `light: light world` | `---- lighting N faces ----` | the lighting |

That also settles the question the two logs cannot answer on their own: a
console that stops at the same spot in both files may have died *after* the
last line, in the call the next line would have named.

### No console, no stdout

A PSP EBOOT has nothing attached to `stdout` - it is a TTY handle that nobody
drains. `logvprint_locked__()` no longer writes to it at all on the PSP
(`#ifndef __PSP__`): the write costs a second formatting pass and a few
kilobytes of stack, and a TTY that stops being drained blocks its writer, which
mid compile is indistinguishable from the crash being chased. The tool's own
log and the editor's hook already carry every line; the desktop and the Windows
build keep `printf` output as before.

The lump copy is checked now: a `.bsp` whose lumps point past the end of the
file (truncated by a full stick, or read while it was still being written) is
reported as `lump 3 is outside the bsp: offset ..., length ..., ... byte file`
instead of `memcpy()`ing from past the buffer.
