/*
 * Kabluk Mapper Portable - layout.c
 */
#include "layout.h"

void layout_compute(Layout *layout, int left_open, int right_open)
{
    int left = left_open ? PANEL_W : 0;
    int right = right_open ? RIGHT_PANEL_W : 0;

    layout->grid_x = left;
    layout->grid_y = 0;
    layout->grid_w = SCREEN_W - left - right;
    layout->grid_h = SCREEN_H;

    layout->cols = layout->grid_w / CELL_SIZE;
    layout->rows = layout->grid_h / CELL_SIZE;

    if (layout->cols < 1) {
        layout->cols = 1;
    }
    if (layout->rows < 1) {
        layout->rows = 1;
    }
    if (layout->cols > MAX_VIEW_COLS) {
        layout->cols = MAX_VIEW_COLS;
    }
    if (layout->rows > MAX_VIEW_ROWS) {
        layout->rows = MAX_VIEW_ROWS;
    }
}
