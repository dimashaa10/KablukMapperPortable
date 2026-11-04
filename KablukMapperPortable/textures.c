/*
 * Kabluk Mapper Portable - textures.c
 */
#include "textures.h"

/* biggest mip the editor ever wants: enough for a 64x64 level */
#define MIP_MAX 64

/* The preview drawn on a block in the grid. The cell itself is CELL_SIZE (16)
 * px, so a 16 px mip is one texture pixel per screen pixel: sharp, and 1 KB
 * per texture instead of the 16 KB a 64x64 one would cost (the grid can show
 * a few hundred blocks and the PSP shares 2 MB of video memory with the two
 * framebuffers). It still beats the smallest mip stretched over the cell,
 * which is what made blocks look like grey mush. */
#define TEXTURES_BLOCK_LEVEL CELL_SIZE

/* The PSP renderer only accepts power-of-two textures, so the console gets a
 * resampled one when a texture is 24x24 or 40x64: without this SDL refuses it
 * and the cell stays empty. The desktop renderers take any size, so there the
 * pixels go up untouched. */
#if defined(KMP_HAVE_PSP) || defined(__PSP__)
#define TEXTURES_POT 1
#else
#define TEXTURES_POT 0
#endif

static unsigned char mip_pixels[MIP_MAX * MIP_MAX];
static unsigned int argb[MIP_MAX * MIP_MAX];
#if TEXTURES_POT
static unsigned int argb_pot[MIP_MAX * MIP_MAX];
#endif

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

#if TEXTURES_POT
static int textures_pot_size(int value)
{
    int size = 8;

    while (size < value && size < MIP_MAX) {
        size *= 2;
    }
    return size;
}

/* Nearest neighbour to the next power of two, both ways up and down. */
static void textures_resample(int w, int h, int pot_w, int pot_h)
{
    int x;
    int y;

    for (y = 0; y < pot_h; y++) {
        int sy = y * h / pot_h;

        for (x = 0; x < pot_w; x++) {
            int sx = x * w / pot_w;

            argb_pot[y * pot_w + x] = argb[sy * w + sx];
        }
    }
}

#endif /* TEXTURES_POT */

/* Builds the texture a cell shows: the centre square of the mip, so the
 * picture is never stretched. A 32x64 texture is drawn from its middle 32x32
 * (a square swatch, like every other cell) instead of being squeezed into a
 * square or shown as a thin sliver with empty space on both sides. */
static SDL_Texture *textures_build(Gfx *g, const Wad *wad, int index, int level,
                                   int *out_w, int *out_h)
{
    int w = 0;
    int h = 0;
    int count;
    int side;
    int ox;
    int oy;
    int x;
    int y;

    count = textures_read(wad, index, level, &w, &h);
    if (count == 0) {
        return NULL;
    }
    side = (w < h) ? w : h;
    ox = (w - side) / 2;
    oy = (h - side) / 2;
    for (y = 0; y < side; y++) {
        const unsigned char *src = mip_pixels + (oy + y) * w + ox;

        for (x = 0; x < side; x++) {
            argb[y * side + x] = wad_palette_rgb(wad, src[x]);
        }
    }
    *out_w = side;
    *out_h = side;

#if TEXTURES_POT
    /* Square first, then the power-of-two resample: the two steps used to be
     * the other way round, and rounding 24 to 32 (or 40 to 64) stretched the
     * picture on one axis only. */
    if (side != textures_pot_size(side)) {
        int pot = textures_pot_size(side);

        textures_resample(side, side, pot, pot);
        *out_w = pot;
        *out_h = pot;
        return gfx_texture_from_argb(g, argb_pot, pot, pot);
    }
#endif
    return gfx_texture_from_argb(g, argb, side, side);
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

    level = wad_fit_level(wad, index, TEXTURES_BLOCK_LEVEL);
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
