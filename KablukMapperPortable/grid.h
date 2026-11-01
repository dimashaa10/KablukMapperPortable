/*
 * Kabluk Mapper Portable - grid.h
 *
 * The square editor grid. One cell is one 128x128x128 unit block and can hold
 * a texture index and/or a point entity (index into the fgd class table).
 *
 * The outer ring of the map is the "shell" (floor, ceiling, walls) and is
 * solid from the start, so a map can never be open to the void - the eraser
 * refuses to remove it.
 */
#ifndef KMP_GRID_H
#define KMP_GRID_H

#include "config.h"
#include "fgd.h"
#include "gfx.h"
#include "layout.h"
#include "textures.h"
#include "wad.h"

#define CELL_EMPTY 0
#define CELL_SOLID 1

#define TEX_NONE 0xFFFF

typedef struct Grid {
    unsigned char solid[MAP_ROWS][MAP_COLS];
    unsigned short tex[MAP_ROWS][MAP_COLS];   /* texture index from the wad */
    unsigned char entity[MAP_ROWS][MAP_COLS]; /* fgd class index + 1        */

    int cursor_x; /* cell under the cursor, map coords */
    int cursor_y;
    int cam_x;    /* first visible cell, map coords */
    int cam_y;

    int sel_x; /* selected point entity, -1 = nothing selected */
    int sel_y;
} Grid;

void grid_init(Grid *grid);
void grid_clear_map(Grid *grid); /* empty map, no pre-placed walls */

/* 1 for the outermost ring of cells. Nothing is reserved there: the exporter
 * walls off the border cells the user left empty (see compile.c). */
int grid_is_border(int cx, int cy);

/* Blocks, free everywhere - the exporter adds the sealing walls. */
void grid_paint(Grid *grid, int cx, int cy, int tex_index);
void grid_set_cell(Grid *grid, int cx, int cy, unsigned char value);
void grid_erase(Grid *grid, int cx, int cy);
unsigned char grid_get_cell(const Grid *grid, int cx, int cy);
int grid_get_tex(const Grid *grid, int cx, int cy);

/* Point entities: class_index is an fgd class index, -1 removes. */
void grid_set_entity(Grid *grid, int cx, int cy, int class_index);
int grid_entity_at(const Grid *grid, int cx, int cy); /* -1 = none */

/* Selection used by the right panel. */
void grid_select(Grid *grid, int cx, int cy);
void grid_select_none(Grid *grid);
int grid_selected_class(const Grid *grid); /* -1 = worldspawn/nothing */

/* Cursor (needs the layout to know how many cells are visible) */
void grid_move_cursor(Grid *grid, const Layout *layout, int dx, int dy);
void grid_set_cursor(Grid *grid, const Layout *layout, int cx, int cy);
void grid_keep_cursor_visible(Grid *grid, const Layout *layout);
void grid_clamp_camera(Grid *grid, const Layout *layout);

/* Cursor cell -> screen pixel (top-left corner of the cell) */
void grid_cell_to_screen(const Grid *grid, int cx, int cy, int *sx, int *sy);

/* Screen pixel -> map cell. Returns 1 if the point is over the grid area. */
int grid_screen_to_cell(const Grid *grid, const Layout *layout, int mx, int my,
                        int *cx, int *cy);

void grid_draw(Gfx *g, const Grid *grid, const Layout *layout, Textures *textures,
               const Wad *wad);

#endif /* KMP_GRID_H */
