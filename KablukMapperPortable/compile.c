/*
 * Kabluk Mapper Portable - compile.c
 *
 * Grid -> Quake .map exporter with a step-by-step log.
 *
 * Geometry: every solid cell is a 128x128x128 block standing on z = 0.
 * Neighbouring cells that share a texture are merged into one brush per row,
 * which keeps the .map (and later the BSP) a lot smaller. The whole map also
 * gets a floor slab below z = 0 and a ceiling slab above z = 128, so nothing
 * can leak into the void.
 */
#include "compile.h"

#include <stdio.h>
#include <string.h>

#define MAP_NAME "kabluk"
#define SLAB_THICKNESS 16
#define BRUSHES_PER_STEP 64

/* ------------------------------------------------------------------ */
/* Text helpers (no snprintf: keeps this C89 and PSP friendly)         */
/* ------------------------------------------------------------------ */
static int cat_str(char *buf, int len, const char *text)
{
    int i = 0;

    while (text[i] != '\0' && len < COMPILE_LOG_LEN - 1) {
        buf[len++] = text[i++];
    }
    buf[len] = '\0';
    return len;
}

static int cat_int(char *buf, int len, int value)
{
    char tmp[12];
    int n = 0;
    int i;
    int digits = 1;

    if (value < 0) {
        if (len < COMPILE_LOG_LEN - 1) {
            buf[len++] = '-';
        }
        value = -value;
    }
    while (digits * 10 <= value && digits < 1000000) {
        digits *= 10;
    }
    while (digits > 0) {
        tmp[n++] = (char)('0' + (value / digits) % 10);
        digits /= 10;
    }
    for (i = 0; i < n && len < COMPILE_LOG_LEN - 1; i++) {
        buf[len++] = tmp[i];
    }
    buf[len] = '\0';
    return len;
}

void compile_log_add(CompileJob *job, const char *line)
{
    if (job->line_count >= COMPILE_LOG_LINES) {
        int i;
        for (i = 1; i < COMPILE_LOG_LINES; i++) {
            strcpy(job->log[i - 1], job->log[i]);
        }
        job->line_count = COMPILE_LOG_LINES - 1;
        if (job->scroll > 0) {
            job->scroll--;
        }
    }
    strncpy(job->log[job->line_count], line, COMPILE_LOG_LEN - 1);
    job->log[job->line_count][COMPILE_LOG_LEN - 1] = '\0';
    job->line_count++;
}

void compile_scroll(CompileJob *job, int delta)
{
    /* scroll counts lines scrolled up from the newest line */
    int max_scroll = job->line_count - 10;

    if (max_scroll < 0) {
        max_scroll = 0;
    }
    job->scroll += delta;
    if (job->scroll > max_scroll) {
        job->scroll = max_scroll;
    }
    if (job->scroll < 0) {
        job->scroll = 0;
    }
}

void compile_init(CompileJob *job)
{
    job->state = COMPILE_IDLE;
    job->step = 0;
    job->progress = 0;
    job->line_count = 0;
    job->scroll = 0;
    job->out_dir[0] = '\0';
    job->map_path[0] = '\0';
    job->brush_total = 0;
    job->brush_done = 0;
    job->entity_total = 0;
    job->entity_done = 0;
    job->cur_row = 0;
    job->cur_col = 0;
    job->in_entity_pass = 0;
    job->file = NULL;
    job->bytes = 0;
}

int compile_is_running(const CompileJob *job)
{
    return job->state == COMPILE_RUNNING;
}

const char *compile_status_text(const CompileJob *job)
{
    switch (job->state) {
    case COMPILE_RUNNING:
        return "COMPILING";
    case COMPILE_DONE:
        return "DONE";
    case COMPILE_FAILED:
        return "FAILED";
    default:
        return "READY";
    }
}

/* ------------------------------------------------------------------ */
/* Geometry helpers                                                    */
/* ------------------------------------------------------------------ */
static int cell_texture(const Grid *grid, int cx, int cy)
{
    return (int)grid->tex[cy][cx];
}

static int count_solid(const Grid *grid)
{
    int x;
    int y;
    int n = 0;

    for (y = 0; y < MAP_ROWS; y++) {
        for (x = 0; x < MAP_COLS; x++) {
            if (grid->solid[y][x] != CELL_EMPTY) {
                n++;
            }
        }
    }
    return n;
}

/* number of brushes: one per horizontal run of cells with the same texture */
static int count_runs(const Grid *grid)
{
    int x;
    int y;
    int n = 0;
    int prev_tex;

    for (y = 0; y < MAP_ROWS; y++) {
        prev_tex = -2;
        for (x = 0; x < MAP_COLS; x++) {
            if (grid->solid[y][x] == CELL_EMPTY) {
                prev_tex = -2;
                continue;
            }
            if (cell_texture(grid, x, y) != prev_tex) {
                n++;
                prev_tex = cell_texture(grid, x, y);
            }
        }
    }
    return n;
}

static int count_entities(const Grid *grid)
{
    int x;
    int y;
    int n = 0;

    for (y = 0; y < MAP_ROWS; y++) {
        for (x = 0; x < MAP_COLS; x++) {
            if (grid->entity[y][x] != 0) {
                n++;
            }
        }
    }
    return n;
}

/* Finds the next horizontal run of solid cells. job->cur_row / cur_col walk
 * through the map once and are advanced by this call. Returns 0 when the map
 * is finished. */
static int next_run(const Grid *grid, CompileJob *job, int *x0, int *x1,
                    int *tex)
{
    while (job->cur_row < MAP_ROWS) {
        int y = job->cur_row;
        int x = job->cur_col;
        int start;
        int t;

        while (x < MAP_COLS && grid->solid[y][x] == CELL_EMPTY) {
            x++;
        }
        if (x >= MAP_COLS) {
            job->cur_row++;
            job->cur_col = 0;
            continue;
        }
        start = x;
        t = cell_texture(grid, x, y);
        while (x + 1 < MAP_COLS && grid->solid[y][x + 1] != CELL_EMPTY &&
               cell_texture(grid, x + 1, y) == t) {
            x++;
        }
        job->cur_col = x + 1;
        *x0 = start;
        *x1 = x;
        *tex = t;
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* File writing                                                        */
/* ------------------------------------------------------------------ */
static void write_text(CompileJob *job, const char *text)
{
    FILE *file = (FILE *)job->file;
    int len = (int)strlen(text);

    if (file == NULL) {
        return;
    }
    fwrite(text, 1, (size_t)len, file);
    job->bytes += len;
}

/* One brush: an axis aligned box with 6 outward facing planes. */
static void write_box(CompileJob *job, int minx, int miny, int minz, int maxx,
                      int maxy, int maxz, const char *texture)
{
    char buf[256];

    write_text(job, "{\n");
    /* +X */
    sprintf(buf, "( %d %d %d ) ( %d %d %d ) ( %d %d %d ) %s 0 0 0 1 1\n", maxx,
            miny, minz, maxx, miny, maxz, maxx, maxy, maxz, texture);
    write_text(job, buf);
    /* -X */
    sprintf(buf, "( %d %d %d ) ( %d %d %d ) ( %d %d %d ) %s 0 0 0 1 1\n", minx,
            miny, minz, minx, maxy, minz, minx, miny, maxz, texture);
    write_text(job, buf);
    /* +Y */
    sprintf(buf, "( %d %d %d ) ( %d %d %d ) ( %d %d %d ) %s 0 0 0 1 1\n", minx,
            maxy, minz, maxx, maxy, maxz, minx, maxy, maxz, texture);
    write_text(job, buf);
    /* -Y */
    sprintf(buf, "( %d %d %d ) ( %d %d %d ) ( %d %d %d ) %s 0 0 0 1 1\n", minx,
            miny, minz, maxx, miny, maxz, maxx, miny, minz, texture);
    write_text(job, buf);
    /* +Z */
    sprintf(buf, "( %d %d %d ) ( %d %d %d ) ( %d %d %d ) %s 0 0 0 1 1\n", minx,
            miny, maxz, maxx, maxy, maxz, maxx, miny, maxz, texture);
    write_text(job, buf);
    /* -Z */
    sprintf(buf, "( %d %d %d ) ( %d %d %d ) ( %d %d %d ) %s 0 0 0 1 1\n", minx,
            miny, minz, maxx, miny, minz, maxx, maxy, minz, texture);
    write_text(job, buf);
    write_text(job, "}\n");
}

static const char *wad_fallback_texture(const Wad *wad)
{
    if (wad != NULL && wad->count > 0) {
        return wad->names[0];
    }
    return "brown";
}

static const char *brush_texture(const Grid *grid, const Wad *wad, int cx,
                                 int cy)
{
    int index = (int)grid->tex[cy][cx];

    if (wad != NULL && index >= 0 && index < wad->count) {
        return wad->names[index];
    }
    return wad_fallback_texture(wad);
}

/* ------------------------------------------------------------------ */
/* Job                                                                 */
/* ------------------------------------------------------------------ */
void compile_start(CompileJob *job, const Grid *grid, const Fgd *fgd,
                   const Settings *settings, const Wad *wad)
{
    char buf[COMPILE_LOG_LEN];
    int len;

    compile_init(job);
    job->state = COMPILE_RUNNING;
    job->step = 0;

    strncpy(job->out_dir, settings->out_dir[0] != '\0' ? settings->out_dir : ".",
            PATH_LEN - 1);
    job->out_dir[PATH_LEN - 1] = '\0';

    /* build the target path */
    len = 0;
    len = cat_str(job->map_path, 0, job->out_dir);
    if (len > 0 && job->map_path[len - 1] != '/' && job->map_path[len - 1] != '\\') {
        job->map_path[len++] = '/';
        job->map_path[len] = '\0';
    }
    cat_str(job->map_path, len, MAP_NAME ".map");

    compile_log_add(job, "KABLUK MAPPER - MAP COMPILER");
    len = 0;
    len = cat_str(buf, len, "OUTPUT: ");
    cat_str(buf, len, job->map_path);
    compile_log_add(job, buf);

    (void)fgd;
    (void)grid;
    (void)wad;
}

static void compile_fail(CompileJob *job, const char *why)
{
    if (job->file != NULL) {
        fclose((FILE *)job->file);
        job->file = NULL;
    }
    compile_log_add(job, why);
    compile_log_add(job, "COMPILE ABORTED");
    job->state = COMPILE_FAILED;
    job->progress = 0;
}

void compile_step(CompileJob *job, const Grid *grid, const Fgd *fgd,
                  const Settings *settings, const Wad *wad)
{
    char buf[COMPILE_LOG_LEN];
    int len;
    int i;

    if (job->state != COMPILE_RUNNING) {
        return;
    }

    switch (job->step) {
    case 0: { /* statistics */
        int solid = count_solid(grid);
        int ents = count_entities(grid);

        job->brush_total = count_runs(grid) + 2; /* + floor and ceiling slabs */
        job->entity_total = ents;
        job->progress = 5;

        len = 0;
        len = cat_str(buf, len, "MAP: ");
        len = cat_int(buf, len, solid);
        len = cat_str(buf, len, " BLOCKS, ");
        len = cat_int(buf, len, ents);
        cat_str(buf, len, " ENTITIES");
        compile_log_add(job, buf);

        len = 0;
        len = cat_str(buf, len, "BRUSHES AFTER MERGE: ");
        len = cat_int(buf, len, job->brush_total);
        compile_log_add(job, buf);
        job->step++;
        break;
    }

    case 1: /* seal check */
        for (i = 0; i < MAP_COLS; i++) {
            if (grid->solid[0][i] == CELL_EMPTY ||
                grid->solid[MAP_ROWS - 1][i] == CELL_EMPTY) {
                compile_fail(job, "LEAK: MAP SHELL IS OPEN");
                return;
            }
        }
        for (i = 0; i < MAP_ROWS; i++) {
            if (grid->solid[i][0] == CELL_EMPTY ||
                grid->solid[i][MAP_COLS - 1] == CELL_EMPTY) {
                compile_fail(job, "LEAK: MAP SHELL IS OPEN");
                return;
            }
        }
        compile_log_add(job, "SEAL CHECK: SHELL CLOSED (NO LEAKS)");
        compile_log_add(job, "FLOOR + CEILING SLABS ADDED");
        job->progress = 10;
        job->step++;
        break;

    case 2: /* entity check */
        if (job->entity_total > 0) {
            int x;
            int y;
            int bad = 0;

            for (y = 0; y < MAP_ROWS; y++) {
                for (x = 0; x < MAP_COLS; x++) {
                    int cls;
                    const FgdClass *c;

                    if (grid->entity[y][x] == 0) {
                        continue;
                    }
                    cls = (int)grid->entity[y][x] - 1;
                    c = fgd_class(fgd, cls);
                    if (c == NULL) {
                        bad++;
                    }
                }
            }
            if (bad > 0) {
                len = 0;
                len = cat_str(buf, len, "WARNING: ");
                len = cat_int(buf, len, bad);
                cat_str(buf, len, " ENTITY CLASSES NOT IN FGD");
                compile_log_add(job, buf);
            } else {
                compile_log_add(job, "ENTITY CHECK: ALL CLASSES FOUND");
            }
        }
        if (wad != NULL && wad->count > 0) {
            len = 0;
            len = cat_str(buf, len, "WAD TEXTURES: ");
            len = cat_int(buf, len, wad->count);
            len = cat_str(buf, len, ", DEFAULT ");
            cat_str(buf, len, wad->names[0]);
            compile_log_add(job, buf);
        } else {
            compile_log_add(job, "NO WAD: USING 'brown' FOR ALL FACES");
        }
        job->progress = 15;
        job->step++;
        break;

    case 3: /* open the file */
        job->file = (void *)fopen(job->map_path, "wb");
        if (job->file == NULL) {
            compile_fail(job, "CANNOT CREATE THE .MAP FILE");
            return;
        }
        compile_log_add(job, "WRITING WORLDSPAWN");
        write_text(job, "// Game: Quake\n// Format: Standard\n// Written by Kabluk Mapper\n");
        write_text(job, "// entity 0\n{\n\"classname\" \"worldspawn\"\n");
        if (settings->wad_path[0] != '\0') {
            write_text(job, "\"wad\" \"");
            write_text(job, settings->wad_path);
            write_text(job, "\"\n");
        }
        write_text(job, "\"message\" \"Kabluk map\"\n");

        /* floor and ceiling slabs covering the whole map */
        write_box(job, 0, 0, -SLAB_THICKNESS, MAP_COLS * BLOCK_UNITS,
                  MAP_ROWS * BLOCK_UNITS, 0, wad_fallback_texture(wad));
        write_box(job, 0, 0, BLOCK_HEIGHT_UNITS, MAP_COLS * BLOCK_UNITS,
                  MAP_ROWS * BLOCK_UNITS, BLOCK_HEIGHT_UNITS + SLAB_THICKNESS,
                  wad_fallback_texture(wad));
        job->brush_done = 2;
        job->cur_row = 0;
        job->cur_col = 0;
        job->progress = 20;
        job->step++;
        break;

    case 4: { /* brush chunks */
        int done = 0;
        int exhausted = 0;

        while (done < BRUSHES_PER_STEP) {
            int x0;
            int x1;
            int tex;

            if (!next_run(grid, job, &x0, &x1, &tex)) {
                exhausted = 1;
                break;
            }
            (void)tex;
            write_box(job, x0 * BLOCK_UNITS, job->cur_row * BLOCK_UNITS, 0,
                      (x1 + 1) * BLOCK_UNITS, (job->cur_row + 1) * BLOCK_UNITS,
                      BLOCK_HEIGHT_UNITS, brush_texture(grid, wad, x0, job->cur_row));
            job->brush_done++;
            done++;
        }

        if (job->brush_total > 0) {
            job->progress = 20 + (job->brush_done * 60) / job->brush_total;
        }
        if (exhausted || job->brush_done >= job->brush_total) {
            job->progress = 80;
            write_text(job, "}\n");
            compile_log_add(job, "BRUSHES WRITTEN");
            job->step++;
        }
        break;
    }

    case 5: { /* entities */
        int x;
        int y;
        int written = 0;

        for (y = 0; y < MAP_ROWS; y++) {
            for (x = 0; x < MAP_COLS; x++) {
                int cls;
                const FgdClass *c;

                if (grid->entity[y][x] == 0) {
                    continue;
                }
                cls = (int)grid->entity[y][x] - 1;
                c = fgd_class(fgd, cls);
                if (c == NULL) {
                    continue;
                }
                len = 0;
                len = cat_str(buf, len, "// entity ");
                len = cat_int(buf, len, written + 1);
                cat_str(buf, len, "\n{\n\"classname\" \"");
                write_text(job, buf);
                write_text(job, c->name);
                write_text(job, "\"\n\"origin\" \"");
                sprintf(buf, "%d %d 24", x * BLOCK_UNITS + BLOCK_UNITS / 2,
                        y * BLOCK_UNITS + BLOCK_UNITS / 2);
                write_text(job, buf);
                write_text(job, "\"\n}\n");
                written++;
            }
        }
        job->entity_done = written;
        if (written > 0) {
            len = 0;
            len = cat_str(buf, len, "WROTE ");
            len = cat_int(buf, len, written);
            cat_str(buf, len, " POINT ENTITIES");
            compile_log_add(job, buf);
        }
        job->progress = 95;
        job->step++;
        break;
    }

    default: { /* close and report */
        FILE *file = (FILE *)job->file;

        if (file != NULL) {
            fclose(file);
            job->file = NULL;
        }
        len = 0;
        len = cat_str(buf, len, "WROTE ");
        len = cat_int(buf, len, (int)job->bytes);
        len = cat_str(buf, len, " BYTES, ");
        len = cat_int(buf, len, job->brush_total);
        cat_str(buf, len, " BRUSHES");
        compile_log_add(job, buf);
        compile_log_add(job, "MAP EXPORTED - BSP STAGE NOT BUILT IN YET");
        job->progress = 100;
        job->state = COMPILE_DONE;
        job->step++;
        break;
    }
    }
}
