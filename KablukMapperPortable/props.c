/*
 * Kabluk Mapper Portable - props.c
 *
 * Read-only for now: it shows the map rules (block size, floor/ceiling,
 * leak check), the cursor position in cells and Quake units, and where the
 * map will be compiled to. Entity editing will hang off the same panel.
 */
#include "props.h"

#include "config.h"
#include "fs.h"

#define PROPS_X (SCREEN_W - RIGHT_PANEL_W)
#define PAD 8
#define ROW_H 12
#define CONTENT_W (RIGHT_PANEL_W - PAD * 2)

static void props_row(Gfx *g, int y, const char *label, const char *value,
                      unsigned int value_color)
{
    gfx_text(g, PROPS_X + PAD, y, label, COL_TEXT_DIM, 1);
    gfx_text_right(g, PROPS_X + RIGHT_PANEL_W - PAD, y, value, value_color, 1);
}

static void props_title(Gfx *g, int y, const char *title)
{
    gfx_text(g, PROPS_X + PAD, y, title, COL_ACCENT, 1);
}

static void props_separator(Gfx *g, int y)
{
    gfx_rect(g, PROPS_X + PAD, y, CONTENT_W, 1, COL_SEPARATOR);
}

/* "12:34" without stdio */
static void props_pair(char *out, int a, int b)
{
    int n = 0;
    int i;
    int digits;
    int value;
    int started = 0;

    for (i = 0; i < 2; i++) {
        value = (i == 0) ? a : b;
        if (i == 1) {
            out[n++] = ':';
        }
        if (value < 0) {
            out[n++] = '-';
            value = -value;
        }
        if (value == 0) {
            out[n++] = '0';
            started = 1;
        }
        digits = 1;
        while (digits * 10 <= value && digits < 1000000) {
            digits *= 10;
        }
        while (digits > 0) {
            if (value / digits != 0) {
                started = 1;
            }
            if (started) {
                out[n++] = (char)('0' + (value / digits) % 10);
            }
            digits /= 10;
        }
    }
    out[n] = '\0';
}

void props_draw(Gfx *g, const Grid *grid, const Settings *settings)
{
    int y;
    const char *out_dir;
    char buf[24];

    gfx_rect(g, PROPS_X, 0, RIGHT_PANEL_W, SCREEN_H, COL_PANEL);
    gfx_rect(g, PROPS_X, 0, 2, SCREEN_H, COL_PANEL_EDGE);

    props_title(g, 10, "MAP");

    y = 26;
    props_row(g, y, "FLOOR", MAP_DEFAULT_FLOOR ? "ON" : "OFF", COL_OK);
    y += ROW_H;
    props_row(g, y, "CEILING", MAP_DEFAULT_CEILING ? "ON" : "OFF", COL_OK);
    y += ROW_H;
    props_row(g, y, "LEAK CHK", "ON", COL_OK);
    y += ROW_H;
    props_row(g, y, "BLOCK", "128 UNITS", COL_TEXT);
    y += ROW_H;
    props_row(g, y, "MAP", "64 X 64", COL_TEXT);
    y += ROW_H;

    props_pair(buf, grid->cursor_x, grid->cursor_y);
    props_row(g, y, "CURSOR", buf, COL_TEXT);
    y += ROW_H;

    props_pair(buf, grid->cursor_x * BLOCK_UNITS, grid->cursor_y * BLOCK_UNITS);
    props_row(g, y, "UNITS", buf, COL_TEXT);

    y += ROW_H + 6;
    props_separator(g, y);

    y += 10;
    props_title(g, y, "ENTITY");
    gfx_text(g, PROPS_X + PAD, y + 14, "NONE SELECTED", COL_TEXT_DIM, 1);
    gfx_text(g, PROPS_X + PAD, y + 26, "PICK A CELL IN", COL_TEXT_DIM, 1);
    gfx_text(g, PROPS_X + PAD, y + 38, "THE GRID", COL_TEXT_DIM, 1);

    y += 54;
    props_separator(g, y);

    y += 10;
    props_title(g, y, "BUILD");
    out_dir = settings->out_dir[0] != '\0' ? settings->out_dir : ".";
    gfx_text(g, PROPS_X + PAD, y + 14, "OUT", COL_TEXT_DIM, 1);
    gfx_text_clip(g, PROPS_X + PAD, y + 26, CONTENT_W, out_dir, COL_TEXT, 1);
    gfx_text(g, PROPS_X + PAD, y + 40, "WAD", COL_TEXT_DIM, 1);
    gfx_text_clip(g, PROPS_X + PAD, y + 52, CONTENT_W,
                  settings_wad_is_set(settings) ? fs_basename(settings->wad_path)
                                               : "( NO WAD )",
                  COL_TEXT, 1);

    gfx_text(g, PROPS_X + PAD, SCREEN_H - 12, "[R] HIDE", COL_TEXT_DIM, 1);
}
