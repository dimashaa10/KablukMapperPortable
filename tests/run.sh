#!/bin/sh
#
# Host tests for the parts of the editor that need no window:
#   test_menu.c   - menu.c + settings.c + fs.c: the settings rows, the start
#                   screen and the file browser (built for the desktop and
#                   with -D__PSP__ for the PSP paths)
#   test_input.c  - input.c, with the SDL game controller API faked
#   test_picker.c - wad.c and the geometry of the texture picker grid, with
#                   the drawing functions replaced by recording stubs
#   test_crash.c  - crash.c, the breadcrumb file a console that dies leaves
#                   behind (built with -DKMP_HAVE_PSP: the writing half only
#                   exists there)
#   test_prefs.c  - prefs.c, the kabluk.cfg the editor settings live in
#   test_compile.c - compile.c, the .map the exporter writes: two slabs and
#                   whatever the user painted, no walls around the border
#
# and one check with no test program of its own: the tools' stack frames, read
# out of gcc's -fstack-usage, against the 256 KB the console gives the main
# thread. light's first face overran that once and killed the console with no
# log line at all; see docs/compile-chain.md.
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
echo "== the settings file (floor / ceiling textures survive a restart)"
$cc $flags $inc -o "$work/bin/prefs" \
    "$root/tests/test_prefs.c" "$src/prefs.c" "$src/settings.c" "$src/fs.c"
KMP_TEST_CWD="$work/stick" "$work/bin/prefs"

echo
echo "== the phase breadcrumbs a dead console leaves"
$cc $flags $inc -D__PSP__ -DKMP_HAVE_PSP -DSDL_MAIN_HANDLED \
    -o "$work/bin/crash" "$root/tests/test_crash.c" "$src/crash.c"
KMP_TEST_CWD="$work/stick" "$work/bin/crash"

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
echo "== the .map the exporter writes (no walls around the border)"
$cc $flags $inc -D__PSP__ -DKMP_HAVE_PSP -DSDL_MAIN_HANDLED \
    -o "$work/bin/compile" "$root/tests/test_compile.c" "$src/compile.c" \
    "$src/grid.c" "$src/settings.c" "$src/fs.c" "$src/crash.c" \
    "$src/values.c" "$src/fgd.c"
KMP_TEST_CWD="$work/stick" "$work/bin/compile"

echo
echo "== the tools' stack frames fit the console's stack"
# pspdev creates the main thread with 256 KB (pspsdk src/startup/crt0.c,
# DEFAULT_THREAD_STACK_KB_SIZE). Upstream light kept 477 KB of lightmaps in one
# frame on that stack, which is what took the console down in the first face of
# the first map with nothing in any log. -fstack-usage prints every frame, so a
# return of that cannot go unnoticed. 128 KB is the floor, not the target: the
# largest frame in the tools is qbsp's LoadBrush() at 69 744.
tut="$src/vendor/tyrutils"
su="$work/stack"
mkdir -p "$su"
n=0
for f in "$tut"/light/*.c "$tut"/vis/*.c "$tut"/common/*.c "$tut"/qbsp/*.c; do
    n=$((n + 1))
    $cc -std=gnu99 -O1 -c -fstack-usage -I"$tut/include" \
        -I"$tut/include/common" -I"$tut" -I"$tut/qbsp" "$f" -o "$su/o$n.o" \
        2>/dev/null
done
big=$(cat "$su"/*.su | awk -F'\t' '$2 + 0 > 131072 { print $2 " bytes  " $1 }' | sort -rn)
if [ -n "$big" ]; then
    echo "these frames are bigger than a 256 KB stack has room for:"
    echo "$big"
    exit 1
fi
cat "$su"/*.su | awk -F'\t' '{ if ($2 + 0 > top) { top = $2 + 0; who = $1 } }
                            END { print "largest frame: " top " bytes (" who ")" }'

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
