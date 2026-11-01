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

int grid_is_shell(int cx, int cy)
{
    return (cx <= 0 || cy <= 0 || cx >= MAP_COLS - 1 || cy >= MAP_ROWS - 1);
}

int grid_shell_cells(void)
{
    return 2 * MAP_COLS + 2 * (MAP_ROWS - 2);
}

void grid_build_shell(Grid *grid)
{
    int x;
    int y;

    for (x = 0; x < MAP_COLS; x++) {
        grid->cells[0][x] = CELL_SOLID;
        grid->cells[MAP_ROWS - 1][x] = CELL_SOLID;
    }
    for (y = 0; y < MAP_ROWS; y++) {
        grid->cells[y][0] = CELL_SOLID;
        grid->cells[y][MAP_COLS - 1] = CELL_SOLID;
    }
}

void grid_clear_map(Grid *grid)
{
    int x;
    int y;

    for (y = 0; y < MAP_ROWS; y++) {
        for (x = 0; x < MAP_COLS; x++) {
            grid->cells[y][x] = CELL_EMPTY;
        }
    }
    grid_build_shell(grid);
}

void grid_init(Grid *grid)
{
    grid_clear_map(grid);
    grid->cursor_x = 4;
    grid->cursor_y = 4;
    grid->cam_x = 0;
    grid->cam_y = 0;
}

void grid_set_cell(Grid *grid, int cx, int cy, unsigned char value)
{
    if (cx < 0 || cx >= MAP_COLS || cy < 0 || cy >= MAP_ROWS) {
        return;
    }
    if (value == CELL_EMPTY && grid_is_shell(cx, cy)) {
        return; /* the shell stays: no holes into the void */
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

void grid_clamp_camera(Grid *grid, const Layout *layout)
{
    int max_x = MAP_COLS - layout->cols;
    int max_y = MAP_ROWS - layout->rows;

    if (max_x < 0) {
        max_x = 0;
    }
    if (max_y < 0) {
        max_y = 0;
    }
    grid->cam_x = clamp_int(grid->cam_x, 0, max_x);
    grid->cam_y = clamp_int(grid->cam_y, 0, max_y);
}

void grid_keep_cursor_visible(Grid *grid, const Layout *layout)
{
    if (grid->cursor_x < grid->cam_x) {
        grid->cam_x = grid->cursor_x;
    }
    if (grid->cursor_x >= grid->cam_x + layout->cols) {
        grid->cam_x = grid->cursor_x - layout->cols + 1;
    }
    if (grid->cursor_y < grid->cam_y) {
        grid->cam_y = grid->cursor_y;
    }
    if (grid->cursor_y >= grid->cam_y + layout->rows) {
        grid->cam_y = grid->cursor_y - layout->rows + 1;
    }
    grid_clamp_camera(grid, layout);
}

void grid_set_cursor(Grid *grid, const Layout *layout, int cx, int cy)
{
    grid->cursor_x = clamp_int(cx, 0, MAP_COLS - 1);
    grid->cursor_y = clamp_int(cy, 0, MAP_ROWS - 1);
    grid_keep_cursor_visible(grid, layout);
}

void grid_move_cursor(Grid *grid, const Layout *layout, int dx, int dy)
{
    grid_set_cursor(grid, layout, grid->cursor_x + dx, grid->cursor_y + dy);
}

void grid_cell_to_screen(const Grid *grid, int cx, int cy, int *sx, int *sy)
{
    *sx = (cx - grid->cam_x) * CELL_SIZE;
    *sy = (cy - grid->cam_y) * CELL_SIZE;
}

int grid_screen_to_cell(const Grid *grid, const Layout *layout, int mx, int my,
                        int *cx, int *cy)
{
    if (mx < layout->grid_x || mx >= layout->grid_x + layout->grid_w) {
        return 0;
    }
    if (my < layout->grid_y || my >= layout->grid_y + layout->grid_h) {
        return 0;
    }
    *cx = grid->cam_x + (mx - layout->grid_x) / CELL_SIZE;
    *cy = grid->cam_y + (my - layout->grid_y) / CELL_SIZE;
    *cx = clamp_int(*cx, 0, MAP_COLS - 1);
    *cy = clamp_int(*cy, 0, MAP_ROWS - 1);
    return 1;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void grid_draw_lines(Gfx *g, const Grid *grid, const Layout *layout)
{
    int i;
    int x;
    int y;
    int major;

    for (i = 0; i <= layout->cols; i++) {
        x = layout->grid_x + i * CELL_SIZE;
        if (x >= layout->grid_x + layout->grid_w) {
            break;
        }
        major = ((grid->cam_x + i) % GRID_MAJOR_EVERY) == 0;
        gfx_set_color(g, major ? COL_GRID_MAJOR : COL_GRID_MINOR);
        gfx_draw_vline(g, x, layout->grid_y, layout->grid_h);
    }
    for (i = 0; i <= layout->rows; i++) {
        y = layout->grid_y + i * CELL_SIZE;
        if (y >= layout->grid_y + layout->grid_h) {
            break;
        }
        major = ((grid->cam_y + i) % GRID_MAJOR_EVERY) == 0;
        gfx_set_color(g, major ? COL_GRID_MAJOR : COL_GRID_MINOR);
        gfx_draw_hline(g, layout->grid_x, y, layout->grid_w);
    }
}

static void grid_draw_cells(Gfx *g, const Grid *grid, const Layout *layout)
{
    int cx;
    int cy;
    int sx;
    int sy;
    int cx_end = grid->cam_x + layout->cols;
    int cy_end = grid->cam_y + layout->rows;

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
            sx += layout->grid_x;
            sy += layout->grid_y;
            gfx_rect(g, sx + 1, sy + 1, CELL_SIZE - 2, CELL_SIZE - 2, COL_SOLID);
            gfx_outline(g, sx + 1, sy + 1, CELL_SIZE - 2, CELL_SIZE - 2,
                        COL_SOLID_EDGE);
        }
    }
}

static void grid_draw_map_edge(Gfx *g, const Grid *grid, const Layout *layout)
{
    int sx;
    int sy;

    /* Outline of the whole 64x64 map, drawn only if it is (partly) visible. */
    grid_cell_to_screen(grid, 0, 0, &sx, &sy);
    sx += layout->grid_x;
    sy += layout->grid_y;
    gfx_outline(g, sx, sy, MAP_COLS * CELL_SIZE, MAP_ROWS * CELL_SIZE,
                COL_MAP_EDGE);
}

static void grid_draw_cursor(Gfx *g, const Grid *grid, const Layout *layout)
{
    int sx;
    int sy;

    grid_cell_to_screen(grid, grid->cursor_x, grid->cursor_y, &sx, &sy);
    sx += layout->grid_x;
    sy += layout->grid_y;
    if (sx + CELL_SIZE < layout->grid_x || sx > layout->grid_x + layout->grid_w) {
        return;
    }
    if (sy + CELL_SIZE < layout->grid_y || sy > layout->grid_y + layout->grid_h) {
        return;
    }

    /* soft amber fill + 2 px frame so the cursor is obvious on the PSP LCD */
    gfx_rect_alpha(g, sx + 1, sy + 1, CELL_SIZE - 2, CELL_SIZE - 2, COL_CURSOR, 56);
    gfx_rect(g, sx, sy, CELL_SIZE, 2, COL_CURSOR);
    gfx_rect(g, sx, sy + CELL_SIZE - 2, CELL_SIZE, 2, COL_CURSOR);
    gfx_rect(g, sx, sy, 2, CELL_SIZE, COL_CURSOR);
    gfx_rect(g, sx + CELL_SIZE - 2, sy, 2, CELL_SIZE, COL_CURSOR);
}

static void grid_draw_status(Gfx *g, const Grid *grid, const Layout *layout)
{
    int ty = layout->grid_y + 6;
    int w;
    int tx;

    /* tiny coordinate readout, top-right corner of the grid */
    w = gfx_text_width("X:00  Y:00", 1);
    tx = layout->grid_x + layout->grid_w - 4 - w;
    gfx_rect_alpha(g, tx - 4, ty - 3, w + 8, 13, COL_BG, 200);

    tx = gfx_text(g, tx, ty, "X:", COL_TEXT_DIM, 1);
    tx = gfx_text_int(g, tx, ty, grid->cursor_x, COL_TEXT, 1);
    tx = gfx_text(g, tx, ty, "  Y:", COL_TEXT_DIM, 1);
    gfx_text_int(g, tx, ty, grid->cursor_y, COL_TEXT, 1);
}

void grid_draw(Gfx *g, const Grid *grid, const Layout *layout)
{
    gfx_rect(g, layout->grid_x, layout->grid_y, layout->grid_w, layout->grid_h,
             COL_BG);
    grid_draw_lines(g, grid, layout);
    grid_draw_map_edge(g, grid, layout);
    grid_draw_cells(g, grid, layout);
    grid_draw_cursor(g, grid, layout);
    grid_draw_status(g, grid, layout);
}
