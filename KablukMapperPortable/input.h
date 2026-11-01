/*
 * Kabluk Mapper Portable - input.h
 *
 * The PSP has no keyboard and no mouse: its D-pad, buttons and analog nub
 * reach SDL as a game controller, not as keys. This module turns that pad
 * back into the plain key and mouse events the rest of the editor already
 * understands, so every screen (start menu, editor, settings, browser,
 * keyboard list, compile window) works without knowing about the PSP.
 *
 * On the desktop it does the same for a plugged in gamepad and stays asleep
 * when there is none, which makes the mapping testable on a PC.
 */
#ifndef KMP_INPUT_H
#define KMP_INPUT_H

#include <SDL.h>

/* pad buttons that map to a key; keep in sync with pad_keys[] in input.c */
#define INPUT_MAX_KEYS 12

/* how many events one frame can produce (a key, a repeat, a click, a move) */
#define INPUT_EVENT_MAX 8

typedef struct Input {
    SDL_GameController *pad;
    Uint32 retry_at;                    /* next attempt to open the pad  */
    unsigned char held[INPUT_MAX_KEYS]; /* button state of the last frame */
    Uint32 next_at[INPUT_MAX_KEYS];     /* auto repeat for the D-pad      */
    unsigned char click_held;           /* square = click                 */

    float pointer_x; /* the pad pointer, in screen pixels */
    float pointer_y;
    int last_px; /* last pointer position that was sent, -1 = none */
    int last_py;
} Input;

/* Opens the pad if there is one. Never fatal: no pad just means no events. */
void input_init(Input *input);
void input_shutdown(Input *input);

/* Reads the pad and appends the events it produced to 'events' (at most
 * 'max'). Returns how many were written. Call once per frame. */
int input_pump(Input *input, SDL_Event *events, int max);

#endif /* KMP_INPUT_H */
