/*
 * Host test: input.c, the pad -> key / pointer translation.
 *
 * The SDL game controller API is faked with a single pad, so the translation
 * itself can be walked through: the nub in both directions, a nub that does
 * not rest in the middle, and the buttons that pick a tool.
 */
#include <stdio.h>
#include <string.h>

#include "input.h"
#include "keys.h"

struct FakePad {
    unsigned char button[16];
    Sint16 axis[2];
};
static struct FakePad pad;
static Sint16 joy_axis[2];
static int joy_naxes = 2;

SDL_GameController *SDL_GameControllerOpen(int index)
{
    (void)index;
    return (SDL_GameController *)&pad;
}
const char *SDL_GameControllerName(SDL_GameController *c)
{
    (void)c;
    return "PSP builtin joypad";
}
SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController *c)
{
    return (SDL_Joystick *)c;
}
int SDL_JoystickNumAxes(SDL_Joystick *j)
{
    (void)j;
    return joy_naxes;
}
Sint16 SDL_JoystickGetAxis(SDL_Joystick *j, int axis)
{
    (void)j;
    return axis >= 0 && axis < 2 ? joy_axis[axis] : 0;
}
Sint16 SDL_GameControllerGetAxis(SDL_GameController *c,
                                 SDL_GameControllerAxis axis)
{
    (void)c;
    return axis == SDL_CONTROLLER_AXIS_LEFTX ? joy_axis[0]
         : axis == SDL_CONTROLLER_AXIS_LEFTY ? joy_axis[1] : 0;
}
Uint8 SDL_GameControllerGetButton(SDL_GameController *c,
                                  SDL_GameControllerButton button)
{
    (void)c;
    return button >= 0 && button < 16 ? pad.button[button] : 0;
}
int SDL_NumJoysticks(void)
{
    return 1;
}
SDL_bool SDL_IsGameController(int index)
{
    (void)index;
    return SDL_TRUE;
}
const char *SDL_JoystickNameForIndex(int index)
{
    (void)index;
    return "PSP builtin joypad";
}
SDL_JoystickGUID SDL_JoystickGetDeviceGUID(int index)
{
    SDL_JoystickGUID g;

    (void)index;
    memset(&g, 0, sizeof(g));
    return g;
}
void SDL_JoystickGetGUIDString(SDL_JoystickGUID g, char *psz, int cb)
{
    (void)g;
    if (cb > 0) {
        psz[0] = '0';
        if (cb > 1) {
            psz[1] = '\0';
        }
    }
}
int SDL_GameControllerAddMapping(const char *m)
{
    (void)m;
    return 1;
}
void SDL_GameControllerUpdate(void) {}
Uint32 SDL_GetTicks(void)
{
    static Uint32 t = 1000;

    t += 16;
    return t;
}
int SDL_InitSubSystem(Uint32 flags)
{
    (void)flags;
    return 0;
}
void SDL_GameControllerClose(SDL_GameController *c)
{
    (void)c;
}
void SDL_Log(const char *fmt, ...)
{
    (void)fmt;
}
const char *SDL_GetError(void)
{
    return "fake";
}
Uint32 SDL_GetTicks(void);

/* ---- test helpers ---------------------------------------------------- */
static int fails;
static void check(int ok, const char *what, const char *detail)
{
    if (!ok) {
        fails++;
    }
    printf("%-48s %s%s%s\n", what, ok ? "ok" : "FAIL",
           detail != NULL ? "  " : "", detail != NULL ? detail : "");
}

static Input in;
static SDL_Event ev[16];

static int run(void)
{
    int n = input_pump(&in, ev, 16);

    return n;
}

static int got(int n, Uint32 type, int sym)
{
    int i;

    for (i = 0; i < n; i++) {
        if (ev[i].type == type &&
            (type != SDL_KEYDOWN || ev[i].key.keysym.sym == sym)) {
            return 1;
        }
    }
    return 0;
}

int main(void)
{
    int n;

    memset(&pad, 0, sizeof(pad));
    memset(&in, 0, sizeof(in));
    input_init(&in);
    check(in.pad != NULL, "pad opened", NULL);
    check(in.psp_pad == 1, "PSP pad recognised", NULL);

    /* first frame: the rest position is learned, nothing moves */
    joy_axis[0] = 0;
    joy_axis[1] = 0;
    n = run();
    check(n == 0, "resting nub sends nothing", NULL);
    check(in.pointer_x == 240.0f && in.pointer_y == 136.0f, "pointer at the middle",
          NULL);

    /* vertical: up moves the pointer up, down moves it down */
    joy_axis[1] = 32767; /* SDL: positive Y points down */
    n = run();
    check(in.pointer_y > 136.0f, "nub down moves the pointer down", NULL);
    check(!got(n, SDL_KEYDOWN, KEY_UP), "nub does not fake an arrow key", NULL);
    joy_axis[1] = -32768;
    n = run();
    check(in.pointer_y < 142.0f, "nub up moves the pointer up", NULL);

    /* horizontal still works */
    joy_axis[1] = 0;
    {
        float x0 = in.pointer_x;

        joy_axis[0] = 32767;
        run();
        check(in.pointer_x > x0, "nub right moves the pointer right", NULL);
        joy_axis[0] = 0;
    }

    /* a worn nub: it rests far off centre, the pointer must stay put */
    memset(&in, 0, sizeof(in));
    input_init(&in);
    joy_axis[0] = 3000;
    joy_axis[1] = -24000;
    run();                    /* learns the rest position */
    {
        float y0 = in.pointer_y;
        float x0 = in.pointer_x;
        int i;

        for (i = 0; i < 30; i++) {
            run();
        }
        check(in.pointer_y == y0 && in.pointer_x == x0,
              "an off-centre nub does not drift", NULL);
        joy_axis[1] = 24000; /* now pushed the other way, past the rest */
        run();
        check(in.pointer_y > y0, "and it still moves from there", NULL);
    }

    /* buttons */
    memset(&in, 0, sizeof(in));
    input_init(&in);
    joy_axis[0] = 0;
    joy_axis[1] = 0;
    run();

    pad.button[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = 1;
    n = run();
    check(got(n, SDL_KEYDOWN, KEY_TOOL_NEXT), "R -> next tool", NULL);
    pad.button[SDL_CONTROLLER_BUTTON_RIGHTSHOULDER] = 0;
    run();
    pad.button[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = 1;
    n = run();
    check(got(n, SDL_KEYDOWN, KEY_TOOL_PREV), "L -> previous tool", NULL);
    pad.button[SDL_CONTROLLER_BUTTON_LEFTSHOULDER] = 0;
    run();

    pad.button[SDL_CONTROLLER_BUTTON_Y] = 1;
    n = run();
    check(got(n, SDL_KEYDOWN, KEY_PROP_FOCUS), "triangle -> property panel", NULL);
    pad.button[SDL_CONTROLLER_BUTTON_Y] = 0;
    run();

    pad.button[SDL_CONTROLLER_BUTTON_A] = 1;
    n = run();
    check(got(n, SDL_KEYDOWN, KEY_CONFIRM), "cross -> confirm", NULL);
    pad.button[SDL_CONTROLLER_BUTTON_A] = 0;
    run();

    /* square clicks where the pointer is, once per press */
    {
        int clicks = 0;
        int i;

        pad.button[SDL_CONTROLLER_BUTTON_X] = 1;
        n = run();
        clicks += got(n, SDL_MOUSEBUTTONDOWN, 0);
        for (i = 0; i < 5; i++) {
            n = run();
            clicks += got(n, SDL_MOUSEBUTTONDOWN, 0);
        }
        check(clicks == 1, "square clicks once while held", NULL);
        pad.button[SDL_CONTROLLER_BUTTON_X] = 0;
        run();
        pad.button[SDL_CONTROLLER_BUTTON_X] = 1;
        n = run();
        check(got(n, SDL_MOUSEBUTTONDOWN, 0), "square clicks again", NULL);
    }

    printf("\n%s (%d failure%s)\n", fails == 0 ? "PASS" : "FAIL", fails,
           fails == 1 ? "" : "s");
    return fails != 0;
}
