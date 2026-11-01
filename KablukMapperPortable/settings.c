/*
 * Kabluk Mapper Portable - settings.c
 */
#include "settings.h"

#include "fs.h"

void settings_defaults(Settings *settings)
{
    settings->wad_path[0] = '\0';
    fs_copy(settings->out_dir, PATH_LEN, DEFAULT_OUT_DIR);
}

int settings_wad_is_set(const Settings *settings)
{
    return settings->wad_path[0] != '\0';
}
