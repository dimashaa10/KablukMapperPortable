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
| 1 | seal check: the shell ring must have no holes | 10 % |
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

## The map is sealed by the exporter, not by the editor

The editor starts with an empty map: nothing is pre-placed around the border
any more, so the whole 32 x 32 grid is yours to paint. What keeps the level
from leaking is the exporter:

* the floor slab (`z -16..0`) and the ceiling slab (`z 128..144`) cover the
  whole map;
* every border cell the user left **empty** becomes part of a wall run
  (`z -16..144`, one brush per run, four runs for an untouched map), so a map
  with a single room in the middle still seals;
* border cells the user painted are left exactly as they are, with the texture
  that was used.

The log says how many runs were added:

```
SEALING WALLS: 4 RUNS AROUND THE BORDER
FLOOR + CEILING SLABS ADDED
```

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
| the same map with no blocks at all | 426 924 B (slabs + walls only) | — |

The lightmap lump is still the big one: ~2000 faces, ~200 lumels each. `light`
sizes that buffer from the faces instead of reserving a fixed maximum (see
`vendor/tyrutils/README.kabluk.md`, step 9), so the memory follows the map
instead of a guess, and no `.lit` space is reserved unless `-lit` is asked for.
The 16 MB ceiling left in `MAX_MAP_VISIBILITY` (vis) is upstream and is the
floor of the chain's peak.

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
shorter on the desktop). `qbsp` is the stage that grows - about 7 MB of peak
for a 28 x 28 map on the host, all of it handed back before the next stage -
while `light` adds a few hundred KB. The editor also drops every preview
texture and picker thumbnail before the chain starts, so what the console's
32 MB are spent on is the tools and the wad.

**If the screen goes dark during `LIGHT`** it is usually not a crash: light is
the slow stage (tens of seconds to minutes on a PSP) and the backlight timer
only sees button presses. Press a button, or turn Backlight Auto-Off off in
the PSP's power save settings. The `LIGHT DONE IN n S` line afterwards says
the stage finished.

## When the console dies mid compile: read the two files

The compile window's text lives in RAM, so a console that powers off loses it.
Both halves of a compile also write to disk, and those two files survive and
say how far it got:

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
ms0:/PSP/GAME/KABLUK/maps/kabluk.bsp is 17648 bytes
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
or in the first `fopen()`; one that stops at `... is N bytes` or
`BSP is version 29` died inside the lump copy; one that stops at
`---- lighting N faces ----` died in the lighting itself.
Every one of those has a message in front of it when the cause is something
the tools can see (a missing file, a full memory stick, a cut-off bsp), so a
log cut off *without* a message is the signature of an exception.

The lump copy is checked now: a `.bsp` whose lumps point past the end of the
file (truncated by a full stick, or read while it was still being written) is
reported as `lump 3 is outside the bsp: offset ..., length ..., ... byte file`
instead of `memcpy()`ing from past the buffer.
