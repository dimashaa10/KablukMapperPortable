/*
 * Kabluk Mapper Portable - layout.h
 *
 * The grid viewport depends on which panels are open, so it is computed at
 * runtime instead of being a compile time constant. Both panel widths are a
 * whole number of cells (80 = 5 cells, 144 = 9 cells), so the grid always
 * stays aligned to the map regardless of what is folded away.
 */
#ifndef KMP_LAYOUT_H
#define KMP_LAYOUT_H

#include "config.h"

typedef struct Layout {
    int grid_x;
    int grid_y;
    int grid_w;
    int grid_h;
    int cols; /* visible cells */
    int rows;
} Layout;

void layout_compute(Layout *layout, int left_open, int right_open);

#endif /* KMP_LAYOUT_H */
