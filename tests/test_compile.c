/*
 * Host test: what the exporter puts in the .map (compile.c).
 *
 * The editor writes two brushes nobody asked for - the floor slab below z = 0
 * and the ceiling slab above z = 128 - and nothing else. It used to wall off
 * every *empty* border cell as well, so a map nobody had closed came back with
 * geometry the user never painted, four runs of it around the edge.
 *
 * That is gone. Two maps are enough to prove it: an empty grid must produce
 * exactly the two slabs, and a grid with a single painted cell exactly three
 * brushes. With the old border walls the first would have had six and the
 * second seven.
 *
 * The exporter is run with save_only, so the tool chain never starts and the
 * test only has to read back the .map text.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "compile.h"
#include "fgd.h"
#include "grid.h"
#include "settings.h"
#include "values.h"
#include "wad.h"

#define TEST_CWD_ENV "KMP_TEST_CWD"

/* compile.c only calls the chain in step 7, and this test stops at save_only,
 * so the chain is stubbed out here - the point of the test is the .map text. */
void bspchain_init(BspChain *chain)
{
    memset(chain, 0, sizeof(*chain));
    chain->stage = BSPCHAIN_IDLE;
}

void bspchain_set_log(void (*callback)(const char *line))
{
    (void)callback;
}

void bspchain_setup(BspChain *chain, const char *map_path, const char *wad_path)
{
    (void)chain;
    (void)map_path;
    (void)wad_path;
}

int bspchain_run_stage(BspChain *chain)
{
    (void)chain;
    return 0;
}

const char *bspchain_stage_name(int stage)
{
    (void)stage;
    return "STUB";
}

/* grid.c carries its drawing code along, and compile.c asks the kernel how
 * much memory is left; neither is used here, so both are stubbed rather than
 * pulling in gfx.c, textures.c and a PSP. */
unsigned int sceKernelMaxFreeMemSize(void)
{
    return 0;
}

void gfx_set_color(Gfx *g, unsigned int rgb)
{
    (void)g;
    (void)rgb;
}

void gfx_draw_hline(Gfx *g, int x, int y, int w)
{
    (void)g;
    (void)x;
    (void)y;
    (void)w;
}

void gfx_draw_vline(Gfx *g, int x, int y, int h)
{
    (void)g;
    (void)x;
    (void)y;
    (void)h;
}

void gfx_rect(Gfx *g, int x, int y, int w, int h, unsigned int rgb)
{
    (void)g;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)rgb;
}

void gfx_outline(Gfx *g, int x, int y, int w, int h, unsigned int rgb)
{
    (void)g;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)rgb;
}

void gfx_rect_alpha(Gfx *g, int x, int y, int w, int h, unsigned int rgb,
                    unsigned char alpha)
{
    (void)g;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
    (void)rgb;
    (void)alpha;
}

void gfx_blit_fit(Gfx *g, SDL_Texture *texture, int x, int y, int w, int h)
{
    (void)g;
    (void)texture;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}

int gfx_text_width(const char *text, int scale)
{
    (void)scale;
    return (int)strlen(text) * 6;
}

int gfx_text(Gfx *g, int x, int y, const char *text, unsigned int rgb, int scale)
{
    (void)g;
    (void)x;
    (void)y;
    (void)text;
    (void)rgb;
    (void)scale;
    return x;
}

int gfx_text_int(Gfx *g, int x, int y, int value, unsigned int rgb, int scale)
{
    (void)g;
    (void)y;
    (void)value;
    (void)rgb;
    (void)scale;
    return x;
}

SDL_Texture *textures_get(Gfx *g, Textures *textures, const Wad *wad, int index)
{
    (void)g;
    (void)textures;
    (void)wad;
    (void)index;
    return NULL;
}

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

/* Reads a whole file into a malloc'd, NUL terminated buffer (NULL if there is
 * no such file). */
static char *read_all(const char *path)
{
    FILE *file = fopen(path, "rb");
    long size;
    char *text;

    if (file == NULL) {
        return NULL;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    size = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    text = (char *)malloc((size_t)size + 1);
    if (text == NULL) {
        fclose(file);
        return NULL;
    }
    if (fread(text, 1, (size_t)size, file) != (size_t)size) {
        free(text);
        fclose(file);
        return NULL;
    }
    text[size] = '\0';
    fclose(file);
    return text;
}

/* Every brush is six face lines and every face line is "( x y z ) ...", so
 * the number of brushes is the number of those lines divided by six. Counting
 * "{" would also count the worldspawn and entity blocks. */
static int brush_count(const char *text)
{
    int faces = 0;
    const char *p = text;

    while (*p != '\0') {
        if ((p == text || p[-1] == '\n') && p[0] == '(') {
            faces++;
        }
        p++;
    }
    return faces / 6;
}

/* A face line is "( x y z ) ( x y z ) ( x y z ) texture ...", so the z values
 * of a brush say which brush it is:
 *   floor slab     -16 .. 0
 *   ceiling slab   128 .. 144
 *   painted block    0 .. 128
 *   sealing wall   -16 .. 144   <- what this test is about: there are none
 * Returns 1 when any brush covers the whole height. */
static int brush_spans_full_height(const char *text)
{
    const char *p = text;

    while ((p = strchr(p, '{')) != NULL) {
        const char *end = strchr(p, '}');
        const char *q = p;
        int minz = 100000;
        int maxz = -100000;

        if (end == NULL) {
            break;
        }
        while (q < end) {
            int v[3];
            int used = 0;

            if (*q == '(' &&
                sscanf(q, "( %d %d %d )%n", &v[0], &v[1], &v[2], &used) == 3 &&
                used > 0) {
                if (v[2] < minz) {
                    minz = v[2];
                }
                if (v[2] > maxz) {
                    maxz = v[2];
                }
                q += used;
                continue;
            }
            q++;
        }
        if (minz == -16 && maxz == 144) {
            return 1;
        }
        p = end + 1;
    }
    return 0;
}

static int line_present(const CompileJob *job, const char *text)
{
    int i;

    for (i = 0; i < job->line_count; i++) {
        if (strcmp(job->log[i], text) == 0) {
            return 1;
        }
    }
    return 0;
}

/* Runs the exporter to the end for the map that is in 'grid' right now and
 * hands back the .map text (NULL when it was not written). */
static char *export_map(CompileJob *job, const Grid *grid, const Fgd *fgd,
                        Settings *settings, const ValueStore *values,
                        const char *name)
{
    int guard;

    strcpy(settings->map_name, name);
    remove(job->map_path); /* from the previous run, if any */
    compile_start(job, grid, fgd, settings, NULL, 1);
    for (guard = 0; guard < 500 && job->state == COMPILE_RUNNING; guard++) {
        compile_step(job, grid, fgd, settings, NULL, values);
    }
    if (job->state != COMPILE_DONE) {
        return NULL;
    }
    return read_all(job->map_path);
}

int main(void)
{
    const char *cwd = getenv(TEST_CWD_ENV);
    static CompileJob job;
    Grid grid;
    Fgd fgd;
    Settings settings;
    ValueStore values;
    char *text;

    if (cwd != NULL && chdir(cwd) != 0) {
        printf("cannot enter %s\n", cwd);
        return 2;
    }

    memset(&fgd, 0, sizeof(fgd));
    settings_defaults(&settings);
    strcpy(settings.out_dir, ".");
    strcpy(settings.floor_tex, "floor_a");
    strcpy(settings.ceiling_tex, "ceil_b");
    values_init(&values);

    /* ---- an untouched map: the two slabs and nothing else ---------------- */
    grid_init(&grid);
    grid_clear_map(&grid);
    text = export_map(&job, &grid, &fgd, &settings, &values, "t_empty");
    check(text != NULL, "an empty map still exports");
    if (text != NULL) {
        check(brush_count(text) == 2, "empty map = floor slab + ceiling slab");
        check(!brush_spans_full_height(text), "and no wall brush around the border");
        check(strstr(text, "floor_a") != NULL,
              "the floor slab uses the floor texture");
        check(strstr(text, "ceil_b") != NULL,
              "the ceiling slab uses the ceiling texture");
        check(strstr(text, "\"classname\" \"worldspawn\"") != NULL,
              "worldspawn is written");
        free(text);
    }
    check(line_present(&job, "FLOOR + CEILING SLABS ADDED"),
          "the log names the two slabs");
    check(line_present(&job, "NO BORDER WALLS - THE MAP SEALS ITSELF"),
          "the log says the border is left to the user");
    check(!line_present(&job, "SEALING WALLS: 0 RUNS AROUND THE BORDER"),
          "the old sealing wall line is gone");

    /* ---- one painted cell: the two slabs plus that one brush ------------- */
    grid_clear_map(&grid);
    grid_paint(&grid, 5, 5, 7);
    text = export_map(&job, &grid, &fgd, &settings, &values, "t_one");
    check(text != NULL, "a one block map exports");
    if (text != NULL) {
        check(brush_count(text) == 3, "one block = 3 brushes, not 3 + walls");
        check(!brush_spans_full_height(text), "still no wall brush");
        free(text);
    }

    /* ---- a closed map: the ring is the user's own work now --------------- */
    grid_clear_map(&grid);
    {
        int i;

        for (i = 0; i < MAP_COLS; i++) {
            grid_paint(&grid, i, 0, 7);
            grid_paint(&grid, i, MAP_ROWS - 1, 7);
        }
        for (i = 0; i < MAP_ROWS; i++) {
            grid_paint(&grid, 0, i, 7);
            grid_paint(&grid, MAP_COLS - 1, i, 7);
        }
    }
    text = export_map(&job, &grid, &fgd, &settings, &values, "t_ring");
    check(text != NULL, "a closed map exports");
    if (text != NULL) {
        /* 32 columns on the top row, 32 on the bottom, then 30 rows with a
         * cell on each side: 2 + 60 runs, plus the two slabs */
        check(brush_count(text) == 2 + 60 + 2, "the ring becomes 62 runs + 2 slabs");
        check(!brush_spans_full_height(text), "no extra walls behind the ring");
        free(text);
    }

    remove("t_empty.map");
    remove("t_one.map");
    remove("t_ring.map");
    remove("compile.log");
    remove("kabluk-phase.txt");

    printf("\n%s (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures != 0;
}
