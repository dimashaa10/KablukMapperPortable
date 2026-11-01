/*
 * Kabluk Mapper Portable - menu.h
 *
 * Modal screens:
 *   MENU_SETTINGS - editor settings opened with SELECT: path to the .wad, the
 *                   folder compiled maps are written to, the name of the map
 *                   source and the floor / ceiling textures
 *   MENU_BROWSE   - small file browser used to pick either of them; it can
 *                   also create folders
 *
 * The menu never opens a keyboard or a list itself: it raises a MENU_REQUEST_*
 * and main.c does it (it owns the one keyboard and the one picker).
 */
#ifndef KMP_MENU_H
#define KMP_MENU_H

#include <SDL.h>

#include "fs.h"
#include "gfx.h"
#include "settings.h"

#define MENU_CLOSED 0
#define MENU_SETTINGS 1
#define MENU_BROWSE 2

#define MENU_ROW_WAD 0
#define MENU_ROW_OUT 1
#define MENU_ROW_NAME 2
#define MENU_ROW_FLOOR 3
#define MENU_ROW_CEILING 4
#define MENU_ROW_SAVE 5
#define MENU_ROW_CLOSE 6
#define MENU_ROW_COUNT 7

/* what the menu wants main.c to do next */
#define MENU_REQUEST_NONE 0
#define MENU_REQUEST_NAME 1    /* type the map source name */
#define MENU_REQUEST_FOLDER 2  /* type the name of a new folder */
#define MENU_REQUEST_FLOOR 3   /* pick the floor texture */
#define MENU_REQUEST_CEILING 4 /* pick the ceiling texture */
#define MENU_REQUEST_SAVE 5    /* write the .map source now */

#define BROWSE_WAD 0
#define BROWSE_DIR 1

#define BROWSE_VISIBLE 13

#define MENU_STATUS_LEN 64

/* forward declared: menu.c only reads the state of a "save source" job */
struct CompileJob;

typedef struct Menu {
    int mode;
    Settings *settings;
    const struct CompileJob *job;

    int sel;   /* highlighted row in the settings window */
    int hover; /* mouse hover (-1 = none)                  */

    int browse_target; /* BROWSE_WAD / BROWSE_DIR */
    int return_mode;   /* where the browser goes back to: MENU_SETTINGS or MENU_CLOSED */
    char path[PATH_LEN];
    FsEntry entries[FS_MAX_ENTRIES];
    int count;
    int cur;
    int top;

    int request;             /* MENU_REQUEST_*, cleared by main.c */
    char status[MENU_STATUS_LEN]; /* last save / new folder result */
} Menu;

void menu_init(Menu *menu, Settings *settings, const struct CompileJob *job);
int menu_is_open(const Menu *menu);
void menu_open_settings(Menu *menu);

/* Opens the file browser on its own (used by the compile window to pick the
 * output folder); after picking or cancelling it returns to 'return_mode'. */
void menu_open_browse(Menu *menu, int target, int return_mode);
void menu_close(Menu *menu);

/* Results of what main.c did for a MENU_REQUEST_* */
void menu_set_map_name(Menu *menu, const char *name);
void menu_create_folder(Menu *menu, const char *name);
void menu_set_texture(Menu *menu, int ceiling, const char *name);

/* Returns 1 when the event was used by the menu. */
int menu_handle_event(Menu *menu, const SDL_Event *event);
void menu_draw(Gfx *g, const Menu *menu);

#endif /* KMP_MENU_H */
