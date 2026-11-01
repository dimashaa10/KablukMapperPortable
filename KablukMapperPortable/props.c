/*
 * Kabluk Mapper Portable - props.c
 *
 * Draws the property list of the class the selection points at:
 *   - nothing selected -> worldspawn
 *   - a point entity selected with the SELECT tool -> that class
 * Values are the entity's own (typed in with the on screen keyboard) and fall
 * back to the fgd default, shown dimmed, when nothing was set yet.
 */
#include "props.h"

#include <string.h>

#include "config.h"

#define PROPS_X (SCREEN_W - RIGHT_PANEL_W)
#define PAD 8
#define CONTENT_W (RIGHT_PANEL_W - PAD * 2)

#define ROWS_Y 62
#define ROW_H 11
#define ROWS_VISIBLE ((SCREEN_H - 26 - ROWS_Y) / ROW_H)
#define ROWS_BOTTOM (SCREEN_H - 24)

static int props_clamp(int v, int lo, int hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

/* appends a non negative int to a string, returns the new length */
static int props_append_int(char *out, int len, int value)
{
    char tmp[12];
    int n = 0;
    int i;
    int digits = 1;

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
        out[len++] = tmp[i];
    }
    out[len] = '\0';
    return len;
}

void props_view_init(PropsView *view)
{
    view->scroll = 0;
    view->sel = -1;
    view->hover = -1;
}

int props_visible_rows(void)
{
    return ROWS_VISIBLE;
}

/* case insensitive compare, the fgd mixes "flags" and "Flags" */
static int props_streq_ci(const char *a, const char *b)
{
    int i;

    for (i = 0; a[i] != '\0' && b[i] != '\0'; i++) {
        char ca = a[i];
        char cb = b[i];

        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return 0;
        }
    }
    return a[i] == b[i];
}

int props_is_numeric(const FgdProp *prop)
{
    const char *type;

    if (prop == NULL) {
        return 0;
    }
    type = prop->type;
    /* the fgd types that hold nothing but a number */
    if (props_streq_ci(type, "integer") || props_streq_ci(type, "flags") ||
        props_streq_ci(type, "choices") || props_streq_ci(type, "shootable")) {
        return 1;
    }
    return 0;
}

void props_target_cell(const Grid *grid, int *cx, int *cy)
{
    if (grid->sel_x >= 0 && grid->sel_y >= 0) {
        *cx = grid->sel_x;
        *cy = grid->sel_y;
        return;
    }
    *cx = VALUES_WORLD_CX;
    *cy = VALUES_WORLD_CY;
}

const char *props_value(const FgdProp *prop, const ValueStore *values, int cx,
                        int cy)
{
    const char *set;

    if (prop == NULL) {
        return NULL;
    }
    set = values_get(values, cx, cy, prop->key);
    if (set != NULL) {
        return set;
    }
    /* "(list)" is what fgd.c stores for a choices/flags list, not a value */
    if (prop->value[0] != '\0' && strcmp(prop->value, "(list)") != 0) {
        return prop->value;
    }
    return NULL;
}

int props_hit_row(const Fgd *fgd, const PropsView *view, int mx, int my)
{
    int index;

    if (!fgd->loaded || fgd->prop_count <= 0) {
        return -1;
    }
    if (mx < PROPS_X + PAD || mx >= PROPS_X + RIGHT_PANEL_W - 4) {
        return -1;
    }
    if (my < ROWS_Y || my >= ROWS_BOTTOM) {
        return -1;
    }
    index = view->scroll + (my - ROWS_Y) / ROW_H;
    if (index < 0 || index >= fgd->prop_count) {
        return -1;
    }
    /* rows are only drawn while they fit above the hint line */
    if (ROWS_Y + (index - view->scroll) * ROW_H + ROW_H > ROWS_BOTTOM) {
        return -1;
    }
    return index;
}

void props_scroll_by(Fgd *fgd, PropsView *view, int delta)
{
    int max_scroll = fgd->prop_count - ROWS_VISIBLE;

    if (max_scroll < 0) {
        max_scroll = 0;
    }
    view->scroll = props_clamp(view->scroll + delta, 0, max_scroll);
}

void props_draw(Gfx *g, const Grid *grid, Fgd *fgd, PropsView *view,
                const ValueStore *values)
{
    int class_index;
    const FgdClass *cls;
    const char *name;
    const char *subtitle;
    int y;
    int i;
    int max_scroll;
    int cell_x;
    int cell_y;

    gfx_rect(g, PROPS_X, 0, RIGHT_PANEL_W, SCREEN_H, COL_PANEL);
    gfx_rect(g, PROPS_X, 0, 2, SCREEN_H, COL_PANEL_EDGE);

    if (!fgd->loaded) {
        gfx_text(g, PROPS_X + PAD, 12, "NO FGD", COL_WARN, 1);
        gfx_text(g, PROPS_X + PAD, 28, "PUT Quake.fgd", COL_TEXT_DIM, 1);
        gfx_text(g, PROPS_X + PAD, 40, "NEXT TO THE EXE", COL_TEXT_DIM, 1);
        return;
    }

    class_index = grid_selected_class(grid);
    subtitle = "WORLD";
    if (class_index < 0) {
        class_index = fgd_find(fgd, "worldspawn");
    } else {
        subtitle = "POINT ENTITY";
    }
    if (class_index < 0) {
        gfx_text(g, PROPS_X + PAD, 12, "NO WORLDSPAWN", COL_WARN, 1);
        return;
    }

    if (fgd->props_class != class_index) {
        fgd_resolve(fgd, class_index);
        view->scroll = 0;
        view->sel = -1;
        view->hover = -1;
    }
    props_target_cell(grid, &cell_x, &cell_y);

    cls = fgd_class(fgd, class_index);
    name = cls != NULL ? cls->name : "?";

    gfx_text_clip(g, PROPS_X + PAD, 10, CONTENT_W, name, COL_ACCENT, 1);
    gfx_text(g, PROPS_X + PAD, 22, subtitle, COL_TEXT_DIM, 1);

    /* cell of the selected entity, right under the subtitle */
    if (grid->sel_x >= 0) {
        char coord[16];
        int len = 0;

        coord[len++] = 'A';
        coord[len++] = 'T';
        coord[len++] = ' ';
        len = props_append_int(coord, len, grid->sel_x);
        coord[len++] = ':';
        coord[len] = '\0';
        props_append_int(coord, len, grid->sel_y);
        gfx_text_right(g, PROPS_X + RIGHT_PANEL_W - PAD, 22, coord, COL_TEXT_DIM, 1);
    }

    if (cls != NULL && cls->desc[0] != '\0') {
        gfx_text_clip(g, PROPS_X + PAD, 34, CONTENT_W, cls->desc, COL_TEXT_DIM, 1);
    }

    gfx_rect(g, PROPS_X + PAD, 46, CONTENT_W, 1, COL_SEPARATOR);

    max_scroll = fgd->prop_count - ROWS_VISIBLE;
    if (max_scroll < 0) {
        max_scroll = 0;
    }
    view->scroll = props_clamp(view->scroll, 0, max_scroll);

    y = ROWS_Y;
    for (i = view->scroll; i < fgd->prop_count; i++) {
        const FgdProp *prop = &fgd->props[i];
        const char *shown;
        int edited;

        if (y + ROW_H > ROWS_BOTTOM) {
            break;
        }
        edited = values_get(values, cell_x, cell_y, prop->key) != NULL;
        if (i == view->sel) {
            gfx_rect(g, PROPS_X + PAD - 3, y - 2, CONTENT_W + 6, ROW_H, COL_ROW_SEL);
        } else if (i == view->hover) {
            gfx_rect(g, PROPS_X + PAD - 3, y - 2, CONTENT_W + 6, ROW_H, COL_BTN_HOVER);
        }
        gfx_text_clip(g, PROPS_X + PAD, y, CONTENT_W - 44, prop->key,
                      edited ? COL_TEXT : COL_TEXT_DIM, 1);

        shown = props_value(prop, values, cell_x, cell_y);
        if (shown != NULL) {
            gfx_text_right(g, PROPS_X + RIGHT_PANEL_W - PAD, y, shown,
                           edited ? COL_ACCENT : COL_TEXT_DIM, 1);
        }
        y += ROW_H;
    }

    if (fgd->prop_count == 0) {
        gfx_text(g, PROPS_X + PAD, ROWS_Y, "NO PROPERTIES", COL_TEXT_DIM, 1);
    }

    /* scroll bar on the right edge of the panel */
    if (max_scroll > 0) {
        int track_h = ROWS_VISIBLE * ROW_H;
        int thumb_h = track_h * ROWS_VISIBLE / fgd->prop_count;
        int thumb_y;

        if (thumb_h < 8) {
            thumb_h = 8;
        }
        thumb_y = ROWS_Y + (track_h - thumb_h) * view->scroll / max_scroll;
        gfx_rect(g, PROPS_X + RIGHT_PANEL_W - 3, ROWS_Y, 2, track_h, COL_BTN);
        gfx_rect(g, PROPS_X + RIGHT_PANEL_W - 3, thumb_y, 2, thumb_h, COL_ACCENT);
    }

    gfx_text(g, PROPS_X + PAD, SCREEN_H - 14, "[X] EDIT", COL_TEXT_DIM, 1);
    gfx_text_right(g, PROPS_X + RIGHT_PANEL_W - PAD, SCREEN_H - 14, "[R] HIDE",
                   COL_TEXT_DIM, 1);
}
