/*
 * Host test: the settings file behind "pick a floor texture and it is still
 * there next time" (prefs.c).
 *
 * What matters is that every setting survives a save/load round trip, that a
 * value may hold spaces, ':' and '=' (they are paths, and the PSP's are
 * "ms0:/PSP/GAME/..."), that a missing or damaged file leaves the defaults
 * alone instead of breaking the editor, and that a change is noticed.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "prefs.h"
#include "settings.h"

#define TEST_CWD_ENV "KMP_TEST_CWD"

static int failures;

static void check(int ok, const char *what)
{
    if (ok) {
        printf("%-56s ok\n", what);
    } else {
        printf("%-56s FAIL\n", what);
        failures++;
    }
}

static void write_file(const char *text)
{
    FILE *file = fopen(PREFS_FILE_NAME, "wb");

    if (file == NULL) {
        return;
    }
    fputs(text, file);
    fclose(file);
}

int main(void)
{
    const char *cwd = getenv(TEST_CWD_ENV);
    Settings settings;
    Settings back;
    Settings other;

    if (cwd != NULL) {
        if (chdir(cwd) != 0) {
            printf("cannot enter %s\n", cwd);
            return 1;
        }
    }
    remove(PREFS_FILE_NAME);

    printf("== settings file\n");

    settings_defaults(&settings);
    check(prefs_load(&settings) == 0, "no file: nothing is loaded");
    check(prefs_same(&settings, &settings), "with no file the settings stay put");

    /* the four choices the user makes with the picker, the browser and the
     * keyboard, in the form a PSP path takes */
    strcpy(settings.wad_path, "ms0:/PSP/GAME/KABLUKmp/quake.wad");
    strcpy(settings.out_dir, "ms0:/PSP/GAME/KABLUKmp/maps");
    strcpy(settings.map_name, "qwe1");
    strcpy(settings.floor_tex, "brown");
    strcpy(settings.ceiling_tex, "sky1");

    check(prefs_save(&settings) == 1, "the settings are written");

    settings_defaults(&back);
    check(prefs_load(&back) == 5, "all five are read back");
    check(strcmp(back.wad_path, "ms0:/PSP/GAME/KABLUKmp/quake.wad") == 0,
          "the wad path keeps its colons and slashes");
    check(strcmp(back.out_dir, settings.out_dir) == 0, "the output folder");
    check(strcmp(back.map_name, "qwe1") == 0, "the map name");
    check(strcmp(back.floor_tex, "brown") == 0,
          "the floor texture chosen in the picker");
    check(strcmp(back.ceiling_tex, "sky1") == 0,
          "the ceiling texture chosen in the picker");
    check(prefs_same(&settings, &back) == 1,
          "a reloaded file compares equal to what was saved");

    /* a value with spaces and an '=' in it: only the first '=' splits */
    strcpy(settings.floor_tex, "a=b c");
    prefs_save(&settings);
    settings_defaults(&back);
    prefs_load(&back);
    check(strcmp(back.floor_tex, "a=b c") == 0, "a value may hold '=' and spaces");

    /* an unknown key, a comment and a blank line are skipped, not fatal */
    write_file("# a comment\n\nfuturekey=42\nwad=tex.wad\nbroken line\n"
               "floor=brown\n");
    settings_defaults(&back);
    check(prefs_load(&back) == 2, "unknown keys and junk lines are ignored");
    check(strcmp(back.wad_path, "tex.wad") == 0, "the known keys still load");
    check(strcmp(back.floor_tex, "brown") == 0, "and so does the floor texture");
    check(back.out_dir[0] != '\0', "a setting the file omits keeps its default");

    /* a path longer than the field is cut, not spilled */
    {
        char long_path[PATH_LEN + 60];
        int i;

        for (i = 0; i < (int)sizeof(long_path) - 1; i++) {
            long_path[i] = 'p';
        }
        long_path[sizeof(long_path) - 1] = '\0';
        write_file("");
        {
            FILE *file = fopen(PREFS_FILE_NAME, "wb");
            fprintf(file, "wad=%s\n", long_path);
            fclose(file);
        }
        settings_defaults(&back);
        prefs_load(&back);
        check((int)strlen(back.wad_path) == PATH_LEN - 1,
              "a too long path is truncated to the field");
    }

    /* trimming: "floor=brown   " is still "brown" */
    write_file("floor=brown   \n");
    settings_defaults(&back);
    prefs_load(&back);
    check(strcmp(back.floor_tex, "brown") == 0, "trailing spaces are trimmed");

    /* a changed field is seen even when only one is different */
    settings_defaults(&other);
    other = back;
    check(prefs_same(&other, &back) == 1, "a copy compares equal");
    other.floor_tex[0] = 'x';
    check(prefs_same(&other, &back) == 0, "one differing texture is noticed");

    remove(PREFS_FILE_NAME);
    printf("\n%s (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
