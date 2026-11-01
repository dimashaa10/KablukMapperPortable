# ---------------------------------------------------------------------------
# Kabluk Mapper Portable - PSP build
#
#   Uses pspdev / pspsdk through its own build.mak, so it needs a pspdev
#   install (https://github.com/pspdev/pspdev, on Windows the pspdev-win
#   bundle). From the pspdev shell:
#
#       export PSPDEV=/c/pspdev
#       export PATH=$PSPDEV/bin:$PATH
#       cd KablukMapperPortable
#       make          # -> KablukMapper.elf + EBOOT.PBP
#       make dist     # -> dist/PSP/GAME/KABLUK, copy to the memory stick
#
#   docs/psp-build.md has the long version.
# ---------------------------------------------------------------------------

TARGET = KablukMapper
PSP_EBOOT_TITLE = Kabluk Mapper Portable
EXTRA_TARGETS = EBOOT.PBP

# the XMB artwork (PSP only, 144x80 and 480x272)
PSP_EBOOT_ICON = ../psp/ICON0.PNG
PSP_EBOOT_PIC1 = ../psp/PIC1.PNG

# ---------------------------------------------------------------------------
# Where the SDK lives.
#
# "psp-config --pspsdk-path" is the usual answer, but on Windows it can hand
# back a path that make cannot open: a native Windows psp-config drops the
# drive letter, so C:/pspdev/psp/sdk arrives as /pspdev/psp/sdk and the include
# below fails with "No such file or directory". So try the places in order and
# keep the first one that really has a build.mak. Override with
#   make PSPDEV=/c/pspdev          or      make PSPSDK=/c/pspdev/psp/sdk
# ---------------------------------------------------------------------------
SDK_TRIES = $(if $(PSPDEV),$(PSPDEV)/psp/sdk) \
            $(shell psp-config --pspsdk-path 2>/dev/null) \
            $(shell psp-config --psp-prefix 2>/dev/null)/sdk \
            /c/pspdev/psp/sdk /c/dev/pspdev/psp/sdk \
            /usr/local/pspdev/psp/sdk /opt/pspdev/psp/sdk

PSPSDK := $(firstword $(foreach dir,$(SDK_TRIES),\
              $(if $(wildcard $(dir)/lib/build.mak),$(dir))))

ifeq ($(PSPSDK),)
$(info pspsdk not found; looked for lib/build.mak in:)
$(info    $(SDK_TRIES))
$(error set PSPDEV, e.g. "make PSPDEV=/c/pspdev" - see docs/psp-build.md)
endif

# build.mak needs PSPDEV too; take it from the SDK when it is not usable as is
ifeq ($(wildcard $(PSPDEV)/psp/include/SDL2/SDL.h),)
PSPDEV := $(patsubst %/psp/sdk,%,$(PSPSDK))
endif

# ---------------------------------------------------------------------------
# SDL2 for the PSP. The toolchain package puts it in <pspdev>/psp/include/SDL2
# and <pspdev>/psp/lib; psp-pkg-config knows when it is somewhere else. The
# headers are looked for by name, because a wrong -I only shows up as
# "SDL.h: No such file or directory" 21 times over.
# ---------------------------------------------------------------------------
SDL2_INC_TRIES = $(patsubst -I%,%,$(shell psp-pkg-config --cflags SDL2 2>/dev/null)) \
                 $(PSPDEV)/psp/include/SDL2 $(PSPDEV)/psp/include \
                 $(PSPDEV)/psp/sdk/include

SDL2_INC := $(firstword $(foreach dir,$(SDL2_INC_TRIES),\
                $(if $(wildcard $(dir)/SDL.h),$(dir))))

SDL2_LIBDIR_TRIES = $(patsubst -L%,%,$(shell psp-pkg-config --libs SDL2 2>/dev/null)) \
                    $(PSPDEV)/psp/lib $(PSPDEV)/psp/sdk/lib

SDL2_LIBDIR := $(firstword $(foreach dir,$(SDL2_LIBDIR_TRIES),\
                   $(if $(wildcard $(dir)/libSDL2.a),$(dir))))

ifeq ($(SDL2_INC),)
$(warning SDL2 headers not found - looked for SDL.h in: $(SDL2_INC_TRIES))
$(warning install SDL2 into the toolchain: psp-pacman -S sdl2   ("make sdk-info" shows what was found))
SDL2_INC := $(PSPDEV)/psp/include/SDL2
endif

ifeq ($(SDL2_LIBDIR),)
$(warning libSDL2.a not found - looked in: $(SDL2_LIBDIR_TRIES))
SDL2_LIBDIR := $(PSPDEV)/psp/lib
endif

# ---------------------------------------------------------------------------
# Path form. The shell says /c/pspdev, and most Windows psp-gcc builds take
# that - but a native Windows one reads it as C:\c\pspdev and then cannot find
# a single header, which looks exactly like "SDL.h: No such file or directory"
# one line into the first file. So ask the compiler once, with the cheapest
# test there is, and hand it C:/pspdev/... when the POSIX form does not work.
# Both forms are legal for msys/cygwin binaries, the Windows one is legal
# everywhere.
# ---------------------------------------------------------------------------
# (no -o: psp-gcc writes the preprocessed text to stdout and the shell throws it
# away, which also keeps "/dev/null" away from a Windows compiler)
psp_have_header = $(shell { printf '#include <$(1)>\n' | psp-gcc -E -xc -I$(2) - ; } \
                    >/dev/null 2>&1 && echo yes)

POSIX_PATHS_OK := $(call psp_have_header,SDL.h,$(SDL2_INC))

CONVERT_PATHS :=
ifeq ($(POSIX_PATHS_OK),)
ifneq ($(shell command -v psp-gcc 2>/dev/null),)
ifneq ($(shell command -v cygpath 2>/dev/null),)
CONVERT_PATHS := 1
$(warning psp-gcc does not find the SDK headers through /c/... paths - handing it C:/... instead)
endif
endif
endif

win = $(if $(CONVERT_PATHS),$(shell cygpath -m '$(1)' 2>/dev/null || echo '$(1)'),$(1))

# kept in the form the shell uses, for the messages and "make sdk-info"
PSPDEV_POSIX := $(PSPDEV)
PSPSDK_POSIX := $(PSPSDK)
PSPSDK_MAKE := $(PSPSDK)

OBJS = main.o input.o gfx.o grid.o ui.o font.o layout.o props.o settings.o \
       fs.o menu.o fgd.o wad.o textures.o picker.o compile.o compileui.o \
       bspchain.o keyboard.o values.o mapfile.o

# The three map tools are compiled into the editor (see bspchain.c).
TYR_SRC = $(wildcard vendor/tyrutils/common/*.c) \
          $(wildcard vendor/tyrutils/qbsp/*.c) \
          $(wildcard vendor/tyrutils/light/*.c) \
          $(wildcard vendor/tyrutils/vis/*.c)
TYR_OBJS = $(TYR_SRC:.c=.o)

# the compilers are linked into the editor, so they are part of the link list
OBJS += $(TYR_OBJS)

CFLAGS = -O2 -G0 -Wall -std=gnu99 -D__PSP__ -DKMP_HAVE_PSP \
         -I. -I$(call win,$(SDL2_INC)) -DTYRUTILS_VERSION=v0.17-kabluk

# static link order matters: SDL2 first, then the psp libraries it needs.
LIBS = -L$(call win,$(SDL2_LIBDIR)) -lSDL2main -lSDL2 -lGL -lpspvram \
       -lpspaudio -lpspvfpu -lpsphprm -lpsppower -lpsprtc -lm

# The vendored TyrUtils is C99 and warns on everything GCC 15 knows.
$(TYR_OBJS): %.o: %.c
	$(CC) $(CFLAGS) -w -c $< -o $@

# build.mak puts $(PSPDEV)/psp/include and $(PSPSDK)/include on the psp-gcc
# command line itself, so from here on they carry the form the compiler wants.
# "override": PSPDEV may have come from the command line, and it still has to
# arrive at psp-gcc in the form the compiler understands.
override PSPDEV := $(call win,$(PSPDEV))
override PSPSDK := $(call win,$(PSPSDK))

# build.mak brings the psp-gcc rules, PARAM.SFO and pack-pbp.
include $(PSPSDK_MAKE)/lib/build.mak

# A folder that can be copied straight to the memory stick: EBOOT.PBP plus
# the .fgd it needs. A .wad and palette.lmp are copied when they sit in
# resources/ (they are game data and are not part of the repository).
dist: EBOOT.PBP
	rm -rf dist
	mkdir -p dist/PSP/GAME/KABLUK/maps
	cp EBOOT.PBP dist/PSP/GAME/KABLUK/
	cp ../resources/Quake.fgd dist/PSP/GAME/KABLUK/
	@for f in ../resources/*.wad ../resources/palette.lmp; do \
		if [ -f "$$f" ]; then cp "$$f" dist/PSP/GAME/KABLUK/; fi; \
	done; true
	@echo ""
	@echo "copy the contents of dist/ to the root of the memory stick,"
	@echo "then start Kabluk Mapper Portable from the Game menu."

# What the build found, for when something is missing on this machine.
sdk-info:
	@echo "PSPDEV      : $(PSPDEV_POSIX)$(if $(CONVERT_PATHS), -> $(PSPDEV))"
	@echo "PSPSDK      : $(PSPSDK_POSIX)$(if $(CONVERT_PATHS), -> $(PSPSDK))"
	@echo "SDL2 headers: $(SDL2_INC)"
	@echo "SDL2 library: $(SDL2_LIBDIR)"
	@echo "path form   : $(if $(CONVERT_PATHS),C:/... (psp-gcc wants Windows paths),/c/... (psp-gcc takes POSIX paths))"
	@echo "psp-gcc     : $$(command -v psp-gcc || echo '(not on PATH)')"
	@echo "psp-pacman  : $$(command -v psp-pacman || echo '(not on PATH)')"
	@echo "SDL.h files under $(PSPDEV_POSIX):"
	@-find "$(PSPDEV_POSIX)" -maxdepth 6 -name SDL.h 2>/dev/null | head -5
	@echo "libSDL2*.a files under $(PSPDEV_POSIX):"
	@-find "$(PSPDEV_POSIX)" -maxdepth 6 -name 'libSDL2*.a' 2>/dev/null | head -5

.PHONY: dist sdk-info
