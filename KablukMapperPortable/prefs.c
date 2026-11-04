/*
 * Kabluk Mapper Portable - prefs.c
 *
 * See prefs.h. Plain stdio, no allocation: it runs at startup and after a
 * settings change on a console whose memory is spoken for.
 */
#include "prefs.h"

#include <stdio.h>
#include <string.h>

/* one line: the longest value plus "key=" and slack */
#define PREFS_LINE (PATH_LEN + 32)

/* Copies 'src' into a fixed field, truncated. src == NULL means "empty". */
static void prefs_copy(char *dst, int cap, const char *src)
{
    int i = 0;

    if (dst == NULL || cap <= 0) {
        return;
    }
    if (src != NULL) {
        while (src[i] != '\0' && i < cap - 1) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

/* Drops trailing spaces, tabs and line endings, in place. */
static void prefs_trim(char *text)
{
    int len;

    if (text == NULL) {
        return;
    }
    len = (int)strlen(text);
    while (len > 0) {
        char c = text[len - 1];

        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            break;
        }
        len--;
    }
    text[len] = '\0';
}

int prefs_load(Settings *settings)
{
    char line[PREFS_LINE];
    FILE *file;
    int found = 0;

    if (settings == NULL) {
        return 0;
    }
    file = fopen(PREFS_FILE_NAME, "r");
    if (file == NULL) {
        return 0;
    }

    while (fgets(line, (int)sizeof(line), file) != NULL) {
        char *value;
        char *eq = strchr(line, '=');

        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }
        if (eq == NULL) {
            continue;
        }
        *eq = '\0';
        value = eq + 1;
        prefs_trim(line);  /* the key */
        prefs_trim(value); /* the value */

        if (strcmp(line, "wad") == 0) {
            prefs_copy(settings->wad_path, PATH_LEN, value);
            found++;
        } else if (strcmp(line, "out") == 0) {
            prefs_copy(settings->out_dir, PATH_LEN, value);
            found++;
        } else if (strcmp(line, "name") == 0) {
            prefs_copy(settings->map_name, FS_NAME_LEN, value);
            found++;
        } else if (strcmp(line, "floor") == 0) {
            prefs_copy(settings->floor_tex, WAD_NAME_LEN, value);
            found++;
        } else if (strcmp(line, "ceiling") == 0) {
            prefs_copy(settings->ceiling_tex, WAD_NAME_LEN, value);
            found++;
        }
        /* anything else is a setting this build does not know: ignore it */
    }
    fclose(file);
    return found;
}

int prefs_save(const Settings *settings)
{
    FILE *file;

    if (settings == NULL) {
        return 0;
    }
    file = fopen(PREFS_FILE_NAME, "w");
    if (file == NULL) {
        return 0;
    }
    fprintf(file, "# Kabluk Mapper Portable - editor settings\n");
    fprintf(file, "# written by the editor when something here changes\n");
    fprintf(file, "wad=%s\n", settings->wad_path);
    fprintf(file, "out=%s\n", settings->out_dir);
    fprintf(file, "name=%s\n", settings->map_name);
    fprintf(file, "floor=%s\n", settings->floor_tex);
    fprintf(file, "ceiling=%s\n", settings->ceiling_tex);
    if (fflush(file) != 0) {
        fclose(file);
        return 0;
    }
    return fclose(file) == 0;
}

int prefs_same(const Settings *a, const Settings *b)
{
    if (a == NULL || b == NULL) {
        return 0;
    }
    /* Field by field, not memcmp: a shorter value leaves the tail of the
     * field holding the end of the longer one it replaced, and those bytes are
     * not part of the setting. */
    return strcmp(a->wad_path, b->wad_path) == 0 &&
           strcmp(a->out_dir, b->out_dir) == 0 &&
           strcmp(a->map_name, b->map_name) == 0 &&
           strcmp(a->floor_tex, b->floor_tex) == 0 &&
           strcmp(a->ceiling_tex, b->ceiling_tex) == 0;
}
