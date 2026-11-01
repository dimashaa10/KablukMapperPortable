/*
 * Kabluk Mapper Portable - keyboard.h
 *
 * The on screen keyboard the right panel opens when you pick a property.
 * Two layouts, because typing "200" on a full keyboard is silly:
 *   KBD_TEXT    - a-z, 0-9 and the few punctuation marks Quake keys use
 *   KBD_NUMBER  - numpad: 0-9, minus and a dot
 *
 * Navigation is the same as everywhere else in the editor: arrows move,
 * cross / X types the highlighted key, circle / Z closes without saving,
 * START / ENTER saves. A mouse click on a key does the same as X on it.
 * On the desktop every printable key of the real keyboard is also accepted.
 */
#ifndef KMP_KEYBOARD_H
#define KMP_KEYBOARD_H

#include <SDL.h>

#include "config.h"
#include "fgd.h"
#include "gfx.h"

#define KBD_NONE 0
#define KBD_APPLY 1  /* save k->value into the entity */
#define KBD_CANCEL 2 /* close and keep the old value */

typedef struct Keyboard {
    int open;
    int numeric; /* 1 = numpad layout */
    int row;
    int col;
    int hover; /* key under the mouse, -1 = none */

    char key[FGD_NAME_LEN]; /* property being edited, for the title */
    char value[FGD_VALUE_LEN];
    int len;

    int cx; /* entity cell the value belongs to, -1:-1 = worldspawn */
    int cy;
} Keyboard;

void keyboard_init(Keyboard *kbd);
int keyboard_is_open(const Keyboard *kbd);

/* Opens the editor for one property. 'value' may be NULL (empty field). */
void keyboard_open(Keyboard *kbd, const char *key, const char *value,
                   int numeric, int cx, int cy);
void keyboard_close(Keyboard *kbd);

/* Returns KBD_NONE while the keyboard stays open, KBD_APPLY / KBD_CANCEL
 * when the user finished. The caller stores kbd->value on KBD_APPLY. */
int keyboard_handle_event(Keyboard *kbd, const SDL_Event *event);
void keyboard_draw(Gfx *g, const Keyboard *kbd);

#endif /* KMP_KEYBOARD_H */
