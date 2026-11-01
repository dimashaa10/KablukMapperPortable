/*
 * Kabluk Mapper Portable - input.c
 *
 * The PSP pad, as keyboard and mouse events. SDL delivers the pad as a game
 * controller (cross = A, circle = B, square = X, triangle = Y, L / R and the
 * usual D-pad / stick layout), so this file only has to translate:
 *
 *   D-pad          -> arrow keys (cursor, menus, lists) with auto repeat
 *   analog nub     -> mouse pointer (hover, clicks land where it points)
 *   cross  (A)     -> KEY_CONFIRM  ("X" on the desktop)
 *   circle (B)     -> KEY_CANCEL   ("Z")
 *   square (X)     -> left mouse click at the pointer (tool buttons, rows)
 *   triangle (Y)   -> KEY_PROP_FOCUS (open the property panel)
 *   L / R          -> fold the left / right panel
 *   SELECT         -> editor settings
 *   START          -> compile window
 *
 * Only the D-pad repeats; a repeating "confirm" would fire the same action
 * again and again while the button is held.
 */
#include "input.h"

#include <stdio.h>
#include <string.h>

#include "config.h"
#include "keys.h"

#define INPUT_DEADZONE 8000    /* of 32767, the nub rests near 0             */
#define INPUT_REPEAT_DELAY 300 /* ms before a held direction repeats         */
#define INPUT_REPEAT_RATE 90   /* ms between the repeats                     */
#define INPUT_POINTER_SPEED 6.0f /* pixels per frame at full deflection      */
#define INPUT_OPEN_RETRY 1000  /* ms between attempts to find the pad        */

/* The PSP joystick driver reports one pad called "PSP builtin joypad", with
 * the buttons in this order (SDL_sysjoystick.c):
 *
 *   0 triangle  1 circle  2 cross  3 square  4 L  5 R
 *   6 down  7 left  8 up  9 right  10 select  11 start  12 home  13 hold
 *
 * and two axes, the nub. SDL only calls a joystick a game controller when a
 * mapping for its GUID is known, and the controller database an SDL build
 * carries may well not have a PSP entry - so the mapping below is installed
 * here, for the pad's own GUID, before anything is opened. It also overrides
 * a database entry, which keeps the button layout identical on every build. */
#define PSP_PAD_NAME "PSP builtin joypad"
#define PSP_PAD_MAPPING                                                \
    "a:b2,b:b1,x:b3,y:b0,"   /* cross, circle, square, triangle */    \
    "leftshoulder:b4,rightshoulder:b5,"                                \
    "back:b10,start:b11,"    /* select, start */                       \
    "dpup:b8,dpdown:b6,dpleft:b7,dpright:b9,"                          \
    "leftx:a0,lefty:a1"      /* the nub */

typedef struct PadKey {
    int button; /* SDL_GameControllerButton */
    int sym;    /* SDL_Keycode we produce   */
    int repeat; /* 1 = auto repeat (D-pad)  */
} PadKey;

/* INPUT_MAX_KEYS entries at most - see input.h */
static const PadKey pad_keys[] = {
    { SDL_CONTROLLER_BUTTON_DPAD_UP, KEY_UP, 1 },
    { SDL_CONTROLLER_BUTTON_DPAD_DOWN, KEY_DOWN, 1 },
    { SDL_CONTROLLER_BUTTON_DPAD_LEFT, KEY_LEFT, 1 },
    { SDL_CONTROLLER_BUTTON_DPAD_RIGHT, KEY_RIGHT, 1 },
    { SDL_CONTROLLER_BUTTON_A, KEY_CONFIRM, 0 },
    { SDL_CONTROLLER_BUTTON_B, KEY_CANCEL, 0 },
    { SDL_CONTROLLER_BUTTON_Y, KEY_PROP_FOCUS, 0 },
    { SDL_CONTROLLER_BUTTON_LEFTSHOULDER, KEY_PANEL_LEFT, 0 },
    { SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, KEY_PANEL_RIGHT, 0 },
    { SDL_CONTROLLER_BUTTON_BACK, KEY_OPEN_SETTINGS, 0 },
    { SDL_CONTROLLER_BUTTON_START, KEY_OPEN_COMPILE, 0 }
};

#define PAD_KEY_COUNT ((int)(sizeof(pad_keys) / sizeof(pad_keys[0])))

/* ticks passed a deadline, safe against the tick counter wrapping */
static int deadline_passed(Uint32 now, Uint32 deadline)
{
    return (int)(now - deadline) >= 0;
}

/* Give the PSP pad a controller mapping of our own, so the button layout is
 * the same no matter which SDL2 build the program was linked against.
 * Anything that is not the PSP pad is left alone - a PC gamepad has its own
 * mapping, and the keyboard works without one. */
static void input_map_psp_pad(int index)
{
    SDL_JoystickGUID guid;
    char guid_text[33];
    char mapping[256];
    const char *name;

    name = SDL_JoystickNameForIndex(index);
    if (name == NULL || strcmp(name, PSP_PAD_NAME) != 0) {
        return;
    }
    guid = SDL_JoystickGetDeviceGUID(index);
    SDL_JoystickGetGUIDString(guid, guid_text, (int)sizeof(guid_text));
    /* no snprintf on the PSP: the fields are cut with a precision instead */
    sprintf(mapping, "%.32s,%.32s,%s", guid_text, name, PSP_PAD_MAPPING);
    if (SDL_GameControllerAddMapping(mapping) < 0) {
        SDL_Log("pad mapping rejected: %s", SDL_GetError());
    }
}

static void input_open(Input *input)
{
    int count;
    int i;

    input->retry_at = SDL_GetTicks() + INPUT_OPEN_RETRY;

    count = SDL_NumJoysticks();
    for (i = 0; i < count; i++) {
        input_map_psp_pad(i);
    }
    for (i = 0; i < count; i++) {
        const char *name;

        if (!SDL_IsGameController(i)) {
            continue;
        }
        input->pad = SDL_GameControllerOpen(i);
        if (input->pad == NULL) {
            continue;
        }
        name = SDL_GameControllerName(input->pad);
        SDL_Log("pad: %s", name != NULL ? name : "game controller");
        return;
    }
}

void input_init(Input *input)
{
    int i;

    if (input == NULL) {
        return;
    }
    memset(input, 0, sizeof(*input));
    input->pointer_x = SCREEN_W / 2.0f;
    input->pointer_y = SCREEN_H / 2.0f;
    input->last_px = -1;
    input->last_py = -1;
    for (i = 0; i < INPUT_MAX_KEYS; i++) {
        input->next_at[i] = 0;
    }

    /* A pad is optional: the editor is perfectly usable with the keyboard,
     * and on the PSP the module is already there when we start. */
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        SDL_Log("no game controller support: %s", SDL_GetError());
        return;
    }
    input_open(input);
}

void input_shutdown(Input *input)
{
    if (input == NULL || input->pad == NULL) {
        return;
    }
    SDL_GameControllerClose(input->pad);
    input->pad = NULL;
}

static int push_key(SDL_Event *event, int sym)
{
    memset(event, 0, sizeof(*event));
    event->type = SDL_KEYDOWN;
    event->key.type = SDL_KEYDOWN;
    event->key.state = 1; /* SDL_PRESSED */
    event->key.keysym.sym = sym;
    return 1;
}

static int push_motion(SDL_Event *event, Input *input, int x, int y)
{
    if (x == input->last_px && y == input->last_py) {
        return 0;
    }
    memset(event, 0, sizeof(*event));
    event->type = SDL_MOUSEMOTION;
    event->motion.type = SDL_MOUSEMOTION;
    event->motion.x = x;
    event->motion.y = y;
    input->last_px = x;
    input->last_py = y;
    return 1;
}

static int push_click(SDL_Event *event, int x, int y)
{
    memset(event, 0, sizeof(*event));
    event->type = SDL_MOUSEBUTTONDOWN;
    event->button.type = SDL_MOUSEBUTTONDOWN;
    event->button.button = SDL_BUTTON_LEFT;
    event->button.x = x;
    event->button.y = y;
    return 1;
}

int input_pump(Input *input, SDL_Event *events, int max)
{
    Uint32 now;
    int count = 0;
    int px;
    int py;
    int i;

    if (input == NULL || events == NULL || max <= 0) {
        return 0;
    }
    now = SDL_GetTicks();

    if (input->pad == NULL) {
        if (!deadline_passed(now, input->retry_at)) {
            return 0;
        }
        input_open(input);
        if (input->pad == NULL) {
            return 0;
        }
    }

    SDL_GameControllerUpdate();

    for (i = 0; i < PAD_KEY_COUNT && i < INPUT_MAX_KEYS; i++) {
        int down = SDL_GameControllerGetButton(
            input->pad, (SDL_GameControllerButton)pad_keys[i].button);

        if (down && !input->held[i]) {
            input->held[i] = 1;
            if (count < max) {
                count += push_key(&events[count], pad_keys[i].sym);
            }
            input->next_at[i] = now + INPUT_REPEAT_DELAY;
        } else if (down) {
            if (pad_keys[i].repeat && deadline_passed(now, input->next_at[i])) {
                if (count < max) {
                    count += push_key(&events[count], pad_keys[i].sym);
                }
                input->next_at[i] = now + INPUT_REPEAT_RATE;
            }
        } else {
            input->held[i] = 0;
        }
    }

    /* the nub moves the pointer; the D-pad keeps stepping the grid cursor,
     * so the two never fight each other */
    {
        Sint16 ax = SDL_GameControllerGetAxis(input->pad,
                                              SDL_CONTROLLER_AXIS_LEFTX);
        Sint16 ay = SDL_GameControllerGetAxis(input->pad,
                                              SDL_CONTROLLER_AXIS_LEFTY);

        if (ax > -INPUT_DEADZONE && ax < INPUT_DEADZONE) {
            ax = 0;
        }
        if (ay > -INPUT_DEADZONE && ay < INPUT_DEADZONE) {
            ay = 0;
        }
        if (ax != 0 || ay != 0) {
            input->pointer_x += (float)ax / 32767.0f * INPUT_POINTER_SPEED;
            input->pointer_y += (float)ay / 32767.0f * INPUT_POINTER_SPEED;
            if (input->pointer_x < 0.0f) {
                input->pointer_x = 0.0f;
            }
            if (input->pointer_y < 0.0f) {
                input->pointer_y = 0.0f;
            }
            if (input->pointer_x > SCREEN_W - 1.0f) {
                input->pointer_x = SCREEN_W - 1.0f;
            }
            if (input->pointer_y > SCREEN_H - 1.0f) {
                input->pointer_y = SCREEN_H - 1.0f;
            }
            px = (int)input->pointer_x;
            py = (int)input->pointer_y;
            if (count < max) {
                count += push_motion(&events[count], input, px, py);
            }
        }
    }

    /* square clicks where the pointer is */
    {
        int down =
            SDL_GameControllerGetButton(input->pad, SDL_CONTROLLER_BUTTON_X);

        if (down && !input->click_held) {
            input->click_held = 1;
            px = (int)input->pointer_x;
            py = (int)input->pointer_y;
            if (count < max) {
                count += push_click(&events[count], px, py);
            }
        } else if (!down) {
            input->click_held = 0;
        }
    }

    return count;
}
