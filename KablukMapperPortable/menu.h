/*
 * Kabluk Mapper Portable - menu.h
 *
 * Modal screens:
 *   MENU_SETTINGS - editor settings opened with SELECT: path to the .wad and
 *                   the folder compiled maps are written to
 *   MENU_BROWSE   - small file browser used to pick either of them
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
#define MENU_ROW_CLOSE 2
#define MENU_ROW_COUNT 3

#define BROWSE_WAD 0
#define BROWSE_DIR 1

#define BROWSE_VISIBLE 13

typedef struct Menu {
    int mode;
    Settings *settings;

    int sel;    /* highlighted row in the settings window */
    int hover;  /* mouse hover (-1 = none)                  */

    int browse_target; /* BROWSE_WAD / BROWSE_DIR */
    int return_mode;   /* where the browser goes back to: MENU_SETTINGS or MENU_CLOSED */
    char path[PATH_LEN];
    FsEntry entries[FS_MAX_ENTRIES];
    int count;
    int cur;
    int top;
} Menu;

void menu_init(Menu *menu, Settings *settings);
int menu_is_open(const Menu *menu);
void menu_open_settings(Menu *menu);

/* Opens the file browser on its own (used by the compile window to pick the
 * output folder); after picking or cancelling it returns to 'return_mode'. */
void menu_open_browse(Menu *menu, int target, int return_mode);
void menu_close(Menu *menu);

/* Returns 1 when the event was used by the menu. */
int menu_handle_event(Menu *menu, const SDL_Event *event);
void menu_draw(Gfx *g, const Menu *menu);

#endif /* KMP_MENU_H */
