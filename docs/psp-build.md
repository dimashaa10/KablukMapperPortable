# Building the editor for the PSP

The editor is a desktop program that happens to fit the PSP exactly: the
window is 480 x 272, the interface is drawn with rectangles and a 5 x 7 bitmap
font, nothing uses threads, and the only library it needs is SDL2 - which
`pspdev-win-15.2.0-v2` already ships for the handheld.

This page is the whole build, from `C:\pspdev` to a folder you can copy onto a
Memory Stick.

## What you need

* `pspdev-win-15.2.0-v2.zip` extracted to **`C:\pspdev`**, so that
  `C:\pspdev\bin\psp-gcc.exe` is there.
* SDL2 inside that pspdev (it is bundled). Two files have to exist:

```sh
ls /c/pspdev/psp/include/SDL2/SDL.h
ls /c/pspdev/psp/lib/libSDL2.a
```

If they do not, install it: the same release publishes the libraries on their
own, as an overlay for exactly this case -

```powershell
Invoke-WebRequest `
  -Uri https://github.com/dmang-dev/pspdev-win/releases/download/v2/pspdev-win-libraries-15.2.0-v2.zip `
  -OutFile $env:TEMP\psp-libs.zip
Expand-Archive $env:TEMP\psp-libs.zip -DestinationPath C:\pspdev -Force
```

- and after it `C:\pspdev\psp\include\SDL2\SDL.h` and
`C:\pspdev\psp\lib\libSDL2.a` exist. `psp-pacman -S sdl2` does the same
thing, but the bundled `pacman.exe` is an MSYS2 binary built against one
specific `msys-archive-13.dll`, and a newer MSYS2 on the same machine will not
load it (see the troubleshooting table).

If `make` complains instead - "SDL.h: No such file or directory" - ask it what
it found:

```sh
make sdk-info
```

It prints `PSPDEV`, `PSPSDK`, the SDL2 header and library folders it picked,
whether `psp-gcc` and `psp-pacman` are on `PATH`, and every `SDL.h` and
`libSDL2*.a` under the toolchain. The Makefile looks for `SDL.h` by name in
`<pspdev>/psp/include/SDL2`, `<pspdev>/psp/include`,
`<pspdev>/psp/sdk/include` and whatever `psp-pkg-config --cflags SDL2` says, so
an installed SDL2 is found even when it sits somewhere unusual.

## Build

Open the **MSYS2 shell** that comes with pspdev (`C:\pspdev\msys2.exe`), then:

```sh
export PSPDEV=/c/pspdev
export PATH=$PSPDEV/bin:$PATH

cd /c/path/to/KablukMapperPortable
make
```

`make` compiles the editor's 21 objects next to their sources (they are
ignored by git), builds the vendored map tools the same way, and links
`EBOOT.PBP` in this folder. `make clean` removes all of it.

The same from `cmd.exe`, if you prefer it:

```bat
set PSPDEV=C:\pspdev
set PATH=C:\pspdev\bin;%PATH%
cd C:\path\to\KablukMapperPortable
make
```

The Makefile finds the SDK itself instead of trusting `psp-config` blindly: it
tries `$PSPDEV/psp/sdk` first, then `psp-config --pspsdk-path`, then
`/c/pspdev/psp/sdk`, and keeps the first one that really contains
`lib/build.mak`. If none of them do, it says which paths it looked at and
stops, so a wrong shell cannot produce a half built `EBOOT.PBP`.

Either path can also be given on the command line, which is the shortest way
out of any doubt about where the toolchain is:

```sh
make PSPDEV=/c/pspdev
make PSPSDK=/c/pspdev/psp/sdk
```

### What the Makefile sets up

| flag | why |
| --- | --- |
| `-O2 -G0` | optimise, and never emit MIPS16 (the GU code and the vendored compilers are much slower in it) |
| `-std=gnu99` | tyrutils is C99; the editor itself stays C89 clean |
| `-DTYRUTILS_VERSION=v0.17` | the vendored qbsp/light/vis want it |
| `-w` for `vendor/tyrutils` | third party code, its warnings are not ours |
| `-lSDL2main -lSDL2 -lGL -lpspvram -lpspaudio -lpspvfpu -lpsphprm -lpsppower -lpsprtc -lm` | SDL2 plus the PSP modules it links against |

`SDL2main` provides the real entry point on the PSP (`SDL_main.h` renames our
`main` to `SDL_main` under `__PSP__`), so `main.c` keeps a plain
`int main(int argc, char *argv[])` and must not define `PSP_MODULE_INFO`
itself.

## Onto the Memory Stick

```sh
make dist
```

writes `dist/PSP/GAME/KABLUK/` with everything the editor wants next to the
binary:

```
dist/PSP/GAME/KABLUK/EBOOT.PBP
dist/PSP/GAME/KABLUK/Quake.fgd
dist/PSP/GAME/KABLUK/palette.lmp
dist/PSP/GAME/KABLUK/*.wad
dist/PSP/GAME/KABLUK/maps/
```

Copy that `KABLUK` folder to `ms0:/PSP/GAME/` (or `ef0:/PSP/GAME/` on a Go).
On start the program makes its own folder the working directory
(`app_chdir_to_eboot` in `main.c`, falling back to `ms0:/PSP/GAME/KABLUK`), so
`Quake.fgd` and every relative path in the settings resolve from there no
matter where the XMB launched it from.

The first run has no wad yet: open the settings (`SELECT`) and pick
`WAD FILE (TEXTURES)`, or load a `.map` whose worldspawn already names one.

## Controls

The PSP video driver never reports keyboard events - there are no keys - so
`input.c` translates the pad into the very same key and mouse events the
desktop version receives. Every other file is unchanged.

| pad | editor |
| --- | --- |
| D-pad | cursor / list / settings rows; hold for a moment to auto-repeat |
| Nub | the mouse pointer |
| `X` (cross) | confirm: click, pick a file, press the highlighted button |
| `O` (circle) | back / cancel |
| `□` (square) | left click where the pointer is - this is how a block gets a tool or a texture gets picked |
| `△` (triangle) | jump to the property panel |
| `L` / `R` | previous / next panel |
| `SELECT` | settings window |
| `START` | compile window |

The mapping between the two lives in `input.c` (`PSP_PAD_MAPPING`): the file
installs it for the pad's own GUID before anything is opened, so the layout
above is the same on every SDL2 build - stock SDL2 has no controller entry for
`PSP builtin joypad`, and without one the pad would not be reported as a game
controller at all.

Two of these are worth remembering: **cross** drives every list and button,
and **square** is the mouse. Moving the nub slides a pointer over the screen
and pressing square acts at that spot, which is what the block palette, the
texture picker and the property rows expect.

Text fields (the map name, a property value) are typed with the built-in
on-screen keyboard: D-pad moves over the letters, cross types, circle closes.

There is no `ESC` on the handheld: the start screen's `QUIT` returns to the
XMB, and `ESC` / circle leaves the editor back to the start screen.

## Notes and limits

* **Textures.** The PSP renderer only accepts `BGR565`, `ABGR1555`,
  `ABGR4444` and `ABGR8888`, with 512 x 512 as the maximum. `ARGB8888` - what
  the editor builds its block previews in - is not among them, so `gfx.c` asks
  the renderer which format it takes once (`gfx_choose_format`) and converts
  the pixels on upload. On the PSP that is `ABGR8888`: same colours, red and
  blue swapped, no visible difference. The biggest texture the editor makes is
  64 x 64, which the converter handles in a small static buffer.
* **Screen.** 480 x 272 is the real resolution, not a scaled window, so the
  layout (80 px palette, 160 px properties, 15 x 17 grid) is used as designed.
* **Memory.** The editor's own 21 objects are ~60 KB of code. The heap is the
  rest of the console's 32 MB (pspsdk's start code takes all of it), and the
  bulk of it goes to tyrutils while a map compiles; a map of the size shipped
  here compiles to ~443 KB of `.bsp`.
* **Draw calls.** Everything on screen is a filled rectangle or a line - the
  text is glyph pixels merged into vertical runs. The editor's own screen is
  about 1 000 of them per frame, the row heavy file browser about 3 800,
  which is what the PSP's graphics unit is there for at the 60 fps the loop
  caps to. If a future screen ever feels slow, the first thing to add is an
  8 x 8 glyph atlas texture instead of the runs (`gfx_text()` is the only
  place that draws them).
* **No filesystem surprises.** The PSP build uses newlib's `ms0:/` paths, and
  `fs.c` creates folders with the same code as on the desktop.

## Troubleshooting

| what you see | what it means |
| --- | --- |
| `psp-config: command not found` | `PATH` / `PSPDEV` is not set in this shell - see Build |
| `/pspdev/psp/sdk/lib/build.mak: No such file or directory` | the Windows `psp-config` dropped the `C:` of `C:\pspdev`; say where it is: `make PSPDEV=/c/pspdev` (the Makefile also tries `/c/pspdev/psp/sdk` by itself) |
| `SDL.h: No such file or directory` | SDL2 is not in the pspdev prefix: `psp-pacman -S sdl2`, then `make sdk-info` to see where it landed |
| `psp-pacman`: `msys-archive-13.dll: cannot open shared object file` | the bundled pacman was built against an older MSYS2 `libarchive` than the one on your machine - skip it and extract `pspdev-win-libraries-15.2.0-v2.zip` over `C:\pspdev` instead |
| `cannot find -lSDL2main` / `-lSDL2` | the headers are there but the static libraries are not; the same `psp-pacman -S sdl2` fixes it, `make sdk-info` lists the `.a` files it found |
| `EBOOT.PBP` builds, screen stays black | the firmware is older than 6.00 (`PSP_FW_VERSION` in the Makefile), or the folder is not `ms0:/PSP/GAME/KABLUK` |
| starts, then "WAD MISSING" in the editor | pick the wad in the settings, or put it next to `EBOOT.PBP` |
| the compile stops with an allocation error on a big map | the heap is what is left of the PSP's 32 MB after the screen, the textures and the wad; compile with a smaller wad loaded |

## State of this port

Everything below the toolchain is in the repo and builds as C99 with the SDL2
calls the PSP driver implements: window + accelerated renderer, `COPY`
sprites, points, lines and filled rectangles, `UpdateTexture`, game controller
events, timers. The input layer was exercised against a fake SDL that reports
a game controller, which is how the button table above was checked.

`pspdev` itself is not part of this repository, so the first `make` on your
machine is the real test - if anything in the SDK disagrees with the Makefile,
that is where it will show.
