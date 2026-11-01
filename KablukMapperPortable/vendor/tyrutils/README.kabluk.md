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
   `TYR_NORETURN_ATTR` hide the GCC `__attribute__` syntax (MSVC, pspsdk),
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
9. **`MAX_MAP_LIGHTING` 16 MB -> 1 MB** (`include/common/bspfile.h`, override
   with `-DKMP_MAX_MAP_LIGHTING=...`). `light` allocates four bytes per
   lightdata byte, i.e. 64 MB per run upstream — impossible on a PSP. Our grid
   maps need a few hundred KB; a map that exceeds 1 MB of lightdata now fails
   with a clear message instead of eating the machine.

Re-syncing with a newer upstream means re-applying these nine steps.
