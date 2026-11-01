/*
 * Kabluk Mapper Portable - picker.c
 */
#include "picker.h"

#include "config.h"
#include "keys.h"

#define PK_X 40
#define PK_Y 26
#define PK_W (SCREEN_W - PK_X * 2) /* 400 */
#define PK_H 224
#define PK_LIST_Y (PK_Y + 26)
#define PK_ROW_H 14
#define PK_ROW_TOP (PK_LIST_Y - 3)
#define PICKER_THUMB 11

static int picker_clamp(int v, int lo, int hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

void picker_init(Picker *picker)
{
    picker->open = 0;
    picker->kind = PICK_ENTITY;
    picker->target = PICK_TARGET_BLOCK;
    picker->count = 0;
    picker->cur = 0;
    picker->top = 0;
    picker->hover = -1;
}

int picker_is_open(const Picker *picker)
{
    return picker->open;
}

void picker_close(Picker *picker)
{
    picker->open = 0;
    picker->hover = -1;
}

static void picker_reset_view(Picker *picker)
{
    picker->open = 1;
    picker->hover = -1;
}

void picker_open_entities(Picker *picker, const Fgd *fgd, int preselect)
{
    int i;

    picker_reset_view(picker);
    picker->kind = PICK_ENTITY;
    picker->count = fgd_point_list(fgd, picker->index, FGD_MAX_CLASSES);

    picker->cur = 0;
    for (i = 0; i < picker->count; i++) {
        if (picker->index[i] == preselect) {
            picker->cur = i;
            break;
        }
    }
    picker->top = 0;
    if (picker->cur >= PICKER_VISIBLE) {
        picker->top = picker->cur - PICKER_VISIBLE + 1;
    }
}

void picker_open_textures(Picker *picker, const Wad *wad, int preselect)
{
    picker_reset_view(picker);
    picker->kind = PICK_TEXTURE;
    picker->count = wad->count;

    picker->cur = 0;
    if (preselect >= 0 && preselect < picker->count) {
        picker->cur = preselect;
    }
    picker->top = 0;
    if (picker->cur >= PICKER_VISIBLE) {
        picker->top = picker->cur - PICKER_VISIBLE + 1;
    }
}

static void picker_move(Picker *picker, int delta)
{
    if (picker->count <= 0) {
        return;
    }
    picker->cur = picker_clamp(picker->cur + delta, 0, picker->count - 1);
    if (picker->cur < picker->top) {
        picker->top = picker->cur;
    }
    if (picker->cur >= picker->top + PICKER_VISIBLE) {
        picker->top = picker->cur - PICKER_VISIBLE + 1;
    }
}

static int picker_hit(const Picker *picker, int mx, int my)
{
    int index;

    if (mx < PK_X + 8 || mx >= PK_X + PK_W - 8) {
        return -1;
    }
    if (my < PK_ROW_TOP) {
        return -1;
    }
    index = (my - PK_ROW_TOP) / PK_ROW_H;
    if (index < 0 || index >= PICKER_VISIBLE) {
        return -1;
    }
    index += picker->top;
    if (index >= picker->count) {
        return -1;
    }
    return index;
}

int picker_handle_event(Picker *picker, const SDL_Event *event)
{
    int hit;

    if (!picker->open) {
        return PICKER_EVENT_NONE;
    }

    switch (event->type) {
    case SDL_KEYDOWN:
        switch (event->key.keysym.sym) {
        case KEY_UP:
            picker_move(picker, -1);
            return PICKER_EVENT_NONE;
        case KEY_DOWN:
            picker_move(picker, 1);
            return PICKER_EVENT_NONE;
        case KEY_CONFIRM:
        case KEY_OPEN_SETTINGS:
            return picker->count > 0 ? PICKER_EVENT_CONFIRM : PICKER_EVENT_NONE;
        case KEY_CANCEL:
        case KEY_QUIT:
            return PICKER_EVENT_CANCEL;
        default:
            return PICKER_EVENT_NONE;
        }

    case SDL_MOUSEMOTION:
        picker->hover = picker_hit(picker, event->motion.x, event->motion.y);
        return PICKER_EVENT_NONE;

    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button != SDL_BUTTON_LEFT) {
            return PICKER_EVENT_CANCEL;
        }
        hit = picker_hit(picker, event->button.x, event->button.y);
        if (hit < 0) {
            return PICKER_EVENT_CANCEL; /* clicking outside closes */
        }
        picker->cur = hit;
        return PICKER_EVENT_CONFIRM;

    case SDL_MOUSEWHEEL:
        picker_move(picker, event->wheel.y > 0 ? -1 : 1);
        return PICKER_EVENT_NONE;

    default:
        return PICKER_EVENT_NONE;
    }
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void picker_draw_row(Gfx *g, const Picker *picker, int row, int y,
                            const Fgd *fgd, Textures *textures, const Wad *wad)
{
    int index = picker->top + row;
    int selected = (index == picker->cur);
    unsigned int color;
    const char *label;

    if (index >= picker->count) {
        return;
    }
    if (selected) {
        gfx_rect(g, PK_X + 8, y - 1, PK_W - 16, PK_ROW_H - 1, COL_ROW_SEL);
    } else if (index == picker->hover) {
        gfx_rect(g, PK_X + 8, y - 1, PK_W - 16, PK_ROW_H - 1, COL_BTN_HOVER);
    }

    if (picker->kind == PICK_ENTITY) {
        label = fgd->classes[picker->index[index]].name;
        color = selected ? COL_TEXT : COL_ACCENT;
        gfx_text_clip(g, PK_X + 14, y + 2, PK_W - 30, label, color, 1);
    } else {
        SDL_Texture *texture = textures_get(g, textures, wad, index);

        label = wad->names[index];
        color = selected ? COL_TEXT : COL_ACCENT;
        if (texture != NULL) {
            gfx_blit(g, texture, PK_X + 14, y + 1, PICKER_THUMB, PICKER_THUMB);
        } else {
            gfx_rect(g, PK_X + 14, y + 1, PICKER_THUMB, PICKER_THUMB, COL_SOLID);
        }
        gfx_outline(g, PK_X + 13, y, PICKER_THUMB + 2, PICKER_THUMB + 2, COL_BTN_EDGE);
        gfx_text_clip(g, PK_X + 14 + PICKER_THUMB + 8, y + 2, PK_W - 50, label, color, 1);
    }
}

void picker_draw(Gfx *g, const Picker *picker, const Fgd *fgd, Textures *textures,
                 const Wad *wad)
{
    int i;
    int y;
    char num[16];
    const char *info = "";

    if (!picker->open) {
        return;
    }

    gfx_rect_alpha(g, 0, 0, SCREEN_W, SCREEN_H, 0x000000, 170);

    gfx_rect(g, PK_X, PK_Y, PK_W, PK_H, COL_PANEL);
    gfx_outline(g, PK_X, PK_Y, PK_W, PK_H, COL_ACCENT);
    gfx_rect(g, PK_X + 1, PK_Y + 1, PK_W - 2, 20, COL_BTN);

    gfx_text(g, PK_X + 10, PK_Y + 7,
             picker->kind == PICK_ENTITY ? "POINT ENTITY" : "TEXTURE", COL_ACCENT, 1);
    gfx_text_right(g, PK_X + PK_W - 10, PK_Y + 7, "[X] OK  [Z] CANCEL", COL_TEXT_DIM, 1);

    for (i = 0; i < PICKER_VISIBLE; i++) {
        y = PK_ROW_TOP + i * PK_ROW_H;
        picker_draw_row(g, picker, i, y, fgd, textures, wad);
    }

    if (picker->count == 0) {
        gfx_text(g, PK_X + 14, PK_LIST_Y + 4,
                 picker->kind == PICK_ENTITY ? "NO ENTITIES IN THE FGD"
                                             : "NO TEXTURES: PICK A WAD IN SETTINGS",
                 COL_WARN, 1);
    }

    /* footer: description of the highlighted entity, or the texture counter */
    gfx_rect(g, PK_X + 1, PK_Y + PK_H - 22, PK_W - 2, 21, COL_LIST_INFO);
    if (picker->kind == PICK_ENTITY && picker->count > 0) {
        info = fgd->classes[picker->index[picker->cur]].desc;
    } else if (picker->kind == PICK_TEXTURE && picker->count > 0) {
        info = wad->names[picker->cur];
    }
    gfx_text_clip(g, PK_X + 10, PK_Y + PK_H - 15, PK_W - 70, info, COL_TEXT_DIM, 1);

    /* "12/76" counter */
    {
        int n = 0;
        int value = picker->cur + 1;
        int digits = 1;
        int wnum;
        int wofs;

        if (picker->count == 0) {
            value = 0;
        }
        while (digits * 10 <= value && digits < 1000000) {
            digits *= 10;
        }
        while (digits > 0) {
            num[n++] = (char)('0' + (value / digits) % 10);
            digits /= 10;
        }
        num[n] = '\0';
        wnum = gfx_text_width(num, 1);
        wofs = gfx_text_width("/00", 1);
        {
            char total[16];
            int m = 0;
            int v = picker->count;
            int d = 1;

            if (v == 0) {
                total[m++] = '0';
            }
            while (d * 10 <= v && d < 1000000) {
                d *= 10;
            }
            while (d > 0) {
                total[m++] = (char)('0' + (v / d) % 10);
                d /= 10;
            }
            total[m] = '\0';
            gfx_text(g, PK_X + PK_W - 10 - wofs - wnum, PK_Y + PK_H - 15, num,
                     COL_TEXT, 1);
            gfx_text(g, PK_X + PK_W - 10 - wofs, PK_Y + PK_H - 15, "/", COL_TEXT_DIM, 1);
            gfx_text(g, PK_X + PK_W - 10 - wofs + gfx_text_width("/", 1),
                     PK_Y + PK_H - 15, total, COL_TEXT_DIM, 1);
        }
    }
}
