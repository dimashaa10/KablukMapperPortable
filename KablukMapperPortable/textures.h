/*
 * Kabluk Mapper Portable - textures.h
 *
 * Lazy caches of the previews read from the .wad.
 *
 * Two of them, because they are asked for very different pictures:
 *
 *   textures_get()   - the 14 px preview drawn on a block in the grid. One
 *                      entry per wad texture, tiny (8x8 .. 16x16 mip), never
 *                      released: the grid can show a few hundred blocks and
 *                      each of them has to be free to draw.
 *   textures_thumb() - the thumbnail of the texture picker, as large as the
 *                      picker cell (32 px). A wad can hold hundreds of
 *                      textures and they are not all on screen, so these go
 *                      through a small least-recently-used cache: at most
 *                      TEXTURES_THUMB_SLOTS of them exist at a time, which is
 *                      what keeps the PSP's texture memory happy.
 */
#ifndef KMP_TEXTURES_H
#define KMP_TEXTURES_H

#include <SDL.h>

#include "gfx.h"
#include "wad.h"

#define TEX_STATE_NONE 0
#define TEX_STATE_READY 1
#define TEX_STATE_FAILED 2

/* 48 slots: a page of the picker (36 cells) plus a row of history, so
 * scrolling re-uses the thumbnails instead of rebuilding them. */
#define TEXTURES_THUMB_SLOTS 48

typedef struct TextureThumb {
    SDL_Texture *tex;
    int index; /* wad texture, -1 when the slot is free */
    int level; /* mip level in 'tex' */
    unsigned int stamp;
} TextureThumb;

typedef struct Textures {
    SDL_Texture *tex[WAD_MAX_TEX];
    unsigned char state[WAD_MAX_TEX];

    TextureThumb thumb[TEXTURES_THUMB_SLOTS];
    unsigned char thumb_failed[WAD_MAX_TEX];
    unsigned int stamp;
} Textures;

void textures_reset(Textures *textures);

/* Small preview for a block in the grid. */
SDL_Texture *textures_get(Gfx *g, Textures *textures, const Wad *wad, int index);

/* Large thumbnail for the texture picker: the smallest mip level that is not
 * bigger than max_size, so a 64x64 texture is shown from its 32x32 level and
 * a 32x64 one keeps its shape. NULL when the wad cannot supply it. */
SDL_Texture *textures_thumb(Gfx *g, Textures *textures, const Wad *wad, int index,
                            int max_size);

#endif /* KMP_TEXTURES_H */
