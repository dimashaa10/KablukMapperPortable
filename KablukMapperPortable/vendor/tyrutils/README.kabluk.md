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
   single-threaded path now returns 1. The Win32 `RunThreadsOn()` also runs
   the worker inline when `numthreads <= 1` instead of creating a thread: the
   editor catches a tool's fatal error with `longjmp`, which must not happen
   on a worker thread. `bspchain.c` passes `-threads 1` to `light` and `vis`,
   so the Windows build behaves exactly like the single threaded one.
7. **`qbsp/tyr_qbsp_prefix.h` (new)** — qbsp ships private copies of cmdlib,
   mathlib, file and bspfile. Linked into one binary they collide with
   `common/*` and with `light/entities.c`, so the 19 clashing symbols get a
   `q_` prefix. `qbsp.h` includes this header first and every qbsp translation
   unit includes `qbsp.h`, so the rename is consistent — with one exception
   that GCC 14 and younger refuse to compile: `qbsp/bspfile.c` includes
   `file.h` *before* `qbsp.h`, so `LoadFile` was still the plain name when the
   prototype was read, and the call two lines later expanded to `q_LoadFile`,
   which nothing declared. `file.h` and `bspfile.h` now include the prefix
   header themselves, so their prototypes carry the prefix whatever order they
   are included in. (Older GCC only warned, and the desktop harness hides it
   with `-w`; `-Werror=implicit-function-declaration` proves the fix.)
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
13. **Windows is `_WIN32`, not `WIN32`** — `common/cmdlib.c` guarded `direct.h`,
    `Q_getwd()` and `Q_mkdir()` with `#ifdef WIN32`, which an **x64** MSVC build
    does not define (it sets `_WIN32` only), so `Q_getwd()` compiled down to a
    call to POSIX `getwd()` and the link failed with LNK2019. The guards are
    `#if defined(WIN32) || defined(_WIN32)` now, and the POSIX fallback uses
    `getcwd()` + `unistd.h` instead of the deprecated `getwd()` (which GCC and
    MinGW warn about at link time). `common/threads.c` keeps its `WIN32` test on
    purpose: on x64 the single threaded branch is used, and `light`/`vis` are
    called with `-threads 1` either way.

14. **`AddLump()` padding is fixed** (`common/bspfile.c`). Upstream did
    `SafeWrite(data, (size + 3) & ~3)` and then, when `size % 4`, wrote
    `size % 4` more zero bytes. The first call read up to three bytes past the
    end of the lump buffer (AddressSanitizer reported it on every compile: the
    entities lump is never a multiple of four) and the file got
    `size + 2 * (size % 4)` bytes for the lump. Now exactly `size` bytes are
    written and the file is padded to a 4-byte boundary, which is what the
    offsets assumed all along. Lumps are read through `fileofs`/`filelen`, so
    the `.bsp` is unchanged for every reader.
15. **Allocations fail as a log line.** `light` dereferenced three
    allocations without checking them (`modelinfo` in `light.c`, `tnodes` in
    `trace.c`, the `epair` in `entities.c`) and `shadowmodels` was only
    half-guarded; the first two are written to immediately, so a console that
    ran out of memory died there instead of reporting it. They all `Error()`
    now, which `bspchain.c` turns back into a failed stage.
16. **`IsAbsolutePath()` understands device paths** (`qbsp/cmdlib.c`). It only
    accepted a leading `/` or a `C:`-style drive, so a PSP path
    (`ms0:/PSP/GAME/KABLUK/quake.wad`) counted as *relative*: the wad name from
    the worldspawn key was glued onto `-wadpath` a second time, the wad was
    never opened and every texture vanished from the `.bsp`. A `:` before the
    first separator now makes a path absolute.
17. **`vis/soundpvs.c` bounds-checks the texture index.** A face with
    `miptex == -1` indexed `dataofs[-1]`, which is a read outside the texture
    lump; the resulting pointer was then handed to `strncasecmp`.
18. **`dataofs[] == -1` is skipped in `soundpvs.c`.** The index check above
    still let a *valid* miptex through whose `dataofs` is `-1` (the format's
    "no data here"), so `miptex` became `base - 1` and the ambient-texture
    `strncasecmp` calls read one byte in front of the lump. AddressSanitizer
    reports it as a heap-buffer-underflow on a map compiled without a wad.
19. **`LoadBSPFile()` checks every lump against the file size.** The header's
    `fileofs`/`filelen` were trusted, so a `.bsp` cut short (full memory stick,
    or read while qbsp was still writing it) made `CopyLump()` `memcpy()` from
    past the buffer it had just `malloc`ed. `LoadFile()` now records the size it
    read (`Sys_LoadedFileSize()`), and a lump that does not fit is an `Error()`
    line - `lump 3 is outside the bsp: offset ..., length ..., ... byte file` -
    rather than an exception with nothing in the log.
20. **Negative lengths are errors, not reads.** `Sys_filelength()` returned
    `-1` when `ftell()` failed, which became `malloc(0)` plus a `fread()` of
    `(size_t)-1` bytes; `SafeRead()`/`SafeWrite()` cast their `int` count to
    `size_t` the same way. All three check now, and `LoadFile()` reports an
    empty file by name. `LoadFile()` also logs `<name> is <n> bytes`, which
    makes the size of the file a tool is about to parse visible in the log.
21. **`light` prints a breadcrumb per step**, because the console can power off
    mid stage and the log then ends at the last line it wrote: `N thread(s),
    loading ...`, `---- bsp loaded ----`, `---- bsp2 conversion done ----`,
    `---- entities loaded ----`, `---- bsp tree copied ----`, `---- lighting N
    faces ----`, `---- lightmaps written ----`.
22. **`LoadFile()` reads with `open`/`lseek`/`read`/`close`, not stdio**
    (`common/cmdlib.c`). On a PSP the chain stopped on the first big `.bsp`
    with the log ending between `---- read, closing ----` and
    `BSP is version 29`: the only statements left there were the `fclose()`
    and the two after it, so the death was in the stream close (the `free()`
    of its buffer) or in the statement behind it. A plain handle has no stream
    and no buffer to hand back, the read loops so a short `read()` on the
    memory stick cannot truncate silently, and each call logs a line of its
    own (`---- allocating ----`, `---- allocated, reading ----`, `---- read N
    of M bytes ----`, `---- closed ----`, `---- loaded ----`). Windows keeps
    the same calls through `_open`/`_read`/`_lseek`/`_close`.
23. **The log file is written before the editor hook** (`common/log.c`). The
    editor's hook (which fills the compile window and `compile.log`) used to
    run first, so a death inside it looked exactly like a death inside the
    tool: neither file had the line. Now the tool's own log gets it first, and
    a line that exists in `light.log` but not in `compile.log` says the hook
    was the one that died.
24. **`tyr_free_kb()` is documented as the reserve, not the heap**
    (`include/common/tyr_port.h`, `bspchain.c`). pspdev's `malloc` takes one
    partition block at the first allocation and leaves a fixed 512 KB behind,
    so the number printed next to every load is 512 for the whole session and
    says nothing about the heap. The editor prints the real budget once:
    `user memory before startup: N KB`, read before `SDL_Init()`.
25. **`vis` sizes the visibility buffer from the map** (`vis/vis.c`,
    `LoadPortals()`). Upstream allocates `MAX_MAP_VISIBILITY` (16 MB) and
    `memset`s it for every map, whatever the map needs; on the PSP that is most
    of the heap, asked for by the last stage of the chain. The buffer is now
    `2 * originalvismapsize + 4 * portalleafs_real`, which is the worst case
    `CompressRow()` can produce (it can double its input, plus four bytes of
    slack per leaf), and the old ceiling still caps it. A 144 brush map drops
    from 16 777 216 bytes to 36, and the `.bsp` is byte for byte identical.

The Visual Studio project adds `_CRT_SECURE_NO_WARNINGS` and
`_CRT_NONSTDC_NO_WARNINGS` (`unlink` in `vis/state.c`), and gives the three
basenames that exist twice — `bspfile.c`, `cmdlib.c`, `mathlib.c`, once in
`common/` and once in `qbsp/` — their own `ObjectFileName`, which is what
MSB8027 complains about. Do not rename or delete qbsp's private copies of
`cmdlib`/`mathlib`/`file`/`bspfile`: `qbsp/tyr_qbsp_prefix.h` handles the
symbol clash, the file names have to stay.

Re-syncing with a newer upstream means re-applying these twenty-five steps.
