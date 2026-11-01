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

/* the two corner credits of the start screen */
#define APP_AUTHOR "DIMTENCORP"
#define APP_VERSION "VERSION 0.1"

/* ------------------------------------------------------------------ */
/* Panels (both can be folded away: L = left, R = right)               */
/* ------------------------------------------------------------------ */
#define PANEL_W 80       /* left tool panel  */
#define RIGHT_PANEL_W 160 /* right property panel (FGD property list) */

#define PANEL_BUTTONS 4
#define PANEL_BTN_MARGIN 6
#define PANEL_BTN_W (PANEL_W - PANEL_BTN_MARGIN * 2) /* 68 */
#define PANEL_BTN_H 56
#define PANEL_BTN_GAP 10
#define PANEL_BTN_TOP                                  \
    ((SCREEN_H - (PANEL_BUTTONS * PANEL_BTN_H +        \
                  (PANEL_BUTTONS - 1) * PANEL_BTN_GAP)) / 2)

/* ------------------------------------------------------------------ */
/* Grid area (whatever is left between the panels)                     */
/* ------------------------------------------------------------------ */
#define CELL_SIZE 16
#define MAX_VIEW_COLS (SCREEN_W / CELL_SIZE) /* 30 */
#define MAX_VIEW_ROWS (SCREEN_H / CELL_SIZE) /* 17 */

#define GRID_MAJOR_EVERY 8 /* every 8th line is brighter (64 units) */

/* Map model, in cells. One cell = one brush = 128x128x128 units.
 * 32 cells * 128 units = 4096 units per axis, centred on the world origin:
 * the map runs from -2048 to +2048, which is the range Quake (and the PSP
 * port later) can handle without running into the +/-4096 world limits. */
#define MAP_COLS 32
#define MAP_ROWS 32

/* One editor block. Quake units; a player is 56x56x32 tall, so 128 units
 * is comfortably above it and a sealed crate can be built from it. */
#define BLOCK_UNITS 128
#define BLOCK_HEIGHT_UNITS BLOCK_UNITS

/* Whole map, in Quake units: 4096 x 4096, from -2048 to +2048. */
#define MAP_WORLD_UNITS (MAP_COLS * BLOCK_UNITS)
#define MAP_HALF_UNITS (MAP_WORLD_UNITS / 2)

/* The map is sealed by the exporter, not by the editor: the floor and the
 * ceiling are always written as slabs, and border cells the user left empty
 * get a wall around them, so a map can never leak into the void no matter
 * what was (not) painted. */

/* ------------------------------------------------------------------ */
/* Timing                                                              */
/* ------------------------------------------------------------------ */
#define FRAME_MS 16 /* ~60 fps cap, PSP screen refresh is 60 Hz */

/* ------------------------------------------------------------------ */
/* File paths (future: ms0:/PSP/GAME/KABLUK on the PSP)                */
/* ------------------------------------------------------------------ */
#define PATH_LEN 260
#define FS_MAX_ENTRIES 96
#define DEFAULT_OUT_DIR "maps"
#define DEFAULT_MAP_NAME "kabluk"

/* Entity definitions. The .fgd is loaded from next to the executable
 * (a copy is put there by the build), then from the repo folders. */
#define FGD_FILE_NAME "Quake.fgd"
#define FGD_TEXT_MAX 98304

#define FGD_MAX_CLASSES 220
#define FGD_MAX_PROPS 48
#define FGD_MAX_BASES 5
#define FGD_NAME_LEN 40
#define FGD_TYPE_LEN 20
#define FGD_VALUE_LEN 48
#define FGD_DESC_LEN 110

#define FGD_KIND_POINT 0
#define FGD_KIND_SOLID 1
#define FGD_KIND_BASE 2

/* Texture list from the .wad chosen in the editor settings (Quake WAD2) */
#define WAD_MAX_TEX 512
#define WAD_NAME_LEN 16

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

#define COL_ENTITY 0x54C23C
#define COL_ENTITY_SEL 0xE8D24A
#define COL_LIST_INFO 0x2E2418

#define COL_FIELD 0x100C08
#define COL_SEPARATOR 0x3B2F21
#define COL_ROW_SEL 0x4A3312
#define COL_OK 0x8FBF5A
#define COL_WARN 0xC96A3C

#endif /* KMP_CONFIG_H */
