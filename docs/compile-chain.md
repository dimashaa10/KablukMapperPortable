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
| 5 | point entities, plus a default `light` if the map has none | 80 % |
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
| qbsp | 7.6 MB | 0.15 s |
| light | +4 MB | 0.05 s |
| vis | +12 MB | 0.05 s |
| whole chain, first compile | 24 MB | ~0.3 s |
| each further compile in the same session | +2.6 MB | — |

Memory comes from upstream's design (whole level in RAM, no freeing) and from
`MAX_MAP_LIGHTING` (already reduced 16 MB -> 1 MB here). For the PSP the
remaining work is: free the tools' pools between runs more aggressively, and
run the chain in a worker thread so the editor keeps drawing while vis runs.

## A note on lighting

Quake needs at least one light or the level is pitch black, so if the map has
no `light` entity the exporter adds one in the middle of the map
(`origin 4096 4096 64`, default brightness). The log says so:

```
NO LIGHT ENTITY: ADDED A DEFAULT LIGHT
```

State the brightness with the FGD property once you place your own light.
