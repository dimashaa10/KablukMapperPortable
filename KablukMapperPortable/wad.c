/*
 * Kabluk Mapper Portable - wad.c
 *
 * WAD2 layout:
 *   header:   "WAD2", int numlumps, int infotableofs
 *   lumpinfo: int filepos, int disksize, int size, char type, char compression,
 *             char pad[2], char name[16]
 *   type 0x44 = MIPTEX: name[16], int width, int height, int offsets[4]
 *   every mip level is RLE compressed (count/255-runs).
 *
 * The palette is not part of a texture wad, so it is picked up from the usual
 * Quake places next to the wad (gfx/palette.lmp) or from a "palette" lump.
 */
#include "wad.h"

#include <stdio.h>
#include <string.h>

#define WAD_LUMP_MIPTEX 0x44
#define WAD_HEADER_SIZE 40 /* miptex header: 16 + 4 + 4 + 16 */
#define WAD_COMP_MAX 16384

static unsigned int wad_comp[WAD_COMP_MAX];

static unsigned int rd32(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

static int read_at(FILE *file, unsigned int pos, unsigned char *buf, int n)
{
    if (fseek(file, (long)pos, SEEK_SET) != 0) {
        return 0;
    }
    return (int)fread(buf, 1, (size_t)n, file) == n;
}

static void wad_reset(Wad *wad)
{
    wad->opened = 0;
    wad->kind = WAD_KIND_NONE;
    wad->count = 0;
    wad->has_palette = 0;
    wad->path[0] = '\0';
}

void wad_close(Wad *wad)
{
    wad_reset(wad);
}

/* Copies at most 15 chars of a lump name; WAD3 keeps a path in there. */
static void wad_name_copy(char *dst, const unsigned char *src)
{
    int i;
    int n = 0;

    for (i = 0; i < WAD_NAME_LEN - 1 && src[i] != 0; i++) {
        if (src[i] == '\\' || src[i] == '/') {
            n = 0; /* keep only the last path component */
            continue;
        }
        dst[n++] = (char)src[i];
    }
    dst[n] = '\0';
}

/* Copy a name that is already terminated into a WAD_NAME_LEN field. The
 * strncpy() this replaces made GCC warn (-Wstringop-truncation) about a copy
 * that truncates, and the PSP build now shows warnings. */
static void wad_name_store(char *dst, const char *src)
{
    int i = 0;

    while (i < WAD_NAME_LEN - 1 && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* ------------------------------------------------------------------ */
/* Palette                                                             */
/* ------------------------------------------------------------------ */
static int wad_read_palette_file(Wad *wad, const char *path)
{
    FILE *file;
    unsigned char buf[768];
    size_t got;

    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    got = fread(buf, 1, sizeof(buf), file);
    fclose(file);
    if (got != sizeof(buf)) {
        return 0;
    }
    memcpy(wad->palette, buf, sizeof(buf));
    wad->has_palette = 1;
    return 1;
}

static void wad_find_palette(Wad *wad, const char *dir, const char *lump_name,
                             unsigned int lump_ofs, unsigned int lump_size)
{
    char path[PATH_LEN];
    int i;

    /* 1. a "palette.lmp" lump inside the wad itself */
    if (lump_name != NULL && lump_size == 768) {
        if (strcmp(lump_name, "palette.lmp") == 0 || strcmp(lump_name, "qpalette") == 0 ||
            strcmp(lump_name, "palette") == 0) {
            FILE *file = fopen(wad->path, "rb");
            if (file != NULL) {
                if (read_at(file, lump_ofs, wad->palette, 768)) {
                    wad->has_palette = 1;
                }
                fclose(file);
            }
            if (wad->has_palette) {
                return;
            }
        }
    }

    /* 2. the usual Quake locations around the wad */
    for (i = 0; i < 4 && !wad->has_palette; i++) {
        path[0] = '\0';
        if (i == 0) {
            /* <dir>/gfx/palette.lmp */
            int n = 0;
            while (dir[n] != '\0' && n < PATH_LEN - 20) {
                path[n] = dir[n];
                n++;
            }
            path[n] = '\0';
            strncat(path, "/gfx/palette.lmp", (size_t)(PATH_LEN - 1 - n));
        } else if (i == 1) {
            int n = 0;
            while (dir[n] != '\0' && n < PATH_LEN - 20) {
                path[n] = dir[n];
                n++;
            }
            path[n] = '\0';
            strncat(path, "/palette.lmp", (size_t)(PATH_LEN - 1 - n));
        } else if (i == 2) {
            int n = 0;
            while (dir[n] != '\0' && n < PATH_LEN - 24) {
                path[n] = dir[n];
                n++;
            }
            path[n] = '\0';
            strncat(path, "/../gfx/palette.lmp", (size_t)(PATH_LEN - 1 - n));
        } else {
            int n = 0;
            while (dir[n] != '\0' && n < PATH_LEN - 28) {
                path[n] = dir[n];
                n++;
            }
            path[n] = '\0';
            strncat(path, "/../../gfx/palette.lmp", (size_t)(PATH_LEN - 1 - n));
        }
        if (path[0] != '\0') {
            wad_read_palette_file(wad, path);
        }
    }
}

unsigned int wad_palette_rgb(const Wad *wad, int index)
{
    if (index < 0) {
        index = 0;
    }
    if (index > 255) {
        index = 255;
    }
    if (wad->has_palette) {
        return 0xFF000000u | ((unsigned int)wad->palette[index * 3] << 16) |
               ((unsigned int)wad->palette[index * 3 + 1] << 8) |
               (unsigned int)wad->palette[index * 3 + 2];
    }
    /* no palette: show the index as grey so the texture shape is visible */
    {
        unsigned int g = (unsigned int)index;
        return 0xFF000000u | (g << 16) | (g << 8) | g;
    }
}

/* ------------------------------------------------------------------ */
/* Loading                                                             */
/* ------------------------------------------------------------------ */
int wad_load(Wad *wad, const char *path)
{
    FILE *file;
    unsigned char header[12];
    unsigned char lump[32];
    int numlumps;
    int info_ofs;
    int i;
    char dir[PATH_LEN];
    char palette_lump[WAD_NAME_LEN];
    unsigned int palette_ofs = 0;
    unsigned int palette_size = 0;

    wad_reset(wad);
    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    if (!read_at(file, 0, header, (int)sizeof(header))) {
        fclose(file);
        return 0;
    }
    if (memcmp(header, "WAD2", 4) == 0) {
        wad->kind = WAD_KIND_WAD2;
    } else if (memcmp(header, "WAD3", 4) == 0) {
        wad->kind = WAD_KIND_WAD3;
    } else {
        fclose(file);
        return 0;
    }

    numlumps = (int)rd32(header + 4);
    info_ofs = (int)rd32(header + 8);
    if (numlumps < 0 || numlumps > 100000) {
        fclose(file);
        return 0;
    }

    strncpy(wad->path, path, PATH_LEN - 1);
    wad->path[PATH_LEN - 1] = '\0';
    strncpy(dir, path, PATH_LEN - 1);
    dir[PATH_LEN - 1] = '\0';
    for (i = (int)strlen(dir) - 1; i > 0; i--) {
        if (dir[i] == '/' || dir[i] == '\\') {
            dir[i] = '\0';
            break;
        }
    }
    if (i <= 0) {
        strncpy(dir, ".", PATH_LEN - 1);
    }

    palette_lump[0] = '\0';

    for (i = 0; i < numlumps; i++) {
        unsigned int pos;
        unsigned int size;
        char name[WAD_NAME_LEN];

        if (!read_at(file, (unsigned int)(info_ofs + i * 32), lump, (int)sizeof(lump))) {
            break;
        }
        pos = rd32(lump);
        size = rd32(lump + 4);
        wad_name_copy(name, lump + 16); /* lumpinfo: 16 byte header, then name[16] */

        if (lump[12] == WAD_LUMP_MIPTEX && wad->count < WAD_MAX_TEX) {
            unsigned char mip[WAD_HEADER_SIZE];

            if (read_at(file, pos, mip, WAD_HEADER_SIZE)) {
                unsigned short w = (unsigned short)rd32(mip + 16);
                unsigned short h = (unsigned short)rd32(mip + 20);

                if (w >= 8 && h >= 8 && w <= 1024 && h <= 1024) {
                    int k = wad->count;

                    wad_name_store(wad->names[k], name);
                    wad->lump_offset[k] = pos;
                    wad->lump_size[k] = size;
                    wad->tex_w[k] = w;
                    wad->tex_h[k] = h;
                    wad->count++;
                }
            }
        } else if ((unsigned int)size == 768 && palette_lump[0] == '\0') {
            wad_name_store(palette_lump, name);
            palette_ofs = pos;
            palette_size = size;
        }
    }

    wad->opened = 1;
    wad_find_palette(wad, dir, palette_lump[0] != '\0' ? palette_lump : NULL,
                     palette_ofs, palette_size);
    fclose(file);
    return wad->count;
}

/* ------------------------------------------------------------------ */
/* Mip levels                                                          */
/* ------------------------------------------------------------------ */
int wad_preview_level(const Wad *wad, int index)
{
    int level = 0;
    int w;
    int h;

    if (index < 0 || index >= wad->count) {
        return 0;
    }
    w = wad->tex_w[index];
    h = wad->tex_h[index];
    while (level < 3 && (w >> (level + 1)) >= 8 && (h >> (level + 1)) >= 8) {
        level++;
    }
    return level;
}

int wad_read_mip(const Wad *wad, int index, int level, unsigned char *out,
                 int cap, int *out_w, int *out_h)
{
    FILE *file;
    unsigned char mip[WAD_HEADER_SIZE];
    unsigned int mip_ofs;
    unsigned int comp_len;
    int w;
    int h;
    int out_size;
    int row;
    int i;

    if (index < 0 || index >= wad->count || level < 0 || level > 3) {
        return 0;
    }
    w = wad->tex_w[index] >> level;
    h = wad->tex_h[index] >> level;
    if (w <= 0 || h <= 0) {
        return 0;
    }
    out_size = w * h;
    if (out_size > cap) {
        return 0;
    }

    file = fopen(wad->path, "rb");
    if (file == NULL) {
        return 0;
    }
    if (!read_at(file, wad->lump_offset[index], mip, WAD_HEADER_SIZE)) {
        fclose(file);
        return 0;
    }
    mip_ofs = wad->lump_offset[index] + rd32(mip + 24 + level * 4);

    /* the compressed data of a small level always fits in the buffer */
    comp_len = wad->lump_size[index];
    if (comp_len > wad->lump_offset[index] + wad->lump_size[index]) {
        comp_len = 0;
    }
    if (mip_ofs < wad->lump_offset[index]) {
        fclose(file);
        return 0;
    }
    comp_len = wad->lump_offset[index] + wad->lump_size[index] - mip_ofs;
    if (comp_len > WAD_COMP_MAX) {
        comp_len = WAD_COMP_MAX;
    }
    if (!read_at(file, mip_ofs, (unsigned char *)wad_comp, (int)comp_len)) {
        fclose(file);
        return 0;
    }
    fclose(file);

    /* RLE: count byte, then 'count' pixels; 255 marks a transparent run */
    row = 0;
    i = 0;
    while (i < (int)comp_len && row < out_size) {
        int count = ((unsigned char *)wad_comp)[i++];

        if (count == 255) {
            if (i >= (int)comp_len) {
                break;
            }
            row += ((unsigned char *)wad_comp)[i++];
        } else {
            int k;
            if (i + count > (int)comp_len) {
                break;
            }
            for (k = 0; k < count && row < out_size; k++) {
                out[row++] = ((unsigned char *)wad_comp)[i++];
            }
        }
    }
    if (out_w != NULL) {
        *out_w = w;
    }
    if (out_h != NULL) {
        *out_h = h;
    }
    return row > 0;
}
