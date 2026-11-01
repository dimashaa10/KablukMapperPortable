/*
 * Kabluk Mapper Portable - compile.h
 *
 * The compile job behind the START menu. It runs inside the editor process
 * (no threads, no external tools), one small chunk per frame, so the log
 * window updates while the PSP keeps drawing at 60 fps.
 *
 * The pipeline:
 *   1. counts blocks / entities
 *   2. checks the map is sealed (the shell ring must be intact)
 *   3. writes a standard Quake .map into the output folder
 *   4. runs the vendored TyrUtils tools on it: qbsp -> light -> vis
 *
 * Steps 1-3 are plain file writing, so they are portable by construction.
 * Step 4 calls the TyrUtils entry points directly (see bspchain.c) - no
 * child processes, which is what makes the whole chain work on a PSP.
 */
#ifndef KMP_COMPILE_H
#define KMP_COMPILE_H

#include "bspchain.h"
#include "config.h"
#include "fgd.h"
#include "grid.h"
#include "settings.h"
#include "values.h"
#include "wad.h"

#define COMPILE_LOG_LINES 80
#define COMPILE_LOG_LEN 80

/* map-wide minimum light written into worldspawn ("light" key, minlight) */
#define DEFAULT_MINLIGHT 200

/* worldspawn "message": the name the engine shows in the level list */
#define DEFAULT_MAP_MESSAGE "Kabluk map"

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
    int wall_total;   /* wall runs the exporter adds around the empty border */
    int save_only;    /* 1 = write the .map and stop, do not run the tools */
    int entity_total;
    int entity_done;

    /* run merging cursors */
    int cur_row;
    int cur_col;
    int in_entity_pass;

    BspChain chain; /* qbsp -> light -> vis */
    int chain_started;
    int light_entities;

    void *file; /* FILE *, hidden so the header stays plain C */
    long bytes;
} CompileJob;

void compile_init(CompileJob *job);
int compile_is_running(const CompileJob *job);
const char *compile_status_text(const CompileJob *job);

/* Clears the log and starts over. The values the user typed are handed to
 * compile_step, which is where the .map gets written.
 * save_only = 1 writes the .map source and stops there ("save as", from the
 * editor settings); 0 also runs qbsp -> light -> vis. */
void compile_start(CompileJob *job, const Grid *grid, const Fgd *fgd,
                   const Settings *settings, const Wad *wad, int save_only);

/* Advances the job by one chunk; call once per frame while it runs. */
void compile_step(CompileJob *job, const Grid *grid, const Fgd *fgd,
                  const Settings *settings, const Wad *wad,
                  const ValueStore *values);

void compile_log_add(CompileJob *job, const char *line);
void compile_scroll(CompileJob *job, int delta);

#endif /* KMP_COMPILE_H */
