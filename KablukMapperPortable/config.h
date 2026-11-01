/*
 * Kabluk Mapper Portable - config.h
 *
 * Every size in the program comes from here. The resolution is the native
 * PSP LCD size (480x272) and the layout is built around it, so the desktop
 * build already looks exactly like the future PSP build.
 *
 * Rules for keeping the code portable (SDL2 on Windows now, pspsdk later):
 *   - no <windows.h>, no WinAPI, no OpenGL, no SDL_ttf
 *   - only plain C (C89 friendly) + the core SDL2 API
 *   - no dynamic allocation, all state lives in static/fixed structs
 *   - integer math only, no floats where avoidable
 */
#ifndef KMP_CONFIG_H
#define KMP_CONFIG_H

/* ------------------------------------------------------------------ */
/* Display                                                             */
/* ------------------------------------------------------------------ */
#define SCREEN_W 480
#define SCREEN_H 272

#define WINDOW_TITLE "Kabluk Mapper Portable"

/* ------------------------------------------------------------------ */
/* Left tool panel                                                     */
/* ------------------------------------------------------------------ */
#define PANEL_W 80
#define PANEL_BUTTONS 4
#define PANEL_BTN_MARGIN 6
#define PANEL_BTN_W (PANEL_W - PANEL_BTN_MARGIN * 2) /* 68 */
#define PANEL_BTN_H 56
#define PANEL_BTN_GAP 10
#define PANEL_BTN_TOP                                  \
    ((SCREEN_H - (PANEL_BUTTONS * PANEL_BTN_H +        \
                  (PANEL_BUTTONS - 1) * PANEL_BTN_GAP)) / 2)

/* ------------------------------------------------------------------ */
/* Grid area (everything right of the panel)                           */
/* ------------------------------------------------------------------ */
#define GRID_X PANEL_W
#define GRID_Y 0
#define GRID_W (SCREEN_W - GRID_X) /* 400 */
#define GRID_H SCREEN_H            /* 272 */

#define CELL_SIZE 16
#define VIEW_COLS (GRID_W / CELL_SIZE) /* 25 visible columns */
#define VIEW_ROWS (GRID_H / CELL_SIZE) /* 17 visible rows    */

#define GRID_MAJOR_EVERY 8 /* every 8th line is brighter (64 units) */

/* Map model, in cells (Quake-ish 64 units per major line). */
#define MAP_COLS 64
#define MAP_ROWS 64

/* ------------------------------------------------------------------ */
/* Timing                                                              */
/* ------------------------------------------------------------------ */
#define FRAME_MS 16 /* ~60 fps cap, PSP screen refresh is 60 Hz */

/* ------------------------------------------------------------------ */
/* Palette (0xRRGGBB) - Quake-ish browns and amber                     */
/* ------------------------------------------------------------------ */
#define COL_BG 0x0B0906
#define COL_GRID_MINOR 0x201A12
#define COL_GRID_MAJOR 0x3B2F21
#define COL_MAP_EDGE 0x8A6A34
#define COL_SOLID 0x6E4E28
#define COL_SOLID_EDGE 0xA07A42
#define COL_CURSOR 0xE8A33C

#define COL_PANEL 0x18120C
#define COL_PANEL_EDGE 0x6B4F2A
#define COL_BTN 0x241B12
#define COL_BTN_HOVER 0x35281A
#define COL_BTN_ACTIVE 0x6B4A1C
#define COL_BTN_EDGE 0x53401F
#define COL_BTN_EDGE_ACTIVE 0xE8A33C

#define COL_TEXT 0xD9C69C
#define COL_TEXT_DIM 0x8C7452
#define COL_ACCENT 0xE8A33C

#endif /* KMP_CONFIG_H */
