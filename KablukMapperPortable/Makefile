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

PSPSDK := $(shell psp-config --pspsdk-path)
ifeq ($(PSPSDK),)
$(error psp-config not found - run make from the pspdev shell, see docs/psp-build.md)
endif

# build.mak needs PSPDEV; derive it from psp-config when it is not exported
ifeq ($(PSPDEV),)
PSPDEV := $(patsubst %/psp,%,$(shell psp-config --psp-prefix))
endif

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
         -I. -I$(PSPDEV)/psp/include -I$(PSPDEV)/psp/include/SDL2 \
         -DTYRUTILS_VERSION=v0.17-kabluk

# static link order matters: SDL2 first, then the psp libraries it needs.
LIBS = -lSDL2main -lSDL2 -lGL -lpspvram -lpspaudio -lpspvfpu -lpsphprm \
       -lpsppower -lpsprtc -lm

# The vendored TyrUtils is C99 and warns on everything GCC 15 knows.
$(TYR_OBJS): %.o: %.c
	$(CC) $(CFLAGS) -w -c $< -o $@

# build.mak brings the psp-gcc rules, PARAM.SFO and pack-pbp.
include $(PSPSDK)/lib/build.mak

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

.PHONY: dist
