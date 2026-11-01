/*
 * Kabluk Mapper Portable - compile.c
 *
 * Grid -> Quake .map exporter with a step-by-step log.
 *
 * Geometry: every solid cell is a 128x128x128 block standing on z = 0.
 * Neighbouring cells that share a texture are merged into one brush per row,
 * which keeps the .map (and later the BSP) a lot smaller. Besides the blocks
 * the exporter always writes a floor slab below z = 0, a ceiling slab above
 * z = 128 and a wall around every border cell the user left empty, so nothing
 * can leak into the void no matter how little was painted.
 */
#include "compile.h"

#include "fs.h"

/* The chain reports its lines through a single callback (see bspchain.h). */
static CompileJob *log_target = NULL;

static void chain_line_to_log(const char *line)
{
    if (log_target != NULL) {
        compile_log_add(log_target, line);
    }
}

#include <stdio.h>
#include <string.h>

#define MAP_NAME "kabluk"
#define SLAB_THICKNESS 16
#define BRUSHES_PER_STEP 64

/* ------------------------------------------------------------------ */
/* Text helpers (no snprintf: keeps this C89 and PSP friendly)         */
/* ------------------------------------------------------------------ */
/* Appends 'text' to a buffer of 'cap' bytes (including the terminator).
 * The log lines and the .map keys use different buffers, so the cap is a
 * parameter instead of a global constant. */
static int cat_str_cap(char *buf, int cap, int len, const char *text)
{
    int i = 0;

    while (text[i] != '\0' && len < cap - 1) {
        buf[len++] = text[i++];
    }
    buf[len] = '\0';
    return len;
}

static int cat_str(char *buf, int len, const char *text)
{
    return cat_str_cap(buf, COMPILE_LOG_LEN, len, text);
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
    job->wall_total = 0;
    job->save_only = 0;
    job->entity_total = 0;
    job->entity_done = 0;
    job->cur_row = 0;
    job->cur_col = 0;
    job->in_entity_pass = 0;
    job->chain_started = 0;
    job->light_entities = 0;
    job->chain.bsp_bytes = 0;
    bspchain_init(&job->chain);
    bspchain_set_log(chain_line_to_log);
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
        /* name the tool while the chain is running */
        if (job->chain_started && job->chain.stage >= BSPCHAIN_QBSP &&
            job->chain.stage <= BSPCHAIN_VIS) {
            return bspchain_stage_name(job->chain.stage);
        }
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

/* Cell index -> world units. The map is centred on the origin, so cell 0 of
 * a 32 cell map starts at -2048 and the last one ends at +2048. */
static int cell_world_x(int cx)
{
    return (cx - MAP_COLS / 2) * BLOCK_UNITS;
}

static int cell_world_y(int cy)
{
    return (cy - MAP_ROWS / 2) * BLOCK_UNITS;
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
        const unsigned char *row;
        int start;
        int t;

        if (y < 0 || y >= MAP_ROWS) {
            return 0;
        }
        row = grid->solid[y];
        if (x < 0 || x > MAP_COLS) {
            x = MAP_COLS;
        }
        while (x < MAP_COLS && row[x] == CELL_EMPTY) {
            x++;
        }
        if (x >= MAP_COLS) {
            job->cur_row++;
            job->cur_col = 0;
            continue;
        }
        start = x;
        t = cell_texture(grid, x, y);
        while (x + 1 < MAP_COLS && row[x + 1] != CELL_EMPTY &&
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

/* Floor / ceiling / sealing wall texture: the one picked in the editor
 * settings, or the first texture of the wad when nothing was picked. */
static const char *slab_texture(const Wad *wad, const char *picked)
{
    if (picked != NULL && picked[0] != '\0') {
        return picked;
    }
    return wad_fallback_texture(wad);
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
/* Sealing walls                                                       */
/* ------------------------------------------------------------------ */
/* The editor no longer pre-fills the border, so the exporter walls it off:
 * every *empty* border cell becomes part of a wall run, cells the user
 * painted are left exactly as they are. Four runs per edge at most, so the
 * brush count barely moves. */
#define BORDER_RUNS_MAX (MAP_ROWS * 2 + 2)

typedef struct BorderRun {
    int horizontal; /* 1 = runs along x on row y, 0 = along y on column x */
    int line;       /* the row / column the run sits on */
    int from;
    int to;         /* inclusive cell range */
} BorderRun;

static int border_runs(const Grid *grid, BorderRun *runs, int max)
{
    int n = 0;
    int i;

    /* top and bottom rows, along x */
    for (i = 0; i < 2; i++) {
        int y = (i == 0) ? 0 : MAP_ROWS - 1;
        int x = 0;

        while (x < MAP_COLS) {
            int start;

            while (x < MAP_COLS && grid->solid[y][x] != CELL_EMPTY) {
                x++;
            }
            if (x >= MAP_COLS) {
                break;
            }
            start = x;
            while (x < MAP_COLS && grid->solid[y][x] == CELL_EMPTY) {
                x++;
            }
            if (n < max) {
                runs[n].horizontal = 1;
                runs[n].line = y;
                runs[n].from = start;
                runs[n].to = x - 1;
                n++;
            }
        }
    }

    /* left and right columns, along y, corners already covered above */
    for (i = 0; i < 2; i++) {
        int x = (i == 0) ? 0 : MAP_COLS - 1;
        int y = 1;

        while (y < MAP_ROWS - 1) {
            int start;

            while (y < MAP_ROWS - 1 && grid->solid[y][x] != CELL_EMPTY) {
                y++;
            }
            if (y >= MAP_ROWS - 1) {
                break;
            }
            start = y;
            while (y < MAP_ROWS - 1 && grid->solid[y][x] == CELL_EMPTY) {
                y++;
            }
            if (n < max) {
                runs[n].horizontal = 0;
                runs[n].line = x;
                runs[n].from = start;
                runs[n].to = y - 1;
                n++;
            }
        }
    }
    return n;
}

static void write_border_walls(CompileJob *job, const Grid *grid, const Wad *wad,
                               const Settings *settings)
{
    BorderRun runs[BORDER_RUNS_MAX];
    const char *texture;
    int count;
    int i;

    texture = slab_texture(wad, settings != NULL ? settings->floor_tex : NULL);
    count = border_runs(grid, runs, BORDER_RUNS_MAX);
    for (i = 0; i < count; i++) {
        const BorderRun *r = &runs[i];
        int minz = -SLAB_THICKNESS;
        int maxz = BLOCK_HEIGHT_UNITS + SLAB_THICKNESS;

        if (r->horizontal) {
            write_box(job, cell_world_x(r->from), cell_world_y(r->line),
                      minz, cell_world_x(r->to + 1),
                      cell_world_y(r->line + 1), maxz, texture);
        } else {
            write_box(job, cell_world_x(r->line), cell_world_y(r->from), minz,
                      cell_world_x(r->line + 1), cell_world_y(r->to + 1), maxz,
                      texture);
        }
    }
}

/* ------------------------------------------------------------------ */
/* Job                                                                 */
/* ------------------------------------------------------------------ */
void compile_start(CompileJob *job, const Grid *grid, const Fgd *fgd,
                   const Settings *settings, const Wad *wad, int save_only)
{
    char buf[COMPILE_LOG_LEN];
    int len;

    compile_init(job);
    job->state = COMPILE_RUNNING;
    job->step = 0;
    job->save_only = save_only;

    strncpy(job->out_dir, settings->out_dir[0] != '\0' ? settings->out_dir : ".",
            PATH_LEN - 1);
    job->out_dir[PATH_LEN - 1] = '\0';

    /* "<out dir>/<name>.map", the name comes from the editor settings; the
     * folder is created when it is missing, so a new map name never fails on
     * a missing directory */
    fs_mkdir(job->out_dir);
    settings_map_path(settings, job->map_path, PATH_LEN);

    log_target = job;
    job->chain_started = 0;
    job->light_entities = 0;
    compile_log_add(job, save_only ? "KABLUK MAPPER - SAVE MAP SOURCE"
                                   : "KABLUK MAPPER - MAP COMPILER");
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
    job->state = COMPILE_FAILED; /* the bar keeps its colour-coded position */
}

void compile_step(CompileJob *job, const Grid *grid, const Fgd *fgd,
                  const Settings *settings, const Wad *wad,
                  const ValueStore *values)
{
    char buf[COMPILE_LOG_LEN];
    int len;

    if (job->state != COMPILE_RUNNING) {
        return;
    }

    switch (job->step) {
    case 0: { /* statistics */
        int solid = count_solid(grid);
        int ents = count_entities(grid);

        {   /* + floor and ceiling slabs + the walls that seal the border */
            BorderRun runs[BORDER_RUNS_MAX];
            int walls = border_runs(grid, runs, BORDER_RUNS_MAX);

            job->wall_total = walls;
            job->brush_total = count_runs(grid) + 2 + walls;
        }
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

    case 1: /* the border is walled off by the exporter, never by the editor */
        len = 0;
        len = cat_str(buf, len, "SEALING WALLS: ");
        len = cat_int(buf, len, job->wall_total);
        cat_str(buf, len, " RUNS AROUND THE BORDER");
        compile_log_add(job, buf);
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
        {
            const char *msg = values_get(values, VALUES_WORLD_CX,
                                         VALUES_WORLD_CY, "message");

            write_text(job, "\"message\" \"");
            write_text(job, msg != NULL ? msg : DEFAULT_MAP_MESSAGE);
            write_text(job, "\"\n");
        }
        /* worldspawn "light" is the map-wide minimum light (minlight in the
         * light tool), so a map is never black even without light entities.
         * The right panel can change it; 200 is the default. */
        {
            const char *light = values_get(values, VALUES_WORLD_CX,
                                           VALUES_WORLD_CY, "light");

            len = 0;
            len = cat_str(buf, len, "\"light\" \"");
            if (light != NULL) {
                len = cat_str(buf, len, light);
            } else {
                len = cat_int(buf, len, DEFAULT_MINLIGHT);
            }
            cat_str(buf, len, "\"\n");
            write_text(job, buf);

            len = 0;
            len = cat_str(buf, len, "WORLDSPAWN LIGHT: ");
            if (light != NULL) {
                len = cat_str(buf, len, light);
            } else {
                len = cat_int(buf, len, DEFAULT_MINLIGHT);
            }
            cat_str(buf, len, " (MINLIGHT)");
            compile_log_add(job, buf);
        }

        /* floor and ceiling slabs covering the whole map (-2048..2048), each
         * with the texture picked for it in the editor settings */
        write_box(job, -MAP_HALF_UNITS, -MAP_HALF_UNITS, -SLAB_THICKNESS,
                  MAP_HALF_UNITS, MAP_HALF_UNITS, 0,
                  slab_texture(wad, settings != NULL ? settings->floor_tex : NULL));
        write_box(job, -MAP_HALF_UNITS, -MAP_HALF_UNITS, BLOCK_HEIGHT_UNITS,
                  MAP_HALF_UNITS, MAP_HALF_UNITS,
                  BLOCK_HEIGHT_UNITS + SLAB_THICKNESS,
                  slab_texture(wad, settings != NULL ? settings->ceiling_tex
                                                      : NULL));
        /* and the walls that keep the map sealed now that nothing is
         * pre-placed in the border */
        write_border_walls(job, grid, wad, settings);
        job->brush_done = 2 + job->wall_total;
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
            write_box(job, cell_world_x(x0), cell_world_y(job->cur_row), 0,
                      cell_world_x(x1 + 1), cell_world_y(job->cur_row + 1),
                      BLOCK_HEIGHT_UNITS,
                      brush_texture(grid, wad, x0, job->cur_row));
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
                if (strcmp(c->name, "light") == 0) {
                    job->light_entities++;
                }
                len = 0;
                len = cat_str(buf, len, "// entity ");
                len = cat_int(buf, len, written + 1);
                cat_str(buf, len, "\n{\n\"classname\" \"");
                write_text(job, buf);
                write_text(job, c->name);
                write_text(job, "\"\n\"origin\" \"");
                sprintf(buf, "%d %d 24", cell_world_x(x) + BLOCK_UNITS / 2,
                        cell_world_y(y) + BLOCK_UNITS / 2);
                write_text(job, buf);
                write_text(job, "\"\n");

                /* every key the user typed in the right panel */
                {
                    int k;
                    int keys = values_count_cell(values, x, y);

                    for (k = 0; k < keys; k++) {
                        const EntityValue *item = values_cell_at(values, x, y, k);
                        char line[FGD_NAME_LEN + FGD_VALUE_LEN + 8];
                        int n;

                        if (item == NULL || item->value[0] == '\0') {
                            continue;
                        }
                        if (strcmp(item->key, "classname") == 0 ||
                            strcmp(item->key, "origin") == 0) {
                            continue;
                        }
                        n = 0;
                        n = cat_str_cap(line, (int)sizeof(line), n, "\"");
                        n = cat_str_cap(line, (int)sizeof(line), n, item->key);
                        n = cat_str_cap(line, (int)sizeof(line), n, "\" \"");
                        n = cat_str_cap(line, (int)sizeof(line), n, item->value);
                        cat_str_cap(line, (int)sizeof(line), n, "\"\n");
                        write_text(job, line);
                    }
                }
                write_text(job, "}\n");
                written++;
            }
        }
        if (job->light_entities > 0) {
            len = 0;
            len = cat_str(buf, len, "LIGHT ENTITIES: ");
            len = cat_int(buf, len, job->light_entities);
            cat_str(buf, len, " (ON TOP OF MINLIGHT)");
            compile_log_add(job, buf);
        }

        job->entity_done = written;
        if (written > 0) {
            len = 0;
            len = cat_str(buf, len, "WROTE ");
            len = cat_int(buf, len, written);
            cat_str(buf, len, " ENTITIES");
            compile_log_add(job, buf);
        }
        job->progress = 80;
        job->step++;
        break;
    }

    case 6: { /* hand the finished .map to the tool chain */
        FILE *file = (FILE *)job->file;

        if (file != NULL) {
            fclose(file);
            job->file = NULL;
        }
        len = 0;
        len = cat_str(buf, len, "MAP WRITTEN: ");
        len = cat_int(buf, len, (int)job->bytes);
        cat_str(buf, len, " BYTES");
        compile_log_add(job, buf);

        if (job->save_only) {
            compile_log_add(job, "MAP SOURCE SAVED - NOT COMPILED");
            job->progress = 100;
            job->state = COMPILE_DONE;
            job->step++;
            break;
        }

        bspchain_setup(&job->chain, job->map_path,
                       settings != NULL ? settings->wad_path : "");
        job->chain_started = 1;
        job->progress = 82;
        compile_log_add(job, "CHAIN: QBSP LIGHT VIS");
        job->step++;
        break;
    }

    case 7: { /* one tool per frame: qbsp, then light, then vis */
        int before = job->chain.stage;

        if (before != BSPCHAIN_DONE && before != BSPCHAIN_FAILED) {
            len = 0;
            len = cat_str(buf, len, "STAGE ");
            cat_str(buf, len, bspchain_stage_name(before));
            compile_log_add(job, buf);
        }

        if (bspchain_run_stage(&job->chain)) {
            job->progress = 82 + (job->chain.stage - 1) * 5;
            break;
        }

        if (job->chain.stage == BSPCHAIN_FAILED) {
            compile_fail(job, "TOOL CHAIN STOPPED - SEE LOG ABOVE");
            break;
        }

        len = 0;
        len = cat_str(buf, len, "BSP READY: ");
        len = cat_int(buf, len, (int)job->chain.bsp_bytes);
        cat_str(buf, len, " BYTES");
        compile_log_add(job, buf);
        compile_log_add(job, "MAP COMPILED - READY TO PLAY");
        job->progress = 100;
        job->state = COMPILE_DONE;
        job->step++;
        break;
    }

    default: {
        /* nothing left to do - the job stays in its finished state */
        break;
    }
    }
}
