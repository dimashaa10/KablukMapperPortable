/*
 * Kabluk Mapper Portable - textures.h
 *
 * Lazy cache of small texture previews read from the .wad. Each block in the
 * grid draws one of these thumbnails, so only the levels that are actually
 * used cost memory (a PSP friendly 4x4 .. 32x32 texture each).
 */
#ifndef KMP_TEXTURES_H
#define KMP_TEXTURES_H

#include <SDL.h>

#include "gfx.h"
#include "wad.h"

#define TEX_STATE_NONE 0
#define TEX_STATE_READY 1
#define TEX_STATE_FAILED 2

typedef struct Textures {
    SDL_Texture *tex[WAD_MAX_TEX];
    unsigned char state[WAD_MAX_TEX];
} Textures;

void textures_reset(Textures *textures);
SDL_Texture *textures_get(Gfx *g, Textures *textures, const Wad *wad, int index);

#endif /* KMP_TEXTURES_H */
