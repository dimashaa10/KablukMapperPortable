/*
 * Kabluk Mapper Portable - mapfile.h
 *
 * Reads a Quake .map back into the editor: the "Load map" entry of the main
 * menu. The reader understands the format this editor writes (entities with
 * key/value pairs, and brushes made of six axis aligned planes) and is
 * tolerant about everything else: any brush whose planes describe an axis
 * aligned box is snapped onto the cell grid, and brushes that are not a plain
 * block (the floor / ceiling slabs, anything diagonal) are skipped, because
 * the exporter writes those again on its own.
 */
#ifndef KMP_MAPFILE_H
#define KMP_MAPFILE_H

#include "config.h"
#include "fgd.h"
#include "grid.h"
#include "values.h"
#include "wad.h"

typedef struct MapFile {
    int ok;

    int brushes;     /* brush blocks found in the file       */
    int cells;       /* cells painted on the grid            */
    int entities;    /* point entities placed                */
    int skipped;     /* brushes that were not a plain block  */
    int tex_missing; /* textures that the wad does not have  */

    char wad[PATH_LEN]; /* worldspawn "wad" key, "" when absent */
    char error[64];     /* why it failed, "" when ok            */
} MapFile;

/* Loads 'path' into 'grid' / 'values'. 'wad' may be NULL: the textures are
 * then left unset (the exporter falls back to the first wad texture).
 * Returns 1 on success, 0 on failure (info->error says why). */
int mapfile_load(const char *path, Grid *grid, const Fgd *fgd,
                 ValueStore *values, const Wad *wad, MapFile *info);

/* Reads only the worldspawn "wad" key, so the wad can be loaded before the
 * textures are looked up. Returns 1 when the key was found. */
int mapfile_peek_wad(const char *path, char *out, int cap);

/* Index of 'name' in the wad, -1 when it is not there (case insensitive). */
int mapfile_tex_index(const Wad *wad, const char *name);

#endif /* KMP_MAPFILE_H */
