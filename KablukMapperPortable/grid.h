/*
 * Kabluk Mapper Portable - grid.h
 *
 * The square editor grid: a fixed 64x64 cell map, a cursor, and a camera
 * (the first visible cell). Everything integer, fixed size, no allocation -
 * the PSP has 32 MB of RAM in total.
 */
#ifndef KMP_GRID_H
#define KMP_GRID_H

#include "config.h"
#include "gfx.h"

#define CELL_EMPTY 0
#define CELL_SOLID 1

typedef struct Grid {
    unsigned char cells[MAP_ROWS][MAP_COLS];
    int cursor_x; /* cell under the cursor, map coords */
    int cursor_y;
    int cam_x;    /* first visible cell, map coords */
    int cam_y;
} Grid;

void grid_init(Grid *grid);
void grid_clear_map(Grid *grid);

/* Cell access, coordinates are clamped to the map. */
void grid_set_cell(Grid *grid, int cx, int cy, unsigned char value);
unsigned char grid_get_cell(const Grid *grid, int cx, int cy);

/* Cursor */
void grid_move_cursor(Grid *grid, int dx, int dy);
void grid_set_cursor(Grid *grid, int cx, int cy);
void grid_keep_cursor_visible(Grid *grid);
void grid_clamp_camera(Grid *grid);

/* Cursor cell -> screen pixel (top-left corner of the cell) */
void grid_cell_to_screen(const Grid *grid, int cx, int cy, int *sx, int *sy);

/* Screen pixel -> map cell. Returns 1 if the point is over the grid area. */
int grid_screen_to_cell(const Grid *grid, int mx, int my, int *cx, int *cy);

void grid_draw(Gfx *g, const Grid *grid);

#endif /* KMP_GRID_H */
