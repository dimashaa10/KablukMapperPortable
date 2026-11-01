/*
 * Kabluk Mapper Portable - ui.h
 *
 * Left tool panel with four buttons, top to bottom.
 */
#ifndef KMP_UI_H
#define KMP_UI_H

#include "gfx.h"

/* Tool ids = button index, top to bottom. */
#define TOOL_SELECT 0
#define TOOL_BLOCK 1
#define TOOL_ENTITY 2
#define TOOL_TEXTURE 3

void ui_button_rect(int index, int *x, int *y, int *w, int *h);
int ui_hit_test(int mx, int my); /* button index or -1 */
const char *ui_tool_name(int index);
void ui_clamp_tool(int *tool);
/* 'cursor_tool' is the highlighted row (the mouse hover, or the row the arrows
 * are on), 'focused' draws the panel as the one holding the pad. */
void ui_draw_panel(Gfx *g, int active_tool, int cursor_tool, int focused);

#endif /* KMP_UI_H */
