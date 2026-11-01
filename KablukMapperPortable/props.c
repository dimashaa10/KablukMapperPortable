/*
 * Kabluk Mapper Portable - props.c
 *
 * Draws the property list of the class the selection points at:
 *   - nothing selected -> worldspawn
 *   - a point entity selected with the SELECT tool -> that class
 * Values shown are the fgd defaults; per entity values come with the editor
 * for them (that is the next step).
 */
#include "props.h"

#include "config.h"

#define PROPS_X (SCREEN_W - RIGHT_PANEL_W)
#define PAD 8
#define CONTENT_W (RIGHT_PANEL_W - PAD * 2)

#define ROWS_Y 62
#define ROW_H 11
#define ROWS_VISIBLE ((SCREEN_H - 26 - ROWS_Y) / ROW_H)

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
}

void props_scroll_by(Fgd *fgd, PropsView *view, int delta)
{
    int max_scroll = fgd->prop_count - ROWS_VISIBLE;

    if (max_scroll < 0) {
        max_scroll = 0;
    }
    view->scroll = props_clamp(view->scroll + delta, 0, max_scroll);
}

void props_draw(Gfx *g, const Grid *grid, Fgd *fgd, PropsView *view)
{
    int class_index;
    const FgdClass *cls;
    const char *name;
    const char *subtitle;
    int y;
    int i;
    int max_scroll;

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
    }

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

        if (y + ROW_H > SCREEN_H - 24) {
            break;
        }
        gfx_text_clip(g, PROPS_X + PAD, y, CONTENT_W - 44, prop->key, COL_TEXT_DIM, 1);
        if (prop->value[0] != '\0') {
            gfx_text_right(g, PROPS_X + RIGHT_PANEL_W - PAD, y, prop->value, COL_TEXT, 1);
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

    gfx_text(g, PROPS_X + PAD, SCREEN_H - 14, "[R] HIDE", COL_TEXT_DIM, 1);
}
