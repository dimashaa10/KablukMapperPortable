/*
 * Kabluk Mapper Portable - ui.c
 *
 * The left panel: background, four tool buttons, simple vector icons.
 * Button captions are just names for now - the actual tools come later.
 */
#include "ui.h"

#include "config.h"

static const char *TOOL_NAMES[PANEL_BUTTONS] = {
    "SELECT",
    "BLOCK",
    "ENTITY",
    "TEXTURE"
};

const char *ui_tool_name(int index)
{
    if (index < 0 || index >= PANEL_BUTTONS) {
        return "";
    }
    return TOOL_NAMES[index];
}

void ui_clamp_tool(int *tool)
{
    if (*tool < 0) {
        *tool = 0;
    }
    if (*tool >= PANEL_BUTTONS) {
        *tool = PANEL_BUTTONS - 1;
    }
}

void ui_button_rect(int index, int *x, int *y, int *w, int *h)
{
    *x = PANEL_BTN_MARGIN;
    *y = PANEL_BTN_TOP + index * (PANEL_BTN_H + PANEL_BTN_GAP);
    *w = PANEL_BTN_W;
    *h = PANEL_BTN_H;
}

int ui_hit_test(int mx, int my)
{
    int i;
    int x;
    int y;
    int w;
    int h;

    for (i = 0; i < PANEL_BUTTONS; i++) {
        ui_button_rect(i, &x, &y, &w, &h);
        if (mx >= x && mx < x + w && my >= y && my < y + h) {
            return i;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */
/* Icons (drawn with plain rects, 20x16 box)                           */
/* ------------------------------------------------------------------ */
static void icon_select(Gfx *g, int x, int y, unsigned int rgb)
{
    /* crosshair */
    gfx_rect(g, x + 9, y + 1, 2, 14, rgb);
    gfx_rect(g, x + 3, y + 7, 14, 2, rgb);
    gfx_rect(g, x + 7, y + 5, 6, 6, COL_PANEL);
    gfx_outline(g, x + 7, y + 5, 6, 6, rgb);
}

static void icon_block(Gfx *g, int x, int y, unsigned int rgb)
{
    /* outlined square with a filled centre, like a brush */
    gfx_outline(g, x + 3, y + 1, 14, 14, rgb);
    gfx_rect(g, x + 7, y + 5, 6, 6, rgb);
}

static void icon_entity(Gfx *g, int x, int y, unsigned int rgb)
{
    /* diamond built from horizontal slices */
    gfx_rect(g, x + 9, y + 1, 2, 2, rgb);
    gfx_rect(g, x + 7, y + 3, 6, 2, rgb);
    gfx_rect(g, x + 5, y + 5, 10, 2, rgb);
    gfx_rect(g, x + 3, y + 7, 14, 2, rgb);
    gfx_rect(g, x + 5, y + 9, 10, 2, rgb);
    gfx_rect(g, x + 7, y + 11, 6, 2, rgb);
    gfx_rect(g, x + 9, y + 13, 2, 2, rgb);
}

static void icon_texture(Gfx *g, int x, int y, unsigned int rgb)
{
    /* 2x2 checkerboard */
    gfx_rect(g, x + 3, y + 1, 7, 7, rgb);
    gfx_rect(g, x + 10, y + 8, 7, 7, rgb);
    gfx_outline(g, x + 10, y + 1, 7, 7, rgb);
    gfx_outline(g, x + 3, y + 8, 7, 7, rgb);
}

static void ui_draw_icon(Gfx *g, int tool, int x, int y, unsigned int rgb)
{
    switch (tool) {
    case TOOL_SELECT:
        icon_select(g, x, y, rgb);
        break;
    case TOOL_BLOCK:
        icon_block(g, x, y, rgb);
        break;
    case TOOL_ENTITY:
        icon_entity(g, x, y, rgb);
        break;
    default:
        icon_texture(g, x, y, rgb);
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Panel                                                               */
/* ------------------------------------------------------------------ */
static void ui_draw_button(Gfx *g, int index, int active, int hover, int cursor)
{
    int x;
    int y;
    int w;
    int h;
    int cx;
    unsigned int icon_color;
    int label_w;

    ui_button_rect(index, &x, &y, &w, &h);

    gfx_rect(g, x, y, w, h, active ? COL_BTN_ACTIVE : (hover ? COL_BTN_HOVER : COL_BTN));
    gfx_outline(g, x, y, w, h, active ? COL_BTN_EDGE_ACTIVE : COL_BTN_EDGE);

    /* the row the arrows are on, while the panel has the pad */
    if (cursor) {
        gfx_outline(g, x - 1, y - 1, w + 2, h + 2, COL_ACCENT);
    }

    if (active) {
        /* small amber marker on the left edge */
        gfx_rect(g, x, y + 2, 2, h - 4, COL_ACCENT);
    }

    icon_color = active ? COL_ACCENT : COL_TEXT;
    cx = x + (w - 20) / 2;
    ui_draw_icon(g, index, cx, y + 10, icon_color);

    label_w = gfx_text_width(TOOL_NAMES[index], 1);
    gfx_text(g, x + (w - label_w) / 2, y + 34, TOOL_NAMES[index],
             active ? COL_ACCENT : COL_TEXT_DIM, 1);
}

/* 'cursor_tool' is the row the arrows sit on (the mouse uses it as the hover
 * row), and 'focused' says that the panel is the one holding the pad. */
void ui_draw_panel(Gfx *g, int active_tool, int cursor_tool, int focused)
{
    int i;

    gfx_rect(g, 0, 0, PANEL_W, SCREEN_H, COL_PANEL);
    gfx_rect(g, PANEL_W - 2, 0, 2, SCREEN_H,
             focused ? COL_ACCENT : COL_PANEL_EDGE);

    for (i = 0; i < PANEL_BUTTONS; i++) {
        ui_draw_button(g, i, i == active_tool, i == cursor_tool,
                       focused && i == cursor_tool && i != active_tool);
    }
}
