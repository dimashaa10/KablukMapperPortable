/*
 * Kabluk Mapper Portable - settings.h
 *
 * Editor settings: where the .wad lives and where the compiled .bsp/.map
 * files should go. Kept in memory for now; saving to a small cfg file is a
 * later step (and on the PSP it would live next to the EBOOT).
 */
#ifndef KMP_SETTINGS_H
#define KMP_SETTINGS_H

#include "config.h"

typedef struct Settings {
    char wad_path[PATH_LEN]; /* full path to a .wad file, "" = not set */
    char out_dir[PATH_LEN];  /* folder for compiled maps, "." = current */
} Settings;

void settings_defaults(Settings *settings);
int settings_wad_is_set(const Settings *settings);

#endif /* KMP_SETTINGS_H */
