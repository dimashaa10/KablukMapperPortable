/*
 * Kabluk Mapper Portable - bspchain.c
 *
 * Glue between the editor and the vendored TyrUtils tools.
 */
#include "bspchain.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fs.h"

/* The tools keep their main(), renamed by vendor/tyrutils/include/common/tyr_port.h */
int tyr_qbsp_main(int argc, char **argv);
int tyr_light_main(int argc, const char **argv);
int tyr_vis_main(int argc, char **argv);

/* Declared in vendor/tyrutils/include/common/tyr_port.h */
void (*tyr_log_hook)(const char *text) = NULL;

#if defined(KMP_HAVE_PSP) || defined(__PSP__)
#include <pspkernel.h> /* sceKernelMaxFreeMemSize */
#endif

/* The number the tools print next to a load (see tyr_port.h).
 *
 * Read it as "the user partition has this much left over", not "the tools have
 * this much to spend": pspdev gives malloc one block, sized from what was free
 * the first time anything allocated and leaving a fixed 512 KB behind for
 * everybody else, so once the editor has started this is 512 and it does not
 * move. If it ever reads like the whole partition (tens of megabytes) the heap
 * block could not be created at all and every malloc fails. The real budget is
 * printed once at startup, in SDL_Log.txt ("user memory before startup"). */
int tyr_free_kb(void)
{
#if defined(KMP_HAVE_PSP) || defined(__PSP__)
    return (int)(sceKernelMaxFreeMemSize() / 1024);
#else
    return -1;
#endif
}

#define CHAIN_LINE_LEN 120
#define CHAIN_ARG_MAX 8

static jmp_buf chain_jmp;
static int chain_armed = 0;

static void (*chain_line_cb)(const char *line) = NULL;
static char chain_line[CHAIN_LINE_LEN];
static int chain_line_len = 0;

/* ------------------------------------------------------------------ */
/* Log plumbing                                                        */
/* ------------------------------------------------------------------ */
static void chain_line_flush(void)
{
    int start = 0;
    int i;
    int has_alpha = 0;

    if (chain_line_len == 0) {
        return;
    }
    chain_line[chain_line_len] = '\0';

    /* the tools pad and print progress bars - drop what carries no text */
    while (chain_line[start] == ' ' || chain_line[start] == '\t') {
        start++;
    }
    for (i = start; chain_line[i] != '\0'; i++) {
        char c = chain_line[i];

        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) {
            has_alpha = 1;
            break;
        }
    }
    if (has_alpha && chain_line_cb != NULL) {
        chain_line_cb(chain_line + start);
    }
    chain_line_len = 0;
}

static void chain_line_add(const char *text)
{
    while (*text != '\0') {
        char c = *text++;

        if (c == '\n') {
            chain_line_flush();
        } else if (c == '\r') {
            /* progress ticks - keep the current line only */
            chain_line_len = 0;
        } else if (chain_line_len < CHAIN_LINE_LEN - 1) {
            chain_line[chain_line_len++] = c;
        }
    }
}

/*
 * qbsp/light/vis call exit(1) on fatal errors. While a stage is running we
 * jump back into bspchain_run_stage() instead of killing the editor.
 */
void tyr_fatal_exit(void)
{
    if (chain_armed) {
        chain_armed = 0;
        longjmp(chain_jmp, 1);
    }
    exit(1);
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */
static void chain_replace_extension(char *path, const char *extension)
{
    int len = (int)strlen(path);
    int i = len;
    int slash = -1;

    while (i > 0) {
        i--;
        if (path[i] == '/' || path[i] == '\\') {
            slash = i;
            break;
        }
    }
    i = len;
    while (i > slash + 1) {
        i--;
        if (path[i] == '.') {
            path[i] = '\0';
            break;
        }
    }
    if ((int)strlen(path) + (int)strlen(extension) < PATH_LEN) {
        strcat(path, extension);
    }
}

static long file_size(const char *path)
{
    FILE *file = fopen(path, "rb");
    long size = -1;

    if (file != NULL) {
        if (fseek(file, 0, SEEK_END) == 0) {
            size = ftell(file);
        }
        fclose(file);
    }
    return size;
}

/* ------------------------------------------------------------------ */
/* Stage running                                                       */
/* ------------------------------------------------------------------ */
/* qbsp looks for the worldspawn "wad" key relative to the directory of the
 * .map file, which is not where the editor keeps it: the wad was picked
 * relative to the editor, so tell qbsp that directory. Without this every
 * texture is reported missing and the .bsp ends up without any miptex. */
static int run_qbsp(const char *map_path, const char *wad_path)
{
    char wad_dir[PATH_LEN];
    char *argv[5];
    int argc = 0;
    int rc;

    argv[argc++] = (char *)"qbsp";
    fs_copy(wad_dir, PATH_LEN, wad_path != NULL ? wad_path : "");
    if (wad_dir[0] != '\0') {
        fs_parent(wad_dir, PATH_LEN);
        if (wad_dir[0] != '\0') {
            argv[argc++] = (char *)"-wadpath";
            argv[argc++] = wad_dir;
        }
    }
    argv[argc++] = (char *)map_path;
    argv[argc] = NULL;

    if (setjmp(chain_jmp) == 0) {
        chain_armed = 1;
        rc = tyr_qbsp_main(argc, argv);
        chain_armed = 0;
    } else {
        rc = 1;
    }
    chain_line_flush();
    return rc;
}

static int run_light(const char *bsp_path)
{
    char *argv[4];
    int rc;

    argv[0] = (char *)"light";
    argv[1] = (char *)"-threads"; /* single thread keeps the PSP happy */
    argv[2] = (char *)"1";
    argv[3] = (char *)bsp_path;

    if (setjmp(chain_jmp) == 0) {
        chain_armed = 1;
        rc = tyr_light_main(4, (const char **)argv);
        chain_armed = 0;
    } else {
        rc = 1;
    }
    chain_line_flush();
    return rc;
}

static int run_vis(const char *bsp_path)
{
    char *argv[4];
    int rc;

    argv[0] = (char *)"vis";
    argv[1] = (char *)"-threads"; /* single thread keeps the PSP happy */
    argv[2] = (char *)"1";
    argv[3] = (char *)bsp_path;

    if (setjmp(chain_jmp) == 0) {
        chain_armed = 1;
        rc = tyr_vis_main(4, argv);
        chain_armed = 0;
    } else {
        rc = 1;
    }
    chain_line_flush();
    return rc;
}

void bspchain_init(BspChain *chain)
{
    memset(chain, 0, sizeof(*chain));
    chain->stage = BSPCHAIN_IDLE;
    chain->run_qbsp = 1;
    chain->run_light = 1;
    chain->run_vis = 1;
    chain->bsp_bytes = 0;
    chain_line_len = 0;
    tyr_log_hook = chain_line_add;
}

void bspchain_setup(BspChain *chain, const char *map_path, const char *wad_path)
{
    fs_copy(chain->map_path, PATH_LEN, map_path);
    fs_copy(chain->bsp_path, PATH_LEN, map_path);
    chain_replace_extension(chain->bsp_path, ".bsp");
    fs_copy(chain->wad_path, PATH_LEN, wad_path != NULL ? wad_path : "");
    chain->bsp_bytes = 0;
    chain->stage = BSPCHAIN_QBSP;
}

void bspchain_set_log(void (*callback)(const char *line))
{
    chain_line_cb = callback;
}

const char *bspchain_stage_name(int stage)
{
    switch (stage) {
    case BSPCHAIN_QBSP:
        return "QBSP";
    case BSPCHAIN_LIGHT:
        return "LIGHT";
    case BSPCHAIN_VIS:
        return "VIS";
    case BSPCHAIN_DONE:
        return "DONE";
    case BSPCHAIN_FAILED:
        return "FAILED";
    default:
        return "IDLE";
    }
}

int bspchain_run_stage(BspChain *chain)
{
    int rc = 0;
    int stage = chain->stage;

    while (stage == BSPCHAIN_QBSP && !chain->run_qbsp) {
        stage++;
    }
    while (stage == BSPCHAIN_LIGHT && !chain->run_light) {
        stage++;
    }
    while (stage == BSPCHAIN_VIS && !chain->run_vis) {
        stage++;
    }
    chain->stage = stage;

    if (chain->stage == BSPCHAIN_QBSP) {
        rc = run_qbsp(chain->map_path, chain->wad_path);
    } else if (chain->stage == BSPCHAIN_LIGHT) {
        rc = run_light(chain->bsp_path);
    } else if (chain->stage == BSPCHAIN_VIS) {
        rc = run_vis(chain->bsp_path);
    } else {
        return 0;
    }

    if (rc != 0) {
        chain->stage = BSPCHAIN_FAILED;
        return 0;
    }

    chain->bsp_bytes = file_size(chain->bsp_path);
    if (chain->bsp_bytes <= 0) {
        chain->stage = BSPCHAIN_FAILED;
        return 0;
    }

    chain->stage++;
    if (chain->stage >= BSPCHAIN_DONE) {
        chain->stage = BSPCHAIN_DONE;
        return 0;
    }
    return 1;
}
