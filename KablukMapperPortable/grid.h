/*
 * Kabluk Mapper Portable - grid.h
 *
 * The square editor grid: a fixed 64x64 cell map, a cursor, and a camera
 * (the first visible cell). Everything integer, fixed size, no allocation -
 * the PSP has 32 MB of RAM in total.
 *
 * One cell is one 128x128x128 unit block. The outer ring of the map is the
 * "shell" (floor, ceiling and walls) and is solid from the start, so a map
 * can never be open to the void - the eraser refuses to remove it.
 */
#ifndef KMP_GRID_H
#define KMP_GRID_H

#include "config.h"
#include "gfx.h"
#include "layout.h"

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
void grid_clear_map(Grid *grid);   /* empty map + fresh shell */
void grid_build_shell(Grid *grid); /* solid border ring: floor/ceiling/walls */

int grid_is_shell(int cx, int cy);
int grid_shell_cells(void);

/* Cell access, coordinates are clamped to the map. Erasing a shell cell is
 * ignored - that is what keeps the map sealed. */
void grid_set_cell(Grid *grid, int cx, int cy, unsigned char value);
unsigned char grid_get_cell(const Grid *grid, int cx, int cy);

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

void grid_draw(Gfx *g, const Grid *grid, const Layout *layout);

#endif /* KMP_GRID_H */
