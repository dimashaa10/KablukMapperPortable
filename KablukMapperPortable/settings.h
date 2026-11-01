/*
 * Kabluk Mapper Portable - settings.h
 *
 * Editor settings: where the .wad lives, where the compiled .map/.bsp files
 * should go, what the map source is called, and which textures the floor and
 * the ceiling of the map get. Kept in memory for now; saving to a small cfg
 * file is a later step (and on the PSP it would live next to the EBOOT).
 */
#ifndef KMP_SETTINGS_H
#define KMP_SETTINGS_H

#include "config.h"
#include "fs.h"

typedef struct Settings {
    char wad_path[PATH_LEN]; /* full path to a .wad file, "" = not set */
    char out_dir[PATH_LEN];  /* folder for compiled maps, "." = current */

    /* name of the .map written by the exporter, without the extension */
    char map_name[FS_NAME_LEN];

    /* textures of the two slabs, "" = first texture of the wad */
    char floor_tex[WAD_NAME_LEN];
    char ceiling_tex[WAD_NAME_LEN];
} Settings;

void settings_defaults(Settings *settings);
int settings_wad_is_set(const Settings *settings);

/* The map file name without extension, sanitised for the filesystem.
 * "kabluk" when the field is empty. */
void settings_map_name(const Settings *settings, char *out, int cap);

/* "<out_dir>/<map_name>.map" - what the exporter writes. */
void settings_map_path(const Settings *settings, char *out, int cap);

#endif /* KMP_SETTINGS_H */
