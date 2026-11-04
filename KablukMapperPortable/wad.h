/*
 * Kabluk Mapper Portable - wad.h
 *
 * Minimal Quake WAD2 reader: texture names for the texture picker and the
 * palettised mip levels for the little previews drawn on blocks.
 * Only stdio is used, so it works the same on Windows and on the PSP.
 */
#ifndef KMP_WAD_H
#define KMP_WAD_H

#include "config.h"

#define WAD_KIND_NONE 0
#define WAD_KIND_WAD2 2
#define WAD_KIND_WAD3 3

/* lumpinfo type of a mip texture; everything else (the palette, for one) is
 * skipped when the texture list is built */
#define WAD_LUMP_MIPTEX 0x44
/* miptex header: 16 byte name, width, height, then the four level offsets */
#define WAD_HEADER_SIZE 40

typedef struct Wad {
    char path[PATH_LEN];
    int opened;
    int kind;
    int count; /* number of mip textures */

    unsigned int lump_offset[WAD_MAX_TEX];
    unsigned int lump_size[WAD_MAX_TEX];
    unsigned short tex_w[WAD_MAX_TEX];
    unsigned short tex_h[WAD_MAX_TEX];
    char names[WAD_MAX_TEX][WAD_NAME_LEN];

    unsigned char palette[768];
    int has_palette;
} Wad;

/* Returns the texture count (0 when the file is not a usable WAD). */
int wad_load(Wad *wad, const char *path);
void wad_close(Wad *wad);

/* "WAD2", "WAD3" or "none" - the two need different mip decoders, so the log
 * says which one was opened. */
const char *wad_kind_text(const Wad *wad);

/* Best mip level to use as a small square preview (0..3). */
int wad_preview_level(const Wad *wad, int index);

/* Smallest mip level (0..3) whose width and height are <= max_size. The
 * picker asks for a level whose size is close to the cell it draws, so the
 * thumbnail is scaled by a little instead of by a factor of five. */
int wad_fit_level(const Wad *wad, int index, int max_size);

/* Longest side of a mip level, for laying a preview out. */
int wad_mip_size(const Wad *wad, int index, int level);

/* Decompresses one mip level. Returns 1 on success. */
int wad_read_mip(const Wad *wad, int index, int level, unsigned char *out,
                 int cap, int *out_w, int *out_h);

/* 0xAARRGGBB colour of a palette index (grey ramp when no palette). */
unsigned int wad_palette_rgb(const Wad *wad, int index);

#endif /* KMP_WAD_H */
