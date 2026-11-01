/*
 * Host test: the menu, the settings window and the file browser.
 *
 * It compiles the real menu.c / settings.c / fs.c and stubs the drawing, so
 * what is checked here is the logic: which row a click lands on, what the
 * browser starts on, where ".." stops, what the start screen does with
 * circle. run.sh builds it twice, as the desktop and as the PSP (__PSP__).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "menu.h"
#include "settings.h"
#include "fs.h"
#include "keys.h"

/* run.sh sets this to a scratch folder that looks like a memory stick */
#define TEST_CWD_ENV "KMP_TEST_CWD"

/* the settings window geometry menu.c uses, for the click tests */
#define BTN_Y 178      /* SET_Y + 164 */
#define BTN_CY (BTN_Y + 11)
#define MAIN_CX (206 + 52)
#define CLOSE_CX (364 + 42)

/* ---- drawing stubs -------------------------------------------------- */
int gfx_text_width(const char *text, int scale)
{
    (void)scale;
    return (int)strlen(text) * 6;
}
int gfx_text(Gfx *g, int x, int y, const char *text, unsigned int rgb, int scale)
{
    (void)g; (void)x; (void)y; (void)text; (void)rgb; (void)scale;
    return 0;
}
int gfx_text_int(Gfx *g, int x, int y, int v, unsigned int rgb, int scale)
{
    (void)g; (void)x; (void)y; (void)v; (void)rgb; (void)scale; return 0;
}
void gfx_text_right(Gfx *g, int x, int y, const char *t, unsigned int c, int s)
{ (void)g; (void)x; (void)y; (void)t; (void)c; (void)s; }
int gfx_text_clip(Gfx *g, int x, int y, int mw, const char *t, unsigned int c, int s)
{ (void)g; (void)x; (void)y; (void)mw; (void)t; (void)c; (void)s; return 0; }
void gfx_rect(Gfx *g, int x, int y, int w, int h, unsigned int c)
{ (void)g; (void)x; (void)y; (void)w; (void)h; (void)c; }
void gfx_outline(Gfx *g, int x, int y, int w, int h, unsigned int c)
{ (void)g; (void)x; (void)y; (void)w; (void)h; (void)c; }
void gfx_rect_alpha(Gfx *g, int x, int y, int w, int h, unsigned int c,
                    unsigned char a)
{ (void)g; (void)x; (void)y; (void)w; (void)h; (void)c; (void)a; }

/* ---- helpers -------------------------------------------------------- */
static int fails;
static void check(int ok, const char *what, const char *detail)
{
    if (!ok) {
        fails++;
    }
    printf("%-46s %s%s%s\n", what, ok ? "ok" : "FAIL",
           detail != NULL ? "  " : "", detail != NULL ? detail : "");
}

static void key_event(SDL_Event *e, int sym)
{
    memset(e, 0, sizeof(*e));
    e->type = SDL_KEYDOWN;
    e->key.type = SDL_KEYDOWN;
    e->key.keysym.sym = sym;
}

static void click(SDL_Event *e, int x, int y)
{
    memset(e, 0, sizeof(*e));
    e->type = SDL_MOUSEBUTTONDOWN;
    e->button.type = SDL_MOUSEBUTTONDOWN;
    e->button.button = SDL_BUTTON_LEFT;
    e->button.x = x;
    e->button.y = y;
}

int main(void)
{
    Settings s;
    Menu m;
    SDL_Event e;
    int i;

    memset(&m, 0, sizeof(m));
    settings_defaults(&s);
    menu_init(&m, &s, NULL);

    /* ---- the folder browser ----------------------------------------- */
    {
        const char *dir = getenv(TEST_CWD_ENV);

        if (dir != NULL && chdir(dir) != 0) {
            printf("cannot enter %s\n", dir);
            return 2;
        }
    }
#if KMP_ON_PSP
    /* nothing picked yet and the default "maps" is not there: ms0:/ */
    settings_defaults(&s);
    menu_open_browse(&m, BROWSE_DIR, MENU_SETTINGS);
    check(strcmp(m.path, "ms0:/") == 0, "PSP: fresh browse starts at ms0:/",
          m.path);
    check(m.entries[0].kind == FS_KIND_USE_THIS, "first entry is USE THIS FOLDER",
          NULL);

    /* an existing relative out_dir is shown as a real path */
    fs_copy(s.out_dir, PATH_LEN, "out");
    menu_open_browse(&m, BROWSE_DIR, MENU_SETTINGS);
    {
        char want[PATH_LEN];
        char here[PATH_LEN];

        if (getcwd(here, sizeof(here)) == NULL) {
            here[0] = '\0';
        }
        fs_join(want, PATH_LEN, here, "out");
        check(strcmp(m.path, want) == 0, "PSP: relative out_dir made absolute",
              m.path);
    }

    /* walking up from deep on the stick stops at ms0:/ */
    fs_copy(s.out_dir, PATH_LEN, "ms0:/PSP/GAME/KABLUK");
    menu_open_browse(&m, BROWSE_DIR, MENU_SETTINGS);
    for (i = 0; i < 10; i++) {
        int idx = -1;
        int n;

        for (n = 0; n < m.count; n++) {
            if (m.entries[n].kind == FS_KIND_PARENT) {
                idx = n;
            }
        }
        if (idx < 0) {
            break;
        }
        m.cur = idx;
        key_event(&e, KEY_CONFIRM);
        menu_handle_event(&m, &e);
    }
    check(strcmp(m.path, "ms0:/") == 0, "PSP: UP walks up to ms0:/ and stops",
          m.path);

    /* picking a folder on the stick */
    m.cur = 0; /* ( USE THIS FOLDER ) */
    key_event(&e, KEY_CONFIRM);
    menu_handle_event(&m, &e);
    check(strcmp(s.out_dir, "ms0:/") == 0, "PSP: USE THIS FOLDER keeps ms0:/",
          s.out_dir);
    check(m.mode == MENU_SETTINGS, "USE THIS FOLDER returns to the settings",
          NULL);
    { char out[PATH_LEN]; settings_map_path(&s, out, PATH_LEN);
      check(strcmp(out, "ms0:/kabluk.map") == 0, "PSP: map path joins once", out); }

    fs_copy(s.out_dir, PATH_LEN, "ms0:/PSP/GAME");
    menu_open_browse(&m, BROWSE_DIR, MENU_SETTINGS);
    m.cur = 0;
    key_event(&e, KEY_CONFIRM);
    menu_handle_event(&m, &e);
    check(strcmp(s.out_dir, "ms0:/PSP/GAME") == 0, "PSP: folder picked on the stick",
          s.out_dir);
#else
    /* desktop: the default "maps" is not there, so it opens on "." */
    settings_defaults(&s);
    menu_open_browse(&m, BROWSE_DIR, MENU_SETTINGS);
    check(strcmp(m.path, BROWSER_ROOT) == 0, "desktop: browse starts at BROWSER_ROOT",
          m.path);
    check(m.entries[0].kind == FS_KIND_USE_THIS, "first entry is USE THIS FOLDER",
          NULL);
    check(strcmp(m.entries[0].name, ".") == 0, "USE THIS FOLDER stays relative",
          m.entries[0].name);

    /* an existing out_dir is where it opens */
    fs_copy(s.out_dir, PATH_LEN, "out");
    menu_open_browse(&m, BROWSE_DIR, MENU_SETTINGS);
    check(strcmp(m.path, "out") == 0, "desktop: opens on an existing out_dir",
          m.path);
    m.cur = 0;
    key_event(&e, KEY_CONFIRM);
    menu_handle_event(&m, &e);
    check(strcmp(s.out_dir, "out") == 0, "desktop: USE THIS FOLDER keeps the dir",
          s.out_dir);
    check(m.mode == MENU_SETTINGS, "USE THIS FOLDER returns to the settings", NULL);
    { char out[PATH_LEN]; settings_map_path(&s, out, PATH_LEN);
      check(strcmp(out, "out/kabluk.map") == 0, "desktop: map path joins once", out); }
#endif

    /* ---- MAIN MENU row ---------------------------------------------- */
    menu_open_settings(&m);
    check(m.resume_editor == 0, "settings keep the resume flag as it was", NULL);
    for (i = 0; i < MENU_ROW_MAIN; i++) {
        key_event(&e, KEY_DOWN);
        menu_handle_event(&m, &e);
    }
    check(m.sel == MENU_ROW_MAIN, "6x DOWN lands on MAIN MENU", NULL);
    key_event(&e, KEY_CONFIRM);
    menu_handle_event(&m, &e);
    check(m.request == MENU_REQUEST_MAIN, "confirm raises MENU_REQUEST_MAIN", NULL);

    /* circle (cancel) closes the settings as before */
    menu_open_settings(&m);
    key_event(&e, KEY_CANCEL);
    menu_handle_event(&m, &e);
    check(m.mode == MENU_CLOSED, "circle still closes the settings", NULL);

    /* ---- the MAIN MENU button by mouse ------------------------------ */
    menu_open_settings(&m);
    m.request = MENU_REQUEST_NONE;
    click(&e, MAIN_CX, BTN_CY);
    menu_handle_event(&m, &e);
    check(m.request == MENU_REQUEST_MAIN, "clicking MAIN MENU raises the request",
          NULL);
    /* a click on the gap between the buttons must do nothing */
    menu_open_settings(&m);
    m.request = MENU_REQUEST_NONE;
    click(&e, 190, BTN_CY);
    menu_handle_event(&m, &e);
    check(m.request == MENU_REQUEST_NONE, "clicking the gap does nothing", NULL);
    /* and CLOSE still works */
    menu_open_settings(&m);
    click(&e, CLOSE_CX, BTN_CY);
    menu_handle_event(&m, &e);
    check(m.mode == MENU_CLOSED, "clicking CLOSE closes the settings", NULL);

    /* ---- the three buttons of the bottom row do not overlap --------- */
    {
        static const struct { int x; int row; } probes[] = {
            { 33, MENU_ROW_SAVE },  { 151, MENU_ROW_SAVE },
            { 153, -1 },            { 205, -1 },
            { 206, MENU_ROW_MAIN }, { 309, MENU_ROW_MAIN },
            { 310, -1 },            { 363, -1 },
            { 364, MENU_ROW_CLOSE },{ 447, MENU_ROW_CLOSE },
            { 448, -1 }
        };
        int n2 = (int)(sizeof(probes) / sizeof(probes[0]));
        int bad = 0;
        int k;

        for (k = 0; k < n2; k++) {
            int want = probes[k].row;

            menu_open_settings(&m);
            m.request = MENU_REQUEST_NONE;
            click(&e, probes[k].x, BTN_CY);
            menu_handle_event(&m, &e);
            if (want == MENU_ROW_CLOSE) {
                if (m.mode != MENU_CLOSED) {
                    printf("  x=%d wanted CLOSE\n", probes[k].x);
                    bad++;
                }
            } else if (want == MENU_ROW_MAIN) {
                if (m.request != MENU_REQUEST_MAIN) {
                    printf("  x=%d wanted MAIN MENU\n", probes[k].x);
                    bad++;
                }
            } else if (want == MENU_ROW_SAVE) {
                if (m.request != MENU_REQUEST_SAVE) {
                    printf("  x=%d wanted SAVE\n", probes[k].x);
                    bad++;
                }
            } else if (m.mode != MENU_SETTINGS || m.request != MENU_REQUEST_NONE) {
                printf("  x=%d wanted nothing\n", probes[k].x);
                bad++;
            }
        }
        check(bad == 0, "SAVE / MAIN MENU / CLOSE hit boxes tile the row", NULL);
    }

    /* ---- start screen: circle goes back to the map ------------------ */
    menu_open_main(&m);
    m.resume_editor = 0;
    key_event(&e, KEY_CANCEL);
    menu_handle_event(&m, &e);
    check(m.mode == MENU_MAIN, "circle does nothing on a fresh start", NULL);
    m.resume_editor = 1;
    key_event(&e, KEY_CANCEL);
    menu_handle_event(&m, &e);
    check(m.mode == MENU_CLOSED, "circle returns to the map being edited", NULL);

    /* ---- drawing must not crash in any mode ------------------------- */
    menu_open_main(&m);
    m.resume_editor = 0;
    menu_draw(NULL, &m);
    m.resume_editor = 1;
    menu_draw(NULL, &m);
    menu_open_settings(&m);
    menu_draw(NULL, &m);
    m.hover = MENU_ROW_MAIN;
    menu_draw(NULL, &m);
    fs_copy(s.out_dir, PATH_LEN, "ms0:/PSP/GAME");
    menu_open_browse(&m, BROWSE_DIR, MENU_SETTINGS);
    menu_draw(NULL, &m);
    menu_open_browse(&m, BROWSE_WAD, MENU_SETTINGS);
    menu_draw(NULL, &m);
    menu_open_browse(&m, BROWSE_MAP, MENU_CLOSED);
    menu_draw(NULL, &m);
    menu_close(&m);
    menu_draw(NULL, &m);
    check(1, "menu_draw runs in every mode", NULL);

    printf("\n%s (%d failure%s)\n", fails == 0 ? "PASS" : "FAIL", fails,
           fails == 1 ? "" : "s");
    return fails != 0;
}
