/*
 * Kabluk Mapper Portable - menu.c
 *
 * Editor settings window + the little file browser that picks a .wad file
 * or an output folder. Everything is drawn with the same gfx layer as the
 * rest of the editor, no widgets library.
 */
#include "menu.h"

#include "compile.h"
#include "config.h"
#include "fs.h"
#include "keys.h"

/* ------------------------------------------------------------------ */
/* Settings window geometry                                            */
/* ------------------------------------------------------------------ */
#define SET_X 20
#define SET_Y 14
#define SET_W (SCREEN_W - SET_X * 2) /* 440 */
#define SET_H 232
#define SET_FIELD_H 20
#define SET_TITLE_H 22

#define SET_FIELD_X (SET_X + 12)      /* 32  */
#define SET_FIELD_W (SET_W - 24)      /* 416 */
#define SET_HALF_W ((SET_FIELD_W - 8) / 2)     /* 204 */
#define SET_HALF2_X (SET_FIELD_X + SET_HALF_W + 8)

#define SET_ROW_GAP 34
#define SET_ROW0_Y (SET_Y + 36) /* wad        */
#define SET_ROW1_Y (SET_ROW0_Y + SET_ROW_GAP)
#define SET_ROW2_Y (SET_ROW1_Y + SET_ROW_GAP)
#define SET_ROW3_Y (SET_ROW2_Y + SET_ROW_GAP) /* floor + ceiling */

#define SET_BTN_Y (SET_Y + 164)
#define SET_BTN_H 22
#define SET_SAVE_W 120
#define SET_SAVE_X SET_FIELD_X
#define SET_CLOSE_W 84
#define SET_CLOSE_X (SET_X + SET_W - 12 - SET_CLOSE_W)
#define SET_HINT_Y (SET_BTN_Y + SET_BTN_H + 6)
#define SET_STATUS_Y (SET_HINT_Y + 24)

/* ------------------------------------------------------------------ */
/* File browser geometry                                               */
/* ------------------------------------------------------------------ */
#define BR_X 6
#define BR_Y 6
#define BR_W (SCREEN_W - 12) /* 468 */
#define BR_H (SCREEN_H - 12) /* 260 */
#define BR_PATH_Y (BR_Y + 22)
#define BR_PATH_H 16
#define BR_LIST_Y (BR_Y + 44)
#define BR_ROW_H 14
#define BR_ROW_TOP (BR_LIST_Y - 3)

/* "NEW FOLDER" button, in the title row on the right */
#define BR_NEW_W 96
#define BR_NEW_H 16
#define BR_NEW_X (BR_X + BR_W - 10 - BR_NEW_W)
#define BR_NEW_Y (BR_Y + 4)

static void browse_open(Menu *menu, int target);

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */
static int menu_clamp(int v, int lo, int hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static int menu_in_rect(int mx, int my, int x, int y, int w, int h)
{
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

/* decimal string without stdio */
static void menu_itoa(char *out, int value)
{
    char tmp[12];
    int n = 0;
    int i;
    int digits = 1;

    if (value < 0) {
        out[0] = '-';
        out++;
        value = -value;
    }
    if (value == 0) {
        tmp[n++] = '0';
    }
    while (digits * 10 <= value && digits < 1000000) {
        digits *= 10;
    }
    while (digits > 0) {
        tmp[n++] = (char)('0' + (value / digits) % 10);
        digits /= 10;
    }
    for (i = 0; i < n; i++) {
        out[i] = tmp[i];
    }
    out[n] = '\0';
}

/* ------------------------------------------------------------------ */
/* Life cycle                                                          */
/* ------------------------------------------------------------------ */
void menu_init(Menu *menu, Settings *settings, const struct CompileJob *job)
{
    menu->mode = MENU_CLOSED;
    menu->settings = settings;
    menu->job = job;
    menu->request = MENU_REQUEST_NONE;
    menu->status[0] = '\0';
    menu->sel = MENU_ROW_WAD;
    menu->hover = -1;
    menu->browse_target = BROWSE_WAD;
    menu->return_mode = MENU_SETTINGS;
    menu->path[0] = '\0';
    menu->count = 0;
    menu->cur = 0;
    menu->top = 0;
}

int menu_is_open(const Menu *menu)
{
    return menu->mode != MENU_CLOSED;
}

void menu_open_settings(Menu *menu)
{
    menu->mode = MENU_SETTINGS;
    menu->sel = MENU_ROW_WAD;
    menu->hover = -1;
}

void menu_open_browse(Menu *menu, int target, int return_mode)
{
    browse_open(menu, target);
    menu->return_mode = return_mode;
}

void menu_close(Menu *menu)
{
    menu->mode = MENU_CLOSED;
    menu->hover = -1;
}

/* ------------------------------------------------------------------ */
/* Browser                                                             */
/* ------------------------------------------------------------------ */
static void browse_refresh(Menu *menu)
{
    int n = 0;

    if (menu->browse_target == BROWSE_DIR) {
        menu->entries[n].kind = FS_KIND_USE_THIS;
        fs_copy(menu->entries[n].name, FS_NAME_LEN, ".");
        n++;
    }
    menu->entries[n].kind = FS_KIND_PARENT;
    fs_copy(menu->entries[n].name, FS_NAME_LEN, "..");
    n++;

    n += fs_list_dir(menu->path,
                     menu->browse_target == BROWSE_WAD ? ".wad" : NULL,
                     menu->entries + n, FS_MAX_ENTRIES - n);

    menu->count = n;
    menu->cur = 0;
    menu->top = 0;
    menu->hover = -1;
}

static void browse_open(Menu *menu, int target)
{
    menu->browse_target = target;
    menu->return_mode = MENU_SETTINGS;
    menu->mode = MENU_BROWSE;

    if (target == BROWSE_WAD) {
        if (menu->settings->wad_path[0] != '\0') {
            fs_copy(menu->path, PATH_LEN, menu->settings->wad_path);
            fs_parent(menu->path, PATH_LEN);
        } else {
            fs_copy(menu->path, PATH_LEN, ".");
        }
    } else {
        if (menu->settings->out_dir[0] != '\0') {
            fs_copy(menu->path, PATH_LEN, menu->settings->out_dir);
        } else {
            fs_copy(menu->path, PATH_LEN, ".");
        }
    }
    browse_refresh(menu);
}

static void browse_move(Menu *menu, int delta)
{
    if (menu->count <= 0) {
        return;
    }
    menu->cur = menu_clamp(menu->cur + delta, 0, menu->count - 1);
    if (menu->cur < menu->top) {
        menu->top = menu->cur;
    }
    if (menu->cur >= menu->top + BROWSE_VISIBLE) {
        menu->top = menu->cur - BROWSE_VISIBLE + 1;
    }
    if (menu->top > menu->count - 1) {
        menu->top = menu->count - 1;
    }
    if (menu->top < 0) {
        menu->top = 0;
    }
}

static void browse_activate(Menu *menu, int index)
{
    char tmp[PATH_LEN];
    const FsEntry *entry;

    if (index < 0 || index >= menu->count) {
        return;
    }
    entry = &menu->entries[index];

    switch (entry->kind) {
    case FS_KIND_USE_THIS:
        fs_copy(menu->settings->out_dir, PATH_LEN, menu->path);
        menu->mode = menu->return_mode;
        break;
    case FS_KIND_PARENT:
        fs_parent(menu->path, PATH_LEN);
        browse_refresh(menu);
        break;
    case FS_KIND_DIR:
        fs_join(tmp, PATH_LEN, menu->path, entry->name);
        fs_copy(menu->path, PATH_LEN, tmp);
        browse_refresh(menu);
        break;
    default: /* file */
        fs_join(menu->settings->wad_path, PATH_LEN, menu->path, entry->name);
        menu->mode = menu->return_mode;
        break;
    }
}

static int browse_hit(const Menu *menu, int mx, int my)
{
    int index;

    if (mx < BR_X + 8 || mx >= BR_X + BR_W - 8) {
        return -1;
    }
    if (my < BR_ROW_TOP) {
        return -1;
    }
    index = (my - BR_ROW_TOP) / BR_ROW_H;
    if (index < 0 || index >= BROWSE_VISIBLE) {
        return -1;
    }
    index += menu->top;
    if (index >= menu->count) {
        return -1;
    }
    return index;
}

static int settings_hit(int mx, int my)
{
    if (menu_in_rect(mx, my, SET_FIELD_X, SET_ROW0_Y, SET_FIELD_W, SET_FIELD_H)) {
        return MENU_ROW_WAD;
    }
    if (menu_in_rect(mx, my, SET_FIELD_X, SET_ROW1_Y, SET_FIELD_W, SET_FIELD_H)) {
        return MENU_ROW_OUT;
    }
    if (menu_in_rect(mx, my, SET_FIELD_X, SET_ROW2_Y, SET_FIELD_W, SET_FIELD_H)) {
        return MENU_ROW_NAME;
    }
    if (menu_in_rect(mx, my, SET_FIELD_X, SET_ROW3_Y, SET_HALF_W, SET_FIELD_H)) {
        return MENU_ROW_FLOOR;
    }
    if (menu_in_rect(mx, my, SET_HALF2_X, SET_ROW3_Y, SET_HALF_W, SET_FIELD_H)) {
        return MENU_ROW_CEILING;
    }
    if (menu_in_rect(mx, my, SET_SAVE_X, SET_BTN_Y, SET_SAVE_W, SET_BTN_H)) {
        return MENU_ROW_SAVE;
    }
    if (menu_in_rect(mx, my, SET_CLOSE_X, SET_BTN_Y, SET_CLOSE_W, SET_BTN_H)) {
        return MENU_ROW_CLOSE;
    }
    return -1;
}

/* what a settings row does when it is confirmed */
static void settings_activate(Menu *menu, int row)
{
    switch (row) {
    case MENU_ROW_WAD:
        browse_open(menu, BROWSE_WAD);
        break;
    case MENU_ROW_OUT:
        browse_open(menu, BROWSE_DIR);
        break;
    case MENU_ROW_NAME:
        menu->request = MENU_REQUEST_NAME;
        break;
    case MENU_ROW_FLOOR:
        menu->request = MENU_REQUEST_FLOOR;
        break;
    case MENU_ROW_CEILING:
        menu->request = MENU_REQUEST_CEILING;
        break;
    case MENU_ROW_SAVE:
        menu->request = MENU_REQUEST_SAVE;
        break;
    default:
        menu_close(menu);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Results of what main.c did for a request                            */
/* ------------------------------------------------------------------ */
static void menu_status(Menu *menu, const char *prefix, const char *text)
{
    char line[MENU_STATUS_LEN];
    int n = 0;
    int i;

    for (i = 0; prefix[i] != '\0' && n < MENU_STATUS_LEN - 1; i++) {
        line[n++] = prefix[i];
    }
    for (i = 0; text[i] != '\0' && n < MENU_STATUS_LEN - 1; i++) {
        line[n++] = text[i];
    }
    line[n] = '\0';
    fs_copy(menu->status, MENU_STATUS_LEN, line);
}

void menu_set_map_name(Menu *menu, const char *name)
{
    fs_sanitize_name(menu->settings->map_name, FS_NAME_LEN, name);
    menu_status(menu, "NAME: ", menu->settings->map_name[0] != '\0'
                                   ? menu->settings->map_name
                                   : DEFAULT_MAP_NAME);
}

void menu_create_folder(Menu *menu, const char *name)
{
    char clean[FS_NAME_LEN];
    char full[PATH_LEN];

    fs_sanitize_name(clean, (int)sizeof(clean), name);
    if (clean[0] == '\0') {
        fs_copy(menu->status, MENU_STATUS_LEN, "NO FOLDER NAME GIVEN");
        return;
    }
    fs_join(full, PATH_LEN, menu->path, clean);
    if (fs_mkdir(full)) {
        menu_status(menu, "NEW FOLDER: ", clean);
        browse_refresh(menu);
        menu->mode = MENU_BROWSE;
    } else {
        menu_status(menu, "CANNOT CREATE: ", clean);
    }
}

void menu_set_texture(Menu *menu, int ceiling, const char *name)
{
    if (ceiling) {
        fs_copy(menu->settings->ceiling_tex, WAD_NAME_LEN, name);
    } else {
        fs_copy(menu->settings->floor_tex, WAD_NAME_LEN, name);
    }
    menu->mode = MENU_SETTINGS;
    menu_status(menu, ceiling ? "CEILING: " : "FLOOR: ", name);
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */
int menu_handle_event(Menu *menu, const SDL_Event *event)
{
    int hit;

    if (menu->mode == MENU_CLOSED) {
        return 0;
    }

    switch (event->type) {
    case SDL_KEYDOWN:
        switch (event->key.keysym.sym) {
        case KEY_OPEN_SETTINGS:
            if (menu->mode == MENU_BROWSE) {
                menu->mode = menu->return_mode;
            } else {
                menu_close(menu);
            }
            return 1;
        case KEY_UP:
            if (menu->mode == MENU_SETTINGS) {
                menu->sel = (menu->sel + MENU_ROW_COUNT - 1) % MENU_ROW_COUNT;
            } else {
                browse_move(menu, -1);
            }
            return 1;
        case KEY_DOWN:
            if (menu->mode == MENU_SETTINGS) {
                menu->sel = (menu->sel + 1) % MENU_ROW_COUNT;
            } else {
                browse_move(menu, 1);
            }
            return 1;
        case KEY_CONFIRM:
            if (menu->mode == MENU_SETTINGS) {
                settings_activate(menu, menu->sel);
            } else {
                browse_activate(menu, menu->cur);
            }
            return 1;
        case KEY_NEW_FOLDER:
            if (menu->mode == MENU_BROWSE) {
                menu->request = MENU_REQUEST_FOLDER;
                return 1;
            }
            return 1;
        case KEY_CANCEL:
            if (menu->mode == MENU_BROWSE) {
                menu->mode = menu->return_mode;
            } else {
                menu_close(menu);
            }
            return 1;
        default:
            return 1; /* modal: swallow the rest */
        }

    case SDL_MOUSEMOTION:
        if (menu->mode == MENU_SETTINGS) {
            menu->hover = settings_hit(event->motion.x, event->motion.y);
        } else {
            menu->hover = browse_hit(menu, event->motion.x, event->motion.y);
        }
        return 1;

    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button != SDL_BUTTON_LEFT) {
            return 1;
        }
        if (menu->mode == MENU_SETTINGS) {
            hit = settings_hit(event->button.x, event->button.y);
            if (hit >= 0) {
                menu->sel = hit;
                settings_activate(menu, hit);
            }
        } else if (menu_in_rect(event->button.x, event->button.y, BR_NEW_X,
                                BR_NEW_Y, BR_NEW_W, BR_NEW_H)) {
            menu->request = MENU_REQUEST_FOLDER;
        } else {
            hit = browse_hit(menu, event->button.x, event->button.y);
            if (hit >= 0) {
                menu->cur = hit;
                browse_activate(menu, hit);
            }
        }
        return 1;

    case SDL_MOUSEWHEEL:
        if (menu->mode == MENU_BROWSE) {
            browse_move(menu, event->wheel.y > 0 ? -1 : 1);
        }
        return 1;

    default:
        return 1;
    }
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void draw_field(Gfx *g, int x, int w, int y, const char *label,
                       const char *value, const char *empty_text, int selected,
                       const char *action)
{
    gfx_text(g, x, y - 10, label, COL_TEXT_DIM, 1);

    gfx_rect(g, x, y, w, SET_FIELD_H, COL_FIELD);
    gfx_outline(g, x, y, w, SET_FIELD_H, selected ? COL_ACCENT : COL_BTN_EDGE);

    if (action != NULL && action[0] != '\0') {
        int waction = gfx_text_width(action, 1);

        if (value != NULL && value[0] != '\0') {
            gfx_text_clip(g, x + 6, y + 7, w - waction - 16, value, COL_TEXT, 1);
        } else {
            gfx_text(g, x + 6, y + 7, empty_text, COL_TEXT_DIM, 1);
        }
        gfx_text_right(g, x + w - 6, y + 7, action,
                       selected ? COL_ACCENT : COL_TEXT_DIM, 1);
        return;
    }

    if (value != NULL && value[0] != '\0') {
        gfx_text_clip(g, x + 6, y + 7, w - 12, value, COL_TEXT, 1);
    } else {
        gfx_text(g, x + 6, y + 7, empty_text, COL_TEXT_DIM, 1);
    }
}

static void draw_button(Gfx *g, int x, int y, int w, int h, const char *label,
                        int active)
{
    int label_w = gfx_text_width(label, 1);

    gfx_rect(g, x, y, w, h, active ? COL_BTN_ACTIVE : COL_BTN);
    gfx_outline(g, x, y, w, h, active ? COL_BTN_EDGE_ACTIVE : COL_BTN_EDGE);
    gfx_text(g, x + (w - label_w) / 2, y + 7, label,
             active ? COL_ACCENT : COL_TEXT, 1);
}

/* One status line: the running "save the map source" job if there is one,
 * otherwise whatever the last action reported. */
static void draw_status(Gfx *g, const Menu *menu, int x, int y, int w)
{
    const struct CompileJob *job = menu->job;

    if (job != NULL && job->save_only) {
        if (job->state == COMPILE_RUNNING) {
            gfx_text(g, x, y, "SAVING...", COL_TEXT_DIM, 1);
            return;
        }
        if (job->state == COMPILE_DONE) {
            gfx_text_clip(g, x, y, w, job->map_path, COL_OK, 1);
            return;
        }
        if (job->state == COMPILE_FAILED) {
            gfx_text_clip(g, x, y, w, "COULD NOT WRITE THE .MAP", COL_WARN, 1);
            return;
        }
    }
    if (menu->status[0] != '\0') {
        gfx_text_clip(g, x, y, w, menu->status, COL_TEXT_DIM, 1);
    }
}

static void draw_settings(Gfx *g, const Menu *menu)
{
    int close_hover;
    int save_hover;

    gfx_rect(g, SET_X, SET_Y, SET_W, SET_H, COL_PANEL);
    gfx_outline(g, SET_X, SET_Y, SET_W, SET_H, COL_ACCENT);

    gfx_rect(g, SET_X + 1, SET_Y + 1, SET_W - 2, SET_TITLE_H, COL_BTN);
    gfx_text(g, SET_X + 10, SET_Y + 8, "EDITOR SETTINGS", COL_ACCENT, 1);
    gfx_text_right(g, SET_X + SET_W - 10, SET_Y + 8, "[SELECT] CLOSE", COL_TEXT_DIM,
                   1);

    draw_field(g, SET_FIELD_X, SET_FIELD_W, SET_ROW0_Y, "WAD FILE  (TEXTURES)",
               menu->settings->wad_path, "( NOT SET )", menu->sel == MENU_ROW_WAD,
               "BROWSE");
    draw_field(g, SET_FIELD_X, SET_FIELD_W, SET_ROW1_Y, "COMPILED FILES GO TO",
               menu->settings->out_dir, "CURRENT FOLDER",
               menu->sel == MENU_ROW_OUT, "BROWSE");
    draw_field(g, SET_FIELD_X, SET_FIELD_W, SET_ROW2_Y,
               "MAP SOURCE NAME  (.MAP, .BSP)", menu->settings->map_name,
               "( " DEFAULT_MAP_NAME " )", menu->sel == MENU_ROW_NAME, "TYPE");

    draw_field(g, SET_FIELD_X, SET_HALF_W, SET_ROW3_Y, "FLOOR TEXTURE",
               menu->settings->floor_tex, "( FIRST IN WAD )",
               menu->sel == MENU_ROW_FLOOR, "PICK");
    draw_field(g, SET_HALF2_X, SET_HALF_W, SET_ROW3_Y, "CEILING TEXTURE",
               menu->settings->ceiling_tex, "( FIRST IN WAD )",
               menu->sel == MENU_ROW_CEILING, "PICK");

    save_hover = (menu->sel == MENU_ROW_SAVE) || (menu->hover == MENU_ROW_SAVE);
    draw_button(g, SET_SAVE_X, SET_BTN_Y, SET_SAVE_W, SET_BTN_H, "SAVE .MAP",
                save_hover);
    close_hover = (menu->sel == MENU_ROW_CLOSE) || (menu->hover == MENU_ROW_CLOSE);
    draw_button(g, SET_CLOSE_X, SET_BTN_Y, SET_CLOSE_W, SET_BTN_H, "CLOSE",
                close_hover);

    gfx_text(g, SET_FIELD_X, SET_HINT_Y,
             "NAME = THE .MAP WRITTEN TO THAT FOLDER,", COL_TEXT_DIM, 1);
    gfx_text(g, SET_FIELD_X, SET_HINT_Y + 12,
             "EMPTY TEXTURES USE THE FIRST ONE IN THE WAD.", COL_TEXT_DIM, 1);

    draw_status(g, menu, SET_FIELD_X, SET_STATUS_Y, SET_FIELD_W);
}

static void draw_entry_label(char *out, int cap, const FsEntry *entry)
{
    int i = 0;

    if (entry->kind == FS_KIND_USE_THIS) {
        fs_copy(out, cap, "( USE THIS FOLDER )");
        return;
    }
    if (entry->kind == FS_KIND_PARENT) {
        fs_copy(out, cap, ".. ( UP )");
        return;
    }
    while (entry->name[i] != '\0' && i < cap - 2) {
        out[i] = entry->name[i];
        i++;
    }
    if (entry->kind == FS_KIND_DIR) {
        out[i++] = '/';
    }
    out[i] = '\0';
}

static void draw_browse(Gfx *g, const Menu *menu)
{
    int i;
    int index;
    int y;
    int selected;
    unsigned int color;
    char label[FS_NAME_LEN + 4];

    gfx_rect(g, BR_X, BR_Y, BR_W, BR_H, COL_PANEL);
    gfx_outline(g, BR_X, BR_Y, BR_W, BR_H, COL_ACCENT);

    gfx_text(g, BR_X + 10, BR_Y + 8,
             menu->browse_target == BROWSE_WAD ? "SELECT WAD FILE"
                                               : "SELECT OUTPUT FOLDER",
             COL_ACCENT, 1);

    /* "NEW FOLDER" button, top right */
    draw_button(g, BR_NEW_X, BR_NEW_Y, BR_NEW_W, BR_NEW_H, "NEW FOLDER",
                menu->request == MENU_REQUEST_FOLDER);

    /* current path */
    gfx_rect(g, BR_X + 8, BR_PATH_Y, BR_W - 16, BR_PATH_H, COL_FIELD);
    gfx_outline(g, BR_X + 8, BR_PATH_Y, BR_W - 16, BR_PATH_H, COL_BTN_EDGE);
    gfx_text_clip(g, BR_X + 13, BR_PATH_Y + 5, BR_W - 26, menu->path, COL_TEXT, 1);

    /* entries */
    for (i = 0; i < BROWSE_VISIBLE; i++) {
        index = menu->top + i;
        if (index >= menu->count) {
            break;
        }
        y = BR_ROW_TOP + i * BR_ROW_H;
        selected = (index == menu->cur);
        if (!selected && index == menu->hover) {
            gfx_rect(g, BR_X + 8, y - 1, BR_W - 16, BR_ROW_H - 1, COL_BTN_HOVER);
        }
        if (selected) {
            gfx_rect(g, BR_X + 8, y - 1, BR_W - 16, BR_ROW_H - 1, COL_ROW_SEL);
        }

        draw_entry_label(label, (int)sizeof(label), &menu->entries[index]);
        if (menu->entries[index].kind == FS_KIND_DIR) {
            color = COL_ACCENT;
        } else if (menu->entries[index].kind == FS_KIND_USE_THIS) {
            color = COL_OK;
        } else if (menu->entries[index].kind == FS_KIND_PARENT) {
            color = COL_TEXT_DIM;
        } else {
            color = COL_TEXT;
        }
        if (selected) {
            color = COL_TEXT;
        }
        gfx_text_clip(g, BR_X + 14, y + 2, BR_W - 34, label, color, 1);
    }

    if (menu->count == 0) {
        gfx_text(g, BR_X + 14, BR_LIST_Y + 4, "NOTHING HERE", COL_TEXT_DIM, 1);
    }

    draw_status(g, menu, BR_X + 10, BR_Y + BR_H - 32, BR_W - 20);
    gfx_text(g, BR_X + 10, BR_Y + BR_H - 16,
             "[UP/DOWN] MOVE   [X] SELECT   [N] NEW FOLDER   [Z] BACK",
             COL_TEXT_DIM, 1);

    /* "N ENTRIES", bottom right */
    {
        char num[16];
        int wnum;
        int wtext;

        menu_itoa(num, menu->count);
        wnum = gfx_text_width(num, 1);
        wtext = gfx_text_width(" ENTRIES", 1);
        gfx_text(g, BR_X + BR_W - 10 - wtext - wnum, BR_Y + BR_H - 16, num,
                 COL_TEXT, 1);
        gfx_text(g, BR_X + BR_W - 10 - wtext, BR_Y + BR_H - 16, " ENTRIES",
                 COL_TEXT_DIM, 1);
    }
}

void menu_draw(Gfx *g, const Menu *menu)
{
    if (menu->mode == MENU_CLOSED) {
        return;
    }

    /* dim everything behind the modal */
    gfx_rect_alpha(g, 0, 0, SCREEN_W, SCREEN_H, 0x000000, 170);

    if (menu->mode == MENU_SETTINGS) {
        draw_settings(g, menu);
    } else {
        draw_browse(g, menu);
    }
}
