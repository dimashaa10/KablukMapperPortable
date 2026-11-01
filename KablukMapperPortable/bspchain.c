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
static int run_qbsp(const char *map_path)
{
    char *argv[3];
    int rc;

    argv[0] = (char *)"qbsp";
    argv[1] = (char *)map_path;
    argv[2] = NULL;

    if (setjmp(chain_jmp) == 0) {
        chain_armed = 1;
        rc = tyr_qbsp_main(2, argv);
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
    char *argv[3];
    int rc;

    argv[0] = (char *)"vis";
    argv[1] = (char *)bsp_path;
    argv[2] = NULL;

    if (setjmp(chain_jmp) == 0) {
        chain_armed = 1;
        rc = tyr_vis_main(2, argv);
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
        rc = run_qbsp(chain->map_path);
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
