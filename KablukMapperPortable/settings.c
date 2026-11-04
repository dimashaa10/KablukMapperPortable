/*
 * Kabluk Mapper Portable - settings.c
 */
#include "settings.h"

#include <stddef.h> /* NULL */
#include <string.h>  /* memset */

#include "fs.h"

void settings_defaults(Settings *settings)
{
    if (settings == NULL) {
        return;
    }
    /* Every field is cleared, not just its first byte: what is left of a
     * longer value would otherwise keep turning up in the file and in the
     * "did anything change" check. */
    memset(settings, 0, sizeof(*settings));
    settings->wad_path[0] = '\0';
    fs_copy(settings->out_dir, PATH_LEN, DEFAULT_OUT_DIR);
    fs_copy(settings->map_name, FS_NAME_LEN, DEFAULT_MAP_NAME);
    settings->floor_tex[0] = '\0';
    settings->ceiling_tex[0] = '\0';
}

int settings_wad_is_set(const Settings *settings)
{
    return settings->wad_path[0] != '\0';
}

void settings_map_name(const Settings *settings, char *out, int cap)
{
    if (out == NULL || cap <= 0) {
        return;
    }
    fs_sanitize_name(out, cap, settings != NULL ? settings->map_name : "");
    if (out[0] == '\0') {
        fs_copy(out, cap, DEFAULT_MAP_NAME);
    }
}

void settings_map_path(const Settings *settings, char *out, int cap)
{
    char clean[FS_NAME_LEN];
    const char *dir;
    int len = 0;
    int i;

    if (out == NULL || cap <= 0) {
        return;
    }
    dir = (settings != NULL && settings->out_dir[0] != '\0')
              ? settings->out_dir
              : ".";
    settings_map_name(settings, clean, (int)sizeof(clean));

    for (i = 0; dir[i] != '\0' && len < cap - 1; i++) {
        out[len++] = dir[i];
    }
    if (len > 0 && out[len - 1] != '/' && out[len - 1] != '\\' && len < cap - 1) {
        out[len++] = '/';
    }
    for (i = 0; clean[i] != '\0' && len < cap - 1; i++) {
        out[len++] = clean[i];
    }
    for (i = 0; i < 4 && len < cap - 1; i++) {
        out[len++] = ".map"[i];
    }
    out[len] = '\0';
}
