/*
 * Kabluk Mapper Portable - picker.c
 *
 * The texture picker is a grid of square cells, nine across and four down,
 * each one a 40 px thumbnail with the texture name under it. The old version
 * listed textures in 14 px rows with an 11 px picture, which read as a column
 * of grey specks: the mip it asked for was an 8x8 one and it was stretched
 * into a square, so both the size and the shape were wrong.
 */
#include "picker.h"

#include "config.h"
#include "keys.h"

/* The popup covers nearly the whole screen: this is a modal choice and the
 * cells are meant to be big enough to tell textures apart at arm's length. */
#define PK_X 8
#define PK_Y 8
#define PK_W (SCREEN_W - PK_X * 2) /* 464 */
#define PK_H (SCREEN_H - PK_Y * 2) /* 256 */
#define PK_HEAD 22
#define PK_FOOT 22

/* texture grid */
#define PK_CELL_W 48
#define PK_CELL_H 52
#define PK_COLS 9
#define PK_ROWS 4
#define PK_PAGE (PK_COLS * PK_ROWS) /* 36 thumbnails per page */
#define PK_THUMB 40
#define PK_GRID_X (PK_X + (PK_W - PK_COLS * PK_CELL_W) / 2)
#define PK_GRID_Y (PK_Y + PK_HEAD + 2)

/* entity list */
#define PK_ROW_H 14
#define PK_ROW_Y (PK_Y + PK_HEAD + 2)

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

/* keeps the page aligned to whole rows, so a row never half disappears */
static void picker_grid_scroll(Picker *picker)
{
    int row = picker->cur / PK_COLS;
    int top_row = picker->top / PK_COLS;

    if (row < top_row) {
        top_row = row;
    }
    if (row >= top_row + PK_ROWS) {
        top_row = row - PK_ROWS + 1;
    }
    if (top_row < 0) {
        top_row = 0;
    }
    picker->top = top_row * PK_COLS;
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
    picker_grid_scroll(picker);
}

/* the entity list moves one row at a time */
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

/* the texture grid moves by cells and keeps its shape at the edges */
static void picker_grid_move(Picker *picker, int dx, int dy)
{
    int col;
    int row;
    int index;

    if (picker->count <= 0) {
        return;
    }
    col = (picker->cur % PK_COLS) + dx;
    row = (picker->cur / PK_COLS) + dy;
    col = picker_clamp(col, 0, PK_COLS - 1);
    if (row < 0) {
        row = 0;
    }
    index = row * PK_COLS + col;
    if (index >= picker->count) {
        index = picker->count - 1; /* short last row */
    }
    picker->cur = index;
    picker_grid_scroll(picker);
}

static void picker_step(Picker *picker, int dx, int dy)
{
    if (picker->kind == PICK_TEXTURE) {
        picker_grid_move(picker, dx, dy);
    } else if (dy != 0) {
        picker_move(picker, dy);
    }
}

static int picker_hit_entity(const Picker *picker, int mx, int my)
{
    int index;

    if (mx < PK_X + 8 || mx >= PK_X + PK_W - 8) {
        return -1;
    }
    if (my < PK_ROW_Y - 3) {
        return -1;
    }
    index = (my - (PK_ROW_Y - 3)) / PK_ROW_H;
    if (index < 0 || index >= PICKER_VISIBLE) {
        return -1;
    }
    index += picker->top;
    if (index >= picker->count) {
        return -1;
    }
    return index;
}

static int picker_hit_texture(const Picker *picker, int mx, int my)
{
    int col;
    int row;
    int index;

    if (mx < PK_GRID_X || my < PK_GRID_Y) {
        return -1;
    }
    col = (mx - PK_GRID_X) / PK_CELL_W;
    row = (my - PK_GRID_Y) / PK_CELL_H;
    if (col < 0 || col >= PK_COLS || row < 0 || row >= PK_ROWS) {
        return -1;
    }
    index = picker->top + row * PK_COLS + col;
    if (index < 0 || index >= picker->count) {
        return -1;
    }
    return index;
}

static int picker_hit(const Picker *picker, int mx, int my)
{
    if (picker->kind == PICK_TEXTURE) {
        return picker_hit_texture(picker, mx, my);
    }
    return picker_hit_entity(picker, mx, my);
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
            picker_step(picker, 0, -1);
            return PICKER_EVENT_NONE;
        case KEY_DOWN:
            picker_step(picker, 0, 1);
            return PICKER_EVENT_NONE;
        case KEY_LEFT:
            picker_step(picker, -1, 0);
            return PICKER_EVENT_NONE;
        case KEY_RIGHT:
            picker_step(picker, 1, 0);
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
        if (picker->kind == PICK_TEXTURE) {
            picker_grid_move(picker, 0, event->wheel.y > 0 ? -1 : 1);
        } else {
            picker_move(picker, event->wheel.y > 0 ? -1 : 1);
        }
        return PICKER_EVENT_NONE;

    default:
        return PICKER_EVENT_NONE;
    }
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void picker_draw_frame(Gfx *g, const Picker *picker)
{
    gfx_rect_alpha(g, 0, 0, SCREEN_W, SCREEN_H, 0x000000, 170);

    gfx_rect(g, PK_X, PK_Y, PK_W, PK_H, COL_PANEL);
    gfx_outline(g, PK_X, PK_Y, PK_W, PK_H, COL_ACCENT);
    gfx_rect(g, PK_X + 1, PK_Y + 1, PK_W - 2, PK_HEAD - 2, COL_BTN);

    gfx_text(g, PK_X + 10, PK_Y + 7,
             picker->kind == PICK_ENTITY ? "POINT ENTITY" : "TEXTURE", COL_ACCENT, 1);
    gfx_text_right(g, PK_X + PK_W - 10, PK_Y + 7, "[X] OK  [Z] CANCEL", COL_TEXT_DIM, 1);

    /* footer */
    gfx_rect(g, PK_X + 1, PK_Y + PK_H - PK_FOOT, PK_W - 2, PK_FOOT - 1, COL_LIST_INFO);
}

/* the "12/76" counter in the bottom right corner */
static void picker_draw_count(Gfx *g, const Picker *picker)
{
    char num[12];
    char total[12];
    int value = picker->count > 0 ? picker->cur + 1 : 0;
    int right = PK_X + PK_W - 10;
    int y = PK_Y + PK_H - 15;
    int total_w;
    int slash_x;

    sprintf(num, "%d", value);
    sprintf(total, "%d", picker->count);

    total_w = gfx_text_width(total, 1);
    slash_x = right - total_w - gfx_text_width("/", 1) - 2;

    gfx_text_right(g, right, y, total, COL_TEXT_DIM, 1);
    gfx_text(g, slash_x, y, "/", COL_TEXT_DIM, 1);
    gfx_text_right(g, slash_x - 4, y, num, COL_TEXT, 1);
}

static void picker_draw_entities(Gfx *g, const Picker *picker, const Fgd *fgd)
{
    int i;
    int y;

    for (i = 0; i < PICKER_VISIBLE; i++) {
        int index = picker->top + i;
        int selected;
        unsigned int color;

        if (index >= picker->count) {
            break;
        }
        y = PK_ROW_Y + i * PK_ROW_H;
        selected = (index == picker->cur);
        if (selected) {
            gfx_rect(g, PK_X + 8, y - 1, PK_W - 16, PK_ROW_H - 1, COL_ROW_SEL);
        } else if (index == picker->hover) {
            gfx_rect(g, PK_X + 8, y - 1, PK_W - 16, PK_ROW_H - 1, COL_BTN_HOVER);
        }
        color = selected ? COL_TEXT : COL_ACCENT;
        gfx_text_clip(g, PK_X + 14, y + 2, PK_W - 30,
                      fgd->classes[picker->index[index]].name, color, 1);
    }

    if (picker->count == 0) {
        gfx_text(g, PK_X + 14, PK_ROW_Y + 4, "NO ENTITIES IN THE FGD", COL_WARN, 1);
        return;
    }
    gfx_text_clip(g, PK_X + 10, PK_Y + PK_H - 15, PK_W - 70,
                  fgd->classes[picker->index[picker->cur]].desc, COL_TEXT_DIM, 1);
}

static void picker_draw_textures(Gfx *g, const Picker *picker, Textures *textures,
                                 const Wad *wad)
{
    int i;
    int x;
    int y;
    unsigned int color;
    const char *info;

    for (i = 0; i < PK_PAGE; i++) {
        int index = picker->top + i;
        int selected;
        SDL_Texture *texture;

        if (index >= picker->count) {
            break;
        }
        x = PK_GRID_X + (i % PK_COLS) * PK_CELL_W;
        y = PK_GRID_Y + (i / PK_COLS) * PK_CELL_H;
        selected = (index == picker->cur);

        if (selected) {
            gfx_rect(g, x, y, PK_CELL_W - 2, PK_CELL_H - 2, COL_ROW_SEL);
        } else if (index == picker->hover) {
            gfx_rect(g, x, y, PK_CELL_W - 2, PK_CELL_H - 2, COL_BTN_HOVER);
        }

        texture = textures_thumb(g, textures, wad, index, PK_THUMB);
        if (texture != NULL) {
            gfx_blit_fit(g, texture, x + 4, y + 2, PK_THUMB, PK_THUMB);
        } else {
            gfx_rect(g, x + 4, y + 2, PK_THUMB, PK_THUMB, COL_SOLID);
        }
        gfx_outline(g, x + 3, y + 1, PK_THUMB + 2, PK_THUMB + 2,
                    selected ? COL_ACCENT : COL_BTN_EDGE);

        color = selected ? COL_TEXT : COL_TEXT_DIM;
        gfx_text_clip(g, x + 2, y + PK_THUMB + 5, PK_CELL_W - 4, wad->names[index],
                      color, 1);
    }

    if (picker->count == 0) {
        gfx_text(g, PK_GRID_X, PK_GRID_Y + 4, "NO TEXTURES: PICK A WAD IN SETTINGS",
                 COL_WARN, 1);
        return;
    }

    /* the name is under the picture already, so the footer is used to say
     * when there is no palette - that is what makes textures look wrong */
    if (!wad->has_palette) {
        info = "NO PALETTE.LMP NEXT TO THE WAD - TEXTURES ARE GREY";
    } else {
        info = wad->names[picker->cur];
    }
    gfx_text_clip(g, PK_X + 10, PK_Y + PK_H - 15, PK_W - 80, info,
                  wad->has_palette ? COL_TEXT_DIM : COL_WARN, 1);
}

void picker_draw(Gfx *g, const Picker *picker, const Fgd *fgd, Textures *textures,
                 const Wad *wad)
{
    if (!picker->open) {
        return;
    }

    picker_draw_frame(g, picker);
    if (picker->kind == PICK_TEXTURE) {
        picker_draw_textures(g, picker, textures, wad);
    } else {
        picker_draw_entities(g, picker, fgd);
    }
    picker_draw_count(g, picker);
}
