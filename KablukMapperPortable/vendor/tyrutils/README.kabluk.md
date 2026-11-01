# TyrUtils (vendored)

Upstream: <https://github.com/sezero/tyrutils> — v0.17, snapshot 2020‑07‑26
Original author: Kevin Shanahan (Tyrann), <http://disenchant.net/utils>
License: GNU GPL v2 **or later** (`COPYING`) — compatible with this project's GPL‑3.0.

Kabluk Mapper links the three map tools **into the editor** instead of running
them as separate programs, so the whole chain `.map -> qbsp -> light -> vis`
is one button press and can work later on the PSP, where spawning processes is
not an option.

## What is vendored

| folder | files |
| --- | --- |
| `qbsp/` | brush, bspfile, cmdlib, csg4, file, globals, map, mathlib, merge, outside, parser, portals, qbsp, solidbsp, surfaces, tjunc, util, wad, winding, writebsp |
| `light/` | entities, light, litfile, ltface, trace |
| `vis/` | flow, soundpvs, state, vis |
| `common/` | bspfile, cmdlib, log, mathlib, threads |
| `include/` | the headers those files need |

`bspinfo` and `bsputil` are not used and are not vendored.

Everything is compiled as C with neither `WIN32` nor `USE_PTHREADS` defined,
which selects upstream's single-threaded path in `common/threads.c` — no
pthreads, no Win32 threads, exactly what pspsdk needs.

## Changes against upstream

Mechanical only; no algorithm was touched.

1. **`include/common/tyr_port.h` (new)** — `TYR_PRINTF_ATTR` /
   `TYR_NORETURN_ATTR` / `TYR_NOINLINE` hide the GCC `__attribute__` syntax
   (MSVC, pspsdk), `strcasecmp`/`strncasecmp` map to `_stricmp`/`_strnicmp`
   on MSVC,
   `TYR_QBSP_ENTRY` / `TYR_LIGHT_ENTRY` / `TYR_VIS_ENTRY` give each tool's
   `main()` a private name, and the two editor hooks are declared here.
2. The three `main()` functions are renamed through those macros and get an
   explicit `int` return type (upstream relied on the implicit-int rule).
3. Every `exit(1)` (`common/cmdlib.c`, `qbsp/util.c`, `qbsp/qbsp.c`,
   `light/light.c`, `vis/vis.c`) became `tyr_fatal_exit()`: while the editor is
   driving, `bspchain.c` longjmps back instead of terminating the process, so
   a broken map cannot kill the editor.
4. **Log hook** — `common/log.c` (`logvprint_locked__`) and the `msgScreen`
   branch of `qbsp/util.c` (`Message`) call `tyr_log_hook()`, so all tool
   output lands in the compile log window (once — other `Message` types
   already go through `logprint()`).
5. `unistd.h` / `sys/time.h` includes are guarded for Windows; `I_FloatTime`
   falls back to `GetTickCount()` there.
6. **`common/threads.c`** — upstream only defines `GetDefaultThreads()` for the
   threaded builds, but `light` and `vis` call it unconditionally; the
   single-threaded path now returns 1.
7. **`qbsp/tyr_qbsp_prefix.h` (new)** — qbsp ships private copies of cmdlib,
   mathlib, file and bspfile. Linked into one binary they collide with
   `common/*` and with `light/entities.c`, so the 19 clashing symbols get a
   `q_` prefix. `qbsp.h` includes this header first; all qbsp translation units
   include `qbsp.h`, so the rename is consistent.
8. **Re-runnable tools** — the editor may compile the same map (or another
   one) several times per session, while upstream assumes one run per process:
   * `qbsp/qbsp.c` zeroes `map` and `options` at entry and calls `FreeAllMem()`
     (that function exists upstream but sits behind `#if 0`) plus the same
     reset on exit;
   * `common/bspfile.c` gains `UnloadBSPFile()`, which frees everything
     `LoadBSPFile()` allocated; `light` and `vis` call it before returning.
9. **The lightdata lump is sized from the map** — upstream reserves
   `MAX_MAP_LIGHTING` (16 MB) *and* multiplies it by 4 for the `.lit` variant,
   i.e. 64 MB of RSS per run, impossible on a PSP. Now `light` adds
   `FaceLightmapSamples()` (new, `light/ltface.c`) over every face
   (`EstimateLightDataSize()`, `light/light.c`), allocates exactly that much,
   skips the 3 Bytes-per-lumel `.lit` area unless `-lit` is given (the lit
   writes in `WriteLightmaps()` are guarded), and uses
   `KMP_MAX_MAP_LIGHTING` (16 MB, `include/common/bspfile.h`) only as a sanity
   ceiling. One light style per face is assumed, which is what a map without
   styled light entities has.
10. **MSVC cannot parse GNU C** — the four places that still used it are gone:
   * `qbsp/qbsp.h` — `Error()` uses `TYR_PRINTF_ATTR`/`TYR_NORETURN_ATTR`
     instead of a raw `__attribute__((format,noreturn))`;
   * `qbsp/qbsp.h` — `min()`/`max()` keep the statement-expression `typeof`
     version for GCC and fall back to plain macros for MSVC;
   * `qbsp/cmdlib.c` — `WIN32_LEAN_AND_MEAN` + `NOMINMAX` before `windows.h`,
     otherwise the SDK's `min`/`max` collide with qbsp's;
   * `light/ltface.c` (4x) and `qbsp/outside.c` — `TYR_NOINLINE` instead of a
     bare `__attribute__((noinline))`.
11. **`ffsl` without GCC** (`include/vis/leafbits.h`) — MSVC has no
    `__builtin_ffsl`; a five line `_BitScanForward` wrapper fills in.
12. **Small fixes found while porting** —
    `common/bspfile.c` (`CopyLump`) initialises its `lumpspec` pointer (MSVC's
    C4703 is an error under `/sdl`), `common/cmdlib.c` keeps `strupr`/
    `strlower` out of the MSVC build (the CRT already declares them, C4273),
    and `vis/vis.c` (`LeafFlow`) sizes the RLE buffer of `CompressRow()` from
    the byte count instead of `portalleafs * 2 / 8`, which was one byte short
    for leaf counts like 12.

The Visual Studio project adds `_CRT_SECURE_NO_WARNINGS` and
`_CRT_NONSTDC_NO_WARNINGS` (`unlink` in `vis/state.c`), and gives the three
basenames that exist twice — `bspfile.c`, `cmdlib.c`, `mathlib.c`, once in
`common/` and once in `qbsp/` — their own `ObjectFileName`, which is what
MSB8027 complains about. Do not rename or delete qbsp's private copies of
`cmdlib`/`mathlib`/`file`/`bspfile`: `qbsp/tyr_qbsp_prefix.h` handles the
symbol clash, the file names have to stay.

Re-syncing with a newer upstream means re-applying these twelve steps.
