/*
 * Kabluk Mapper Portable - compile.h
 *
 * The compile job behind the START menu. It runs inside the editor process
 * (no threads, no external tools), one small chunk per frame, so the log
 * window updates while the PSP keeps drawing at 60 fps.
 *
 * What it does today:
 *   1. counts blocks / entities
 *   2. checks the map is sealed (the shell ring must be intact)
 *   3. writes a standard Quake .map into the output folder
 *
 * The .map is the portable middle step: on the desktop it can be fed to
 * qbsp/light/vis, and later a built-in BSP writer can grow out of the same
 * geometry pass.
 */
#ifndef KMP_COMPILE_H
#define KMP_COMPILE_H

#include "config.h"
#include "fgd.h"
#include "grid.h"
#include "settings.h"
#include "wad.h"

#define COMPILE_LOG_LINES 80
#define COMPILE_LOG_LEN 80

#define COMPILE_IDLE 0
#define COMPILE_RUNNING 1
#define COMPILE_DONE 2
#define COMPILE_FAILED 3

typedef struct CompileJob {
    int state;
    int step;
    int progress; /* 0..100 */

    char log[COMPILE_LOG_LINES][COMPILE_LOG_LEN];
    int line_count;
    int scroll;

    char out_dir[PATH_LEN];
    char map_path[PATH_LEN];

    int brush_total;
    int brush_done;
    int entity_total;
    int entity_done;

    /* run merging cursors */
    int cur_row;
    int cur_col;
    int in_entity_pass;

    void *file; /* FILE *, hidden so the header stays plain C */
    long bytes;
} CompileJob;

void compile_init(CompileJob *job);
int compile_is_running(const CompileJob *job);
const char *compile_status_text(const CompileJob *job);

/* Clears the log and starts over. */
void compile_start(CompileJob *job, const Grid *grid, const Fgd *fgd,
                   const Settings *settings, const Wad *wad);

/* Advances the job by one chunk; call once per frame while it runs. */
void compile_step(CompileJob *job, const Grid *grid, const Fgd *fgd,
                  const Settings *settings, const Wad *wad);

void compile_log_add(CompileJob *job, const char *line);
void compile_scroll(CompileJob *job, int delta);

#endif /* KMP_COMPILE_H */
