/*
 * Kabluk Mapper Portable - textures.c
 */
#include "textures.h"

#define MIP_MAX 64

static unsigned char mip_pixels[MIP_MAX * MIP_MAX];
static unsigned int argb[MIP_MAX * MIP_MAX];

void textures_reset(Textures *textures)
{
    int i;

    for (i = 0; i < WAD_MAX_TEX; i++) {
        if (textures->state[i] == TEX_STATE_READY) {
            gfx_texture_destroy(textures->tex[i]);
        }
        textures->tex[i] = NULL;
        textures->state[i] = TEX_STATE_NONE;
    }
}

SDL_Texture *textures_get(Gfx *g, Textures *textures, const Wad *wad, int index)
{
    int level;
    int w = 0;
    int h = 0;
    int count;
    int i;

    if (index < 0 || index >= wad->count) {
        return NULL;
    }
    if (textures->state[index] == TEX_STATE_READY) {
        return textures->tex[index];
    }
    if (textures->state[index] == TEX_STATE_FAILED) {
        return NULL;
    }

    level = wad_preview_level(wad, index);
    if (!wad_read_mip(wad, index, level, mip_pixels, (int)sizeof(mip_pixels), &w, &h)) {
        textures->state[index] = TEX_STATE_FAILED;
        return NULL;
    }
    count = w * h;
    for (i = 0; i < count; i++) {
        argb[i] = wad_palette_rgb(wad, mip_pixels[i]);
    }
    textures->tex[index] = gfx_texture_from_argb(g, argb, w, h);
    if (textures->tex[index] == NULL) {
        textures->state[index] = TEX_STATE_FAILED;
        return NULL;
    }
    textures->state[index] = TEX_STATE_READY;
    return textures->tex[index];
}
