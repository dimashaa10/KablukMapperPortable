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
| 3 | open the `.map`, write worldspawn and both slabs | 20 % |
| 4 | merge solid cells into brushes, write them (64 per frame) | 20–80 % |
| 5 | point entities; worldspawn gets its `light` key (default 200) | 80 % |
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

## Numbers on a desktop (max size grid map, 8192 x 8192 units, 145 brushes)

| stage | peak RSS | time |
| --- | --- | --- |
| qbsp | 6.6 MB | 0.15 s |
| light | 9.0 MB (with its lightmap lump) | 0.15 s |
| vis | 11.0 MB | 0.05 s |
| whole chain, first compile | 27 MB | ~0.4 s |
| the `.bsp` it writes | 1.6 MB, of which 1.2 MB lightmaps | — |

The lightmap lump is the big one now: 6074 faces, ~200 lumels each. `light`
sizes that buffer from the faces instead of reserving a fixed maximum (see
`vendor/tyrutils/README.kabluk.md`, step 9), so the memory follows the map
instead of a guess, and no `.lit` space is reserved unless `-lit` is asked for.

Memory still grows with every compile in the same session (the tools keep
their pools, upstream assumes one run per process). For the PSP the remaining
work is: free the tools' pools between runs more aggressively, and run the
chain in a worker thread so the editor keeps drawing while vis runs.

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

The value lives in `compile.h` (`DEFAULT_MINLIGHT`); 200 is the usual "bright
but still shaded" Quake minlight. Add your own `light` entities from the FGD
when you want pools of real light on top of that floor — the exporter counts
them (`LIGHT ENTITIES: n`) but never invents one.
