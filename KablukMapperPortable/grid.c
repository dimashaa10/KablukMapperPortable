/*
 * Kabluk Mapper Portable - grid.c
 *
 * Drawing and navigation of the editor grid.
 */
#include "grid.h"

static int clamp_int(int v, int lo, int hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

void grid_init(Grid *grid)
{
    int y;
    int x;

    for (y = 0; y < MAP_ROWS; y++) {
        for (x = 0; x < MAP_COLS; x++) {
            grid->cells[y][x] = CELL_EMPTY;
        }
    }
    grid->cursor_x = VIEW_COLS / 2;
    grid->cursor_y = VIEW_ROWS / 2;
    grid->cam_x = 0;
    grid->cam_y = 0;
}

void grid_clear_map(Grid *grid)
{
    int y;
    int x;

    for (y = 0; y < MAP_ROWS; y++) {
        for (x = 0; x < MAP_COLS; x++) {
            grid->cells[y][x] = CELL_EMPTY;
        }
    }
}

void grid_set_cell(Grid *grid, int cx, int cy, unsigned char value)
{
    if (cx < 0 || cx >= MAP_COLS || cy < 0 || cy >= MAP_ROWS) {
        return;
    }
    grid->cells[cy][cx] = value;
}

unsigned char grid_get_cell(const Grid *grid, int cx, int cy)
{
    if (cx < 0 || cx >= MAP_COLS || cy < 0 || cy >= MAP_ROWS) {
        return CELL_EMPTY;
    }
    return grid->cells[cy][cx];
}

void grid_clamp_camera(Grid *grid)
{
    int max_x = MAP_COLS - VIEW_COLS;
    int max_y = MAP_ROWS - VIEW_ROWS;

    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < 0) {
        max_y = 0;
    }
    grid->cam_x = clamp_int(grid->cam_x, 0, max_x);
    grid->cam_y = clamp_int(grid->cam_y, 0, max_y);
}

void grid_keep_cursor_visible(Grid *grid)
{
    if (grid->cursor_x < grid->cam_x) {
        grid->cam_x = grid->cursor_x;
    }
    if (grid->cursor_x >= grid->cam_x + VIEW_COLS) {
        grid->cam_x = grid->cursor_x - VIEW_COLS + 1;
    }
    if (grid->cursor_y < grid->cam_y) {
        grid->cam_y = grid->cursor_y;
    }
    if (grid->cursor_y >= grid->cam_y + VIEW_ROWS) {
        grid->cam_y = grid->cursor_y - VIEW_ROWS + 1;
    }
    grid_clamp_camera(grid);
}

void grid_set_cursor(Grid *grid, int cx, int cy)
{
    grid->cursor_x = clamp_int(cx, 0, MAP_COLS - 1);
    grid->cursor_y = clamp_int(cy, 0, MAP_ROWS - 1);
    grid_keep_cursor_visible(grid);
}

void grid_move_cursor(Grid *grid, int dx, int dy)
{
    grid_set_cursor(grid, grid->cursor_x + dx, grid->cursor_y + dy);
}

void grid_cell_to_screen(const Grid *grid, int cx, int cy, int *sx, int *sy)
{
    *sx = GRID_X + (cx - grid->cam_x) * CELL_SIZE;
    *sy = GRID_Y + (cy - grid->cam_y) * CELL_SIZE;
}

int grid_screen_to_cell(const Grid *grid, int mx, int my, int *cx, int *cy)
{
    if (mx < GRID_X || mx >= GRID_X + GRID_W || my < GRID_Y || my >= GRID_Y + GRID_H) {
        return 0;
    }
    *cx = grid->cam_x + (mx - GRID_X) / CELL_SIZE;
    *cy = grid->cam_y + (my - GRID_Y) / CELL_SIZE;
    if (*cx < 0) {
        *cx = 0;
    }
    if (*cx >= MAP_COLS) {
        *cx = MAP_COLS - 1;
    }
    if (*cy < 0) {
        *cy = 0;
    }
    if (*cy >= MAP_ROWS) {
        *cy = MAP_ROWS - 1;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void grid_draw_lines(Gfx *g, const Grid *grid)
{
    int i;
    int x;
    int y;
    int major;

    for (i = 0; i <= VIEW_COLS; i++) {
        x = GRID_X + i * CELL_SIZE;
        if (x >= GRID_X + GRID_W) {
            break;
        }
        major = ((grid->cam_x + i) % GRID_MAJOR_EVERY) == 0;
        gfx_set_color(g, major ? COL_GRID_MAJOR : COL_GRID_MINOR);
        gfx_draw_vline(g, x, GRID_Y, GRID_H);
    }
    for (i = 0; i <= VIEW_ROWS; i++) {
        y = GRID_Y + i * CELL_SIZE;
        if (y >= GRID_Y + GRID_H) {
            break;
        }
        major = ((grid->cam_y + i) % GRID_MAJOR_EVERY) == 0;
        gfx_set_color(g, major ? COL_GRID_MAJOR : COL_GRID_MINOR);
        gfx_draw_hline(g, GRID_X, y, GRID_W);
    }
}

static void grid_draw_cells(Gfx *g, const Grid *grid)
{
    int cx;
    int cy;
    int sx;
    int sy;
    int cx_end = grid->cam_x + VIEW_COLS;
    int cy_end = grid->cam_y + VIEW_ROWS;

    if (cx_end > MAP_COLS) {
        cx_end = MAP_COLS;
    }
    if (cy_end > MAP_ROWS) {
        cy_end = MAP_ROWS;
    }

    for (cy = grid->cam_y; cy < cy_end; cy++) {
        for (cx = grid->cam_x; cx < cx_end; cx++) {
            if (grid->cells[cy][cx] == CELL_EMPTY) {
                continue;
            }
            grid_cell_to_screen(grid, cx, cy, &sx, &sy);
            gfx_rect(g, sx + 1, sy + 1, CELL_SIZE - 2, CELL_SIZE - 2, COL_SOLID);
            gfx_outline(g, sx + 1, sy + 1, CELL_SIZE - 2, CELL_SIZE - 2,
                        COL_SOLID_EDGE);
        }
    }
}

static void grid_draw_map_edge(Gfx *g, const Grid *grid)
{
    int sx;
    int sy;

    /* Outline of the 64x64 map, drawn only if it is (partly) visible. */
    grid_cell_to_screen(grid, 0, 0, &sx, &sy);
    gfx_outline(g, sx, sy, MAP_COLS * CELL_SIZE, MAP_ROWS * CELL_SIZE, COL_MAP_EDGE);
}

static void grid_draw_cursor(Gfx *g, const Grid *grid)
{
    int sx;
    int sy;

    grid_cell_to_screen(grid, grid->cursor_x, grid->cursor_y, &sx, &sy);
    if (sx + CELL_SIZE < GRID_X || sx > GRID_X + GRID_W) {
        return;
    }
    if (sy + CELL_SIZE < GRID_Y || sy > GRID_Y + GRID_H) {
        return;
    }

    /* soft amber fill + 2 px frame so the cursor is obvious on the PSP LCD */
    gfx_rect_alpha(g, sx + 1, sy + 1, CELL_SIZE - 2, CELL_SIZE - 2, COL_CURSOR, 56);
    gfx_rect(g, sx, sy, CELL_SIZE, 2, COL_CURSOR);
    gfx_rect(g, sx, sy + CELL_SIZE - 2, CELL_SIZE, 2, COL_CURSOR);
    gfx_rect(g, sx, sy, 2, CELL_SIZE, COL_CURSOR);
    gfx_rect(g, sx + CELL_SIZE - 2, sy, 2, CELL_SIZE, COL_CURSOR);
}

static void grid_draw_status(Gfx *g, const Grid *grid)
{
    int ty = GRID_Y + 6;
    int w;
    int tx;

    /* tiny coordinate readout, top-right corner of the grid */
    w = gfx_text_width("X:00  Y:00", 1);
    tx = GRID_X + GRID_W - 4 - w;
    gfx_rect_alpha(g, tx - 4, ty - 3, w + 8, 13, COL_BG, 200);

    tx = gfx_text(g, tx, ty, "X:", COL_TEXT_DIM, 1);
    tx = gfx_text_int(g, tx, ty, grid->cursor_x, COL_TEXT, 1);
    tx = gfx_text(g, tx, ty, "  Y:", COL_TEXT_DIM, 1);
    gfx_text_int(g, tx, ty, grid->cursor_y, COL_TEXT, 1);
}

void grid_draw(Gfx *g, const Grid *grid)
{
    gfx_rect(g, GRID_X, GRID_Y, GRID_W, GRID_H, COL_BG);
    grid_draw_lines(g, grid);
    grid_draw_map_edge(g, grid);
    grid_draw_cells(g, grid);
    grid_draw_cursor(g, grid);
    grid_draw_status(g, grid);
}
