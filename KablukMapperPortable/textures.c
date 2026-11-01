/*
 * Kabluk Mapper Portable - textures.c
 */
#include "textures.h"

/* biggest mip the editor ever wants: enough for a 64x64 level */
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
        textures->thumb_failed[i] = 0;
    }
    for (i = 0; i < TEXTURES_THUMB_SLOTS; i++) {
        gfx_texture_destroy(textures->thumb[i].tex);
        textures->thumb[i].tex = NULL;
        textures->thumb[i].index = -1;
        textures->thumb[i].level = 0;
        textures->thumb[i].stamp = 0;
    }
    textures->stamp = 0;
}

/* Reads 'level' of a texture into the mip buffer and returns the pixel count,
 * or 0 when it cannot be read. */
static int textures_read(const Wad *wad, int index, int level, int *out_w, int *out_h)
{
    int w = 0;
    int h = 0;

    if (!wad_read_mip(wad, index, level, mip_pixels, (int)sizeof(mip_pixels), &w, &h)) {
        return 0;
    }
    if (w <= 0 || h <= 0 || w * h > (int)sizeof(mip_pixels)) {
        return 0;
    }
    *out_w = w;
    *out_h = h;
    return w * h;
}

static SDL_Texture *textures_build(Gfx *g, const Wad *wad, int index, int level,
                                   int *out_w, int *out_h)
{
    int count;
    int i;

    count = textures_read(wad, index, level, out_w, out_h);
    if (count == 0) {
        return NULL;
    }
    for (i = 0; i < count; i++) {
        argb[i] = wad_palette_rgb(wad, mip_pixels[i]);
    }
    return gfx_texture_from_argb(g, argb, *out_w, *out_h);
}

SDL_Texture *textures_get(Gfx *g, Textures *textures, const Wad *wad, int index)
{
    int level;
    int w = 0;
    int h = 0;

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
    textures->tex[index] = textures_build(g, wad, index, level, &w, &h);
    if (textures->tex[index] == NULL) {
        textures->state[index] = TEX_STATE_FAILED;
        return NULL;
    }
    textures->state[index] = TEX_STATE_READY;
    return textures->tex[index];
}

/* ------------------------------------------------------------------ */
/* Thumbnails (least recently used)                                    */
/* ------------------------------------------------------------------ */
static TextureThumb *textures_thumb_find(Textures *textures, int index, int level)
{
    int i;

    for (i = 0; i < TEXTURES_THUMB_SLOTS; i++) {
        TextureThumb *thumb = &textures->thumb[i];

        if (thumb->tex != NULL && thumb->index == index && thumb->level == level) {
            return thumb;
        }
    }
    return NULL;
}

/* A free slot, or the one that was used longest ago. */
static TextureThumb *textures_thumb_slot(Textures *textures)
{
    TextureThumb *oldest = &textures->thumb[0];
    int i;

    for (i = 0; i < TEXTURES_THUMB_SLOTS; i++) {
        TextureThumb *thumb = &textures->thumb[i];

        if (thumb->tex == NULL) {
            return thumb;
        }
        if (thumb->stamp < oldest->stamp) {
            oldest = thumb;
        }
    }
    return oldest;
}

SDL_Texture *textures_thumb(Gfx *g, Textures *textures, const Wad *wad, int index,
                            int max_size)
{
    TextureThumb *thumb;
    int level;
    int w = 0;
    int h = 0;

    if (index < 0 || index >= wad->count || max_size <= 0) {
        return NULL;
    }
    if (textures->thumb_failed[index]) {
        return NULL;
    }

    level = wad_fit_level(wad, index, max_size);
    thumb = textures_thumb_find(textures, index, level);
    textures->stamp++;
    if (thumb != NULL) {
        thumb->stamp = textures->stamp;
        return thumb->tex;
    }

    thumb = textures_thumb_slot(textures);
    gfx_texture_destroy(thumb->tex);
    thumb->tex = textures_build(g, wad, index, level, &w, &h);
    thumb->index = index;
    thumb->level = level;
    thumb->stamp = textures->stamp;
    if (thumb->tex == NULL) {
        /* do not open the wad again for this one on every frame */
        textures->thumb_failed[index] = 1;
        thumb->index = -1;
        return NULL;
    }
    return thumb->tex;
}
