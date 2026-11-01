/*
 * Kabluk Mapper Portable - picker.h
 *
 * The two list popups of the left panel:
 *   ENTITY  - every @PointClass of the fgd (the last used one is preselected)
 *   TEXTURE - every texture of the .wad chosen in the editor settings
 * Cross / X confirms, circle / Z closes the list.
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

#define PICKER_EVENT_NONE 0
#define PICKER_EVENT_CONFIRM 1
#define PICKER_EVENT_CANCEL 2

#define PICKER_VISIBLE 13

typedef struct Picker {
    int open;
    int kind;
    int count;
    int cur;
    int top;
    int hover;
    int index[FGD_MAX_CLASSES]; /* entity picker: fgd class indices */
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
