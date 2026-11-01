#!/bin/sh
#
# Host tests for the parts of the editor that need no window:
#   test_menu.c   - menu.c + settings.c + fs.c: the settings rows, the start
#                   screen and the file browser (built for the desktop and
#                   with -D__PSP__ for the PSP paths)
#   test_input.c  - input.c, with the SDL game controller API faked
#   test_picker.c - wad.c and the geometry of the texture picker grid, with
#                   the drawing functions replaced by recording stubs
#
# The real sources are compiled against the SDL2 headers that ship in the
# repository; tests/shim holds the two headers a host build needs to get past
# SDL_config.h, and a stub of pspkernel.h.
#
#   sh tests/run.sh          (any POSIX shell, no SDL2 library needed)
#
set -e

root=$(cd "$(dirname "$0")/.." && pwd)
src="$root/KablukMapperPortable"
inc="-I$src -I$root/SDL2/include -I$root/tests/shim"
work=${TMPDIR:-/tmp}/kmp-tests
cc=${CC:-gcc}
flags="-std=gnu99 -Wall -Wextra -Werror"

rm -rf "$work"
mkdir -p "$work/stick/out" "$work/stick/ms0:/PSP/GAME/KABLUK" "$work/bin"

echo "== menu / settings / browser (desktop)"
$cc $flags $inc -o "$work/bin/menu" \
    "$root/tests/test_menu.c" "$src/menu.c" "$src/fs.c" "$src/settings.c"
KMP_TEST_CWD="$work/stick" "$work/bin/menu"

echo
echo "== menu / settings / browser (PSP paths)"
$cc $flags $inc -D__PSP__ -DSDL_MAIN_HANDLED -o "$work/bin/menu_psp" \
    "$root/tests/test_menu.c" "$src/menu.c" "$src/fs.c" "$src/settings.c"
KMP_TEST_CWD="$work/stick" "$work/bin/menu_psp"

echo
echo "== pad translation"
$cc $flags $inc -o "$work/bin/input" "$root/tests/test_input.c" "$src/input.c"
"$work/bin/input"

echo
echo "== wad reader and the texture picker grid"
$cc $flags $inc -o "$work/bin/picker" \
    "$root/tests/test_picker.c" "$src/picker.c" "$src/textures.c" "$src/wad.c" \
    "$src/fgd.c"
KMP_TEST_CWD="$work/stick" "$work/bin/picker"

echo
echo "== the same test, built as the PSP (power-of-two previews)"
$cc $flags $inc -D__PSP__ -DSDL_MAIN_HANDLED -o "$work/bin/picker_psp" \
    "$root/tests/test_picker.c" "$src/picker.c" "$src/textures.c" "$src/wad.c" \
    "$src/fgd.c"
KMP_TEST_CWD="$work/stick" "$work/bin/picker_psp"

echo
echo "== type checked with pspdev's integer types"
# pspdev's SDL spells Uint32 as "unsigned long", the desktop headers as
# "unsigned int". They are different types even when both are 32 bits, and a
# "&format" handed to SDL_QueryTexture compiles on the host and fails on the
# console. Rebuild the headers with the console's spelling and type check
# every source against them - -fsyntax-only, so this is fast.
mkdir -p "$work/sdl_psp"
for h in "$root"/SDL2/include/*.h; do
    sed 's/^typedef uint32_t Uint32;/typedef unsigned long Uint32;/' "$h" |
        grep -v '^SDL_COMPILE_TIME_ASSERT(' > "$work/sdl_psp/$(basename "$h")" || true
done
for f in "$src"/*.c; do
    $cc -fsyntax-only $flags -I"$src" -I"$work/sdl_psp" -I"$root/tests/shim" \
        -I"$src/vendor/tyrutils/include" "$f"
done
echo "every source type checks with Uint32 = unsigned long"

echo
echo "all host tests passed"
