/*
 * Kabluk Mapper Portable - picker.h
 *
 * The two popups of the left panel:
 *   ENTITY  - every @PointClass of the fgd (the last used one is preselected),
 *             drawn as a list with the class description underneath.
 *   TEXTURE - every texture of the .wad chosen in the editor settings, drawn
 *             as a grid of square cells: the picture on top, the name below.
 *
 * Cross / X confirms, circle / Z closes. In the texture grid the arrows move
 * by one cell, up and down jump a whole row, and the pad nub still moves the
 * pointer over it.
 */
#ifndef KMP_PICKER_H
#define KMP_PICKER_H

#include <SDL.h>

#include "fgd.h"
#include "gfx.h"
#include "textures.h"
#include "wad.h"

#define PICK_ENTITY 1
#define PICK_TEXTURE 2

/* where a picked texture goes: a new block, or one of the two slabs */
#define PICK_TARGET_BLOCK 0
#define PICK_TARGET_FLOOR 1
#define PICK_TARGET_CEILING 2

#define PICKER_EVENT_NONE 0
#define PICKER_EVENT_CONFIRM 1
#define PICKER_EVENT_CANCEL 2

/* Rows of the entity list that fit in the popup. */
#define PICKER_VISIBLE 15

typedef struct Picker {
    int open;
    int kind;
    int count;
    int cur;
    int top;    /* entity list: first visible row; texture grid: first cell of
                 * the page, always a multiple of PICKER_COLS */
    int hover;  /* index under the pointer, -1 when there is none */
    int index[FGD_MAX_CLASSES]; /* entity picker: fgd class indices */
    int target;                 /* PICK_TARGET_*: main.c routes the result */
} Picker;

void picker_init(Picker *picker);
int picker_is_open(const Picker *picker);
void picker_close(Picker *picker);

void picker_open_entities(Picker *picker, const Fgd *fgd, int preselect);
void picker_open_textures(Picker *picker, const Wad *wad, int preselect);

/* Returns PICKER_EVENT_* */
int picker_handle_event(Picker *picker, const SDL_Event *event);
void picker_draw(Gfx *g, const Picker *picker, const Fgd *fgd,
                 Textures *textures, const Wad *wad);

#endif /* KMP_PICKER_H */
