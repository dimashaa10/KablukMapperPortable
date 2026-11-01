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

void grid_build_shell(Grid *grid)
{
    int x;
    int y;

    for (x = 0; x < MAP_COLS; x++) {
        grid->solid[0][x] = CELL_SOLID;
        grid->solid[MAP_ROWS - 1][x] = CELL_SOLID;
        grid->tex[0][x] = TEX_NONE;
        grid->tex[MAP_ROWS - 1][x] = TEX_NONE;
        grid->entity[0][x] = 0;
        grid->entity[MAP_ROWS - 1][x] = 0;
    }
    for (y = 0; y < MAP_ROWS; y++) {
        grid->solid[y][0] = CELL_SOLID;
        grid->solid[y][MAP_COLS - 1] = CELL_SOLID;
        grid->tex[y][0] = TEX_NONE;
        grid->tex[y][MAP_COLS - 1] = TEX_NONE;
        grid->entity[y][0] = 0;
        grid->entity[y][MAP_COLS - 1] = 0;
    }
}

void grid_clear_map(Grid *grid)
{
    int x;
    int y;

    for (y = 0; y < MAP_ROWS; y++) {
        for (x = 0; x < MAP_COLS; x++) {
            grid->solid[y][x] = CELL_EMPTY;
            grid->tex[y][x] = TEX_NONE;
            grid->entity[y][x] = 0;
        }
    }
    grid->sel_x = -1;
    grid->sel_y = -1;
    grid_build_shell(grid);
}

void grid_init(Grid *grid)
{
    /* start in the middle of the map: the origin of the world is the middle
     * cell, so that is where the camera and the cursor begin */
    grid_clear_map(grid);
    grid->cursor_x = MAP_COLS / 2;
    grid->cursor_y = MAP_ROWS / 2;
    grid->cam_x = MAP_COLS / 2 - 7;
    grid->cam_y = MAP_ROWS / 2 - 8;
}

void grid_paint(Grid *grid, int cx, int cy, int tex_index)
{
    if (cx < 0 || cx >= MAP_COLS || cy < 0 || cy >= MAP_ROWS) {
        return;
    }
    grid->solid[cy][cx] = CELL_SOLID;
    if (tex_index >= 0 && tex_index < WAD_MAX_TEX) {
        grid->tex[cy][cx] = (unsigned short)tex_index;
    }
}

void grid_set_cell(Grid *grid, int cx, int cy, unsigned char value)
{
    if (cx < 0 || cx >= MAP_COLS || cy < 0 || cy >= MAP_ROWS) {
        return;
    }
    if (value == CELL_EMPTY) {
        grid_erase(grid, cx, cy);
        return;
    }
    grid->solid[cy][cx] = CELL_SOLID;
}

void grid_erase(Grid *grid, int cx, int cy)
{
    if (cx < 0 || cx >= MAP_COLS || cy < 0 || cy >= MAP_ROWS) {
        return;
    }
    if (grid_is_shell(cx, cy)) {
        return; /* the shell stays: no holes into the void */
    }
    grid->solid[cy][cx] = CELL_EMPTY;
    grid->tex[cy][cx] = TEX_NONE;
    if (grid->sel_x == cx && grid->sel_y == cy) {
        grid_select_none(grid);
    }
}

unsigned char grid_get_cell(const Grid *grid, int cx, int cy)
{
    if (cx < 0 || cx >= MAP_COLS || cy < 0 || cy >= MAP_ROWS) {
        return CELL_EMPTY;
    }
    return grid->solid[cy][cx];
}

int grid_get_tex(const Grid *grid, int cx, int cy)
{
    if (cx < 0 || cx >= MAP_COLS || cy < 0 || cy >= MAP_ROWS) {
        return TEX_NONE;
    }
    return (int)grid->tex[cy][cx];
}

void grid_set_entity(Grid *grid, int cx, int cy, int class_index)
{
    if (cx < 0 || cx >= MAP_COLS || cy < 0 || cy >= MAP_ROWS) {
        return;
    }
    if (grid_is_shell(cx, cy)) {
        return; /* entities do not belong in the sealing shell */
    }
    if (class_index < 0 || class_index > 254) {
        grid->entity[cy][cx] = 0;
        if (grid->sel_x == cx && grid->sel_y == cy) {
            grid_select_none(grid);
        }
        return;
    }
    grid->entity[cy][cx] = (unsigned char)(class_index + 1);
}

int grid_entity_at(const Grid *grid, int cx, int cy)
{
    if (cx < 0 || cx >= MAP_COLS || cy < 0 || cy >= MAP_ROWS) {
        return -1;
    }
    if (grid->entity[cy][cx] == 0) {
        return -1;
    }
    return (int)grid->entity[cy][cx] - 1;
}

void grid_select(Grid *grid, int cx, int cy)
{
    if (grid_entity_at(grid, cx, cy) < 0) {
        grid_select_none(grid);
        return;
    }
    grid->sel_x = cx;
    grid->sel_y = cy;
}

void grid_select_none(Grid *grid)
{
    grid->sel_x = -1;
    grid->sel_y = -1;
}

int grid_selected_class(const Grid *grid)
{
    if (grid->sel_x < 0 || grid->sel_y < 0) {
        return -1;
    }
    return grid_entity_at(grid, grid->sel_x, grid->sel_y);
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

static void grid_draw_blocks(Gfx *g, const Grid *grid, const Layout *layout,
                             Textures *textures, const Wad *wad)
{
    int cx;
    int cy;
    int sx;
    int sy;
    int tex_index;
    int inner = CELL_SIZE - 2;
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
            if (grid->solid[cy][cx] == CELL_EMPTY) {
                continue;
            }
            grid_cell_to_screen(grid, cx, cy, &sx, &sy);
            sx += layout->grid_x + 1;
            sy += layout->grid_y + 1;

            tex_index = (int)grid->tex[cy][cx];
            if (tex_index != TEX_NONE && tex_index < wad->count) {
                SDL_Texture *texture = textures_get(g, textures, wad, tex_index);
                if (texture != NULL) {
                    gfx_blit(g, texture, sx, sy, inner, inner);
                    continue;
                }
            }
            /* no texture (shell cells, or a wad that is not loaded yet) */
            gfx_rect(g, sx, sy, inner, inner, COL_SOLID);
            gfx_outline(g, sx, sy, inner, inner, COL_SOLID_EDGE);
        }
    }
}

static void grid_draw_entities(Gfx *g, const Grid *grid, const Layout *layout)
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
            int selected;

            if (grid->entity[cy][cx] == 0) {
                continue;
            }
            selected = (cx == grid->sel_x && cy == grid->sel_y);
            grid_cell_to_screen(grid, cx, cy, &sx, &sy);
            sx += layout->grid_x;
            sy += layout->grid_y;

            /* small green square marks a point entity */
            gfx_rect(g, sx + 5, sy + 5, 6, 6, COL_ENTITY);
            gfx_outline(g, sx + 4, sy + 4, 8, 8, COL_BG);
            if (selected) {
                gfx_outline(g, sx + 2, sy + 2, CELL_SIZE - 4, CELL_SIZE - 4,
                            COL_ENTITY_SEL);
            }
        }
    }
}

static void grid_draw_map_edge(Gfx *g, const Grid *grid, const Layout *layout)
{
    int sx;
    int sy;

    /* Outline of the whole map (-2048..2048 units), drawn even when only a
     * corner of it is on screen. */
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

void grid_draw(Gfx *g, const Grid *grid, const Layout *layout, Textures *textures,
               const Wad *wad)
{
    gfx_rect(g, layout->grid_x, layout->grid_y, layout->grid_w, layout->grid_h,
             COL_BG);
    grid_draw_lines(g, grid, layout);
    grid_draw_map_edge(g, grid, layout);
    grid_draw_blocks(g, grid, layout, textures, wad);
    grid_draw_entities(g, grid, layout);
    grid_draw_cursor(g, grid, layout);
    grid_draw_status(g, grid, layout);
}
