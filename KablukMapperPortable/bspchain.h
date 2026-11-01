/*
 * Kabluk Mapper Portable - bspchain.h
 *
 * Runs the vendored TyrUtils tools (qbsp, light, vis) inside the editor
 * process, one stage at a time, in the usual order:
 *
 *      kabluk.map  --qbsp-->  kabluk.bsp  --light-->  ...  --vis-->  .bsp
 *
 * The tools are plain C programs with a main(); they are entered through
 * tyr_qbsp_main() / tyr_light_main() / tyr_vis_main(). Their errors normally
 * call exit(1); here that becomes a longjmp back into bspchain_run_stage(),
 * so a leaky or broken map only fails the compile instead of closing the
 * editor. Everything they print goes to the log callback.
 *
 * No threads, no child processes: both work on a PSP.
 */
#ifndef KMP_BSPCHAIN_H
#define KMP_BSPCHAIN_H

#include "config.h"

#define BSPCHAIN_IDLE 0
#define BSPCHAIN_QBSP 1
#define BSPCHAIN_LIGHT 2
#define BSPCHAIN_VIS 3
#define BSPCHAIN_DONE 4
#define BSPCHAIN_FAILED 5

typedef struct BspChain {
    int stage;
    int run_qbsp;
    int run_light;
    int run_vis;
    char map_path[PATH_LEN];
    char bsp_path[PATH_LEN];
    char wad_path[PATH_LEN];
    long bsp_bytes;
} BspChain;

void bspchain_init(BspChain *chain);

/* Sets up the three stage paths. Call bspchain_init() first. */
void bspchain_setup(BspChain *chain, const char *map_path, const char *wad_path);

/* Receives every finished line the tools print (no trailing newline). */
void bspchain_set_log(void (*callback)(const char *line));

/* Runs the next enabled stage. Returns 1 while stages remain, 0 when the
 * chain is finished or has failed (check chain->stage). */
int bspchain_run_stage(BspChain *chain);

const char *bspchain_stage_name(int stage);

#endif /* KMP_BSPCHAIN_H */
