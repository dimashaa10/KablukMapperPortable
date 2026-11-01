#!/bin/sh
#
# Host tests for the parts of the editor that need no window:
#   test_menu.c   - menu.c + settings.c + fs.c: the settings rows, the start
#                   screen and the file browser (built for the desktop and
#                   with -D__PSP__ for the PSP paths)
#   test_input.c  - input.c, with the SDL game controller API faked
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
echo "all host tests passed"
