/*
 * Kabluk Mapper Portable - menu.c
 *
 * Editor settings window + the little file browser that picks a .wad file
 * or an output folder. Everything is drawn with the same gfx layer as the
 * rest of the editor, no widgets library.
 */
#include "menu.h"

#include "config.h"
#include "fs.h"
#include "keys.h"

/* ------------------------------------------------------------------ */
/* Settings window geometry                                            */
/* ------------------------------------------------------------------ */
#define SET_X 36
#define SET_Y 34
#define SET_W (SCREEN_W - SET_X * 2) /* 408 */
#define SET_H 204
#define SET_FIELD_H 20
#define SET_ROW0_Y (SET_Y + 46)
#define SET_ROW1_Y (SET_Y + 86)
#define SET_FIELD_X (SET_X + 12)
#define SET_FIELD_W (SET_W - 24)
#define SET_CLOSE_W 84
#define SET_CLOSE_H 22
#define SET_CLOSE_X (SET_X + SET_W - 12 - SET_CLOSE_W)
#define SET_CLOSE_Y (SET_Y + SET_H - 12 - SET_CLOSE_H)

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
void menu_init(Menu *menu, Settings *settings)
{
    menu->mode = MENU_CLOSED;
    menu->settings = settings;
    menu->sel = MENU_ROW_WAD;
    menu->hover = -1;
    menu->browse_target = BROWSE_WAD;
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
        menu->mode = MENU_SETTINGS;
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
        menu->mode = MENU_SETTINGS;
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
    if (menu_in_rect(mx, my, SET_CLOSE_X, SET_CLOSE_Y, SET_CLOSE_W, SET_CLOSE_H)) {
        return MENU_ROW_CLOSE;
    }
    return -1;
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
            menu_close(menu);
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
                if (menu->sel == MENU_ROW_WAD) {
                    browse_open(menu, BROWSE_WAD);
                } else if (menu->sel == MENU_ROW_OUT) {
                    browse_open(menu, BROWSE_DIR);
                } else {
                    menu_close(menu);
                }
            } else {
                browse_activate(menu, menu->cur);
            }
            return 1;
        case KEY_CANCEL:
            if (menu->mode == MENU_BROWSE) {
                menu->mode = MENU_SETTINGS;
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
            if (hit == MENU_ROW_WAD) {
                browse_open(menu, BROWSE_WAD);
            } else if (hit == MENU_ROW_OUT) {
                browse_open(menu, BROWSE_DIR);
            } else if (hit == MENU_ROW_CLOSE) {
                menu_close(menu);
            }
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
static void draw_field(Gfx *g, int y, const char *label, const char *value,
                       const char *empty_text, int selected)
{
    gfx_text(g, SET_FIELD_X, y - 11, label, COL_TEXT_DIM, 1);

    gfx_rect(g, SET_FIELD_X, y, SET_FIELD_W, SET_FIELD_H, COL_FIELD);
    gfx_outline(g, SET_FIELD_X, y, SET_FIELD_W, SET_FIELD_H,
                selected ? COL_ACCENT : COL_BTN_EDGE);

    if (value != NULL && value[0] != '\0') {
        gfx_text_clip(g, SET_FIELD_X + 6, y + 7, SET_FIELD_W - 40, value, COL_TEXT, 1);
    } else {
        gfx_text(g, SET_FIELD_X + 6, y + 7, empty_text, COL_TEXT_DIM, 1);
    }

    gfx_text_right(g, SET_FIELD_X + SET_FIELD_W - 6, y + 7, "BROWSE",
                   selected ? COL_ACCENT : COL_TEXT_DIM, 1);
}

static void draw_settings(Gfx *g, const Menu *menu)
{
    int close_hover;
    int label_w;

    gfx_rect(g, SET_X, SET_Y, SET_W, SET_H, COL_PANEL);
    gfx_outline(g, SET_X, SET_Y, SET_W, SET_H, COL_ACCENT);

    gfx_rect(g, SET_X + 1, SET_Y + 1, SET_W - 2, 22, COL_BTN);
    gfx_text(g, SET_X + 10, SET_Y + 8, "EDITOR SETTINGS", COL_ACCENT, 1);
    gfx_text_right(g, SET_X + SET_W - 10, SET_Y + 8, "[SELECT] CLOSE", COL_TEXT_DIM, 1);

    draw_field(g, SET_ROW0_Y, "WAD FILE  (TEXTURES)", menu->settings->wad_path,
               "( NOT SET )", menu->sel == MENU_ROW_WAD);
    draw_field(g, SET_ROW1_Y, "COMPILED MAPS GO TO", menu->settings->out_dir,
               "CURRENT FOLDER", menu->sel == MENU_ROW_OUT);

    gfx_text(g, SET_FIELD_X, SET_ROW1_Y + SET_FIELD_H + 12,
             "THE WAD PROVIDES TEXTURES AND PALETTE,", COL_TEXT_DIM, 1);
    gfx_text(g, SET_FIELD_X, SET_ROW1_Y + SET_FIELD_H + 24,
             "COMPILED MAPS ARE WRITTEN TO THE FOLDER.", COL_TEXT_DIM, 1);

    gfx_text(g, SET_FIELD_X, SET_CLOSE_Y + 7, "[X] BROWSE", COL_TEXT_DIM, 1);

    close_hover = (menu->sel == MENU_ROW_CLOSE) || (menu->hover == MENU_ROW_CLOSE);
    gfx_rect(g, SET_CLOSE_X, SET_CLOSE_Y, SET_CLOSE_W, SET_CLOSE_H,
             close_hover ? COL_BTN_ACTIVE : COL_BTN);
    gfx_outline(g, SET_CLOSE_X, SET_CLOSE_Y, SET_CLOSE_W, SET_CLOSE_H,
                close_hover ? COL_BTN_EDGE_ACTIVE : COL_BTN_EDGE);
    label_w = gfx_text_width("CLOSE", 1);
    gfx_text(g, SET_CLOSE_X + (SET_CLOSE_W - label_w) / 2, SET_CLOSE_Y + 7, "CLOSE",
             close_hover ? COL_ACCENT : COL_TEXT, 1);
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

    /* "N ENTRIES" counter, top right */
    {
        char num[16];
        int wnum;
        int wtext;

        menu_itoa(num, menu->count);
        wnum = gfx_text_width(num, 1);
        wtext = gfx_text_width(" ENTRIES", 1);
        gfx_text(g, BR_X + BR_W - 10 - wtext - wnum, BR_Y + 8, num, COL_TEXT, 1);
        gfx_text(g, BR_X + BR_W - 10 - wtext, BR_Y + 8, " ENTRIES", COL_TEXT_DIM, 1);
    }

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

    gfx_text(g, BR_X + 10, BR_Y + BR_H - 16, "[UP/DOWN] MOVE   [X] SELECT   [Z] BACK",
             COL_TEXT_DIM, 1);
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
