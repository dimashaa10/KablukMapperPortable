/*
 * Kabluk Mapper Portable - mapfile.c
 *
 * A small token reader on top of stdio: no allocation, no line buffers, so
 * it behaves the same on a PSP. Everything is read one character at a time
 * through a one byte pushback.
 */
#include "mapfile.h"

#include <stdio.h>
#include <string.h>

#include "fs.h"

/* one entity never carries more keys than this in the editor */
#define MAPFILE_MAX_PAIRS 24
#define MAPFILE_PEEK_LEN 4096

#define TOK_END 0
#define TOK_PUNCT 1
#define TOK_WORD 2
#define TOK_STRING 3

#define RD_PUSHBACK 4

typedef struct Reader {
    FILE *file;
    int pushed[RD_PUSHBACK]; /* characters put back, newest last */
    int count;
} Reader;

typedef struct Pair {
    char key[FGD_NAME_LEN];
    char value[FGD_VALUE_LEN];
} Pair;

static int rd_get(Reader *r)
{
    if (r->count > 0) {
        r->count--;
        return r->pushed[r->count];
    }
    return fgetc(r->file);
}

static void rd_unget(Reader *r, int c)
{
    if (c >= 0 && r->count < RD_PUSHBACK) {
        r->pushed[r->count++] = c;
    }
}

/* Skips blanks and // comments; returns 1 when there is something to read. */
static int rd_skip(Reader *r)
{
    int c;

    for (;;) {
        c = rd_get(r);
        if (c == EOF) {
            return 0;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            continue;
        }
        if (c == '/') {
            int next = rd_get(r);

            if (next == '/') {
                do {
                    c = rd_get(r);
                } while (c != EOF && c != '\n');
                continue;
            }
            rd_unget(r, next);
        }
        rd_unget(r, c);
        return 1;
    }
}

static int rd_token(Reader *r, char *out, int cap)
{
    int c;
    int n = 0;

    out[0] = '\0';
    if (!rd_skip(r)) {
        return TOK_END;
    }
    c = rd_get(r);
    if (c == EOF) {
        return TOK_END;
    }

    if (c == '{' || c == '}' || c == '(' || c == ')' || c == ',') {
        out[0] = (char)c;
        out[1] = '\0';
        return TOK_PUNCT;
    }

    if (c == '"') {
        for (;;) {
            c = rd_get(r);
            if (c == EOF) {
                break;
            }
            if (c == '"') {
                break;
            }
            if (n < cap - 1) {
                out[n++] = (char)c;
            }
        }
        out[n] = '\0';
        return TOK_STRING;
    }

    while (c != EOF) {
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '{' ||
            c == '}' || c == '(' || c == ')' || c == '"' || c == ',') {
            rd_unget(r, c);
            break;
        }
        if (n < cap - 1) {
            out[n++] = (char)c;
        }
        c = rd_get(r);
    }
    out[n] = '\0';
    return n > 0 ? TOK_WORD : TOK_END;
}

int mapfile_tex_index(const Wad *wad, const char *name)
{
    int i;

    if (wad == NULL || name == NULL || name[0] == '\0') {
        return -1;
    }
    for (i = 0; i < wad->count && i < WAD_MAX_TEX; i++) {
        const char *tex = wad->names[i];
        int c = 0;

        while (tex[c] != '\0' && name[c] != '\0' && tex[c] == name[c]) {
            c++;
        }
        if (tex[c] == '\0' && name[c] == '\0') {
            return i;
        }
        /* wad names are upper case, .map names are not always */
        c = 0;
        while (tex[c] != '\0' && name[c] != '\0' &&
               (tex[c] | 0x20) == (name[c] | 0x20)) {
            c++;
        }
        if (tex[c] == '\0' && name[c] == '\0') {
            return i;
        }
    }
    return -1;
}

static int read_number(Reader *r, int *value)
{
    char token[64];
    int kind = rd_token(r, token, (int)sizeof(token));
    int sign = 1;
    int i = 0;
    int n = 0;

    if (kind != TOK_WORD && kind != TOK_STRING) {
        return 0;
    }
    if (token[0] == '-') {
        sign = -1;
        i = 1;
    } else if (token[0] == '+') {
        i = 1;
    }
    for (; token[i] >= '0' && token[i] <= '9'; i++) {
        n = n * 10 + (token[i] - '0');
    }
    *value = sign * n;
    return 1;
}

#define MAPFILE_MAX_PLANES 64

/* One brush face: outward normal 'n' and plane offset 'd', a point is inside
 * the brush when n.v - d <= 0 for every face. The exporter writes the normals
 * that way, so a plain box test gives the solid cells back. */
typedef struct Plane {
    float nx;
    float ny;
    float nz;
    float d;
} Plane;

typedef struct Brush {
    int count;      /* planes read so far        */
    int have;       /* bounds are valid          */
    int minx, miny, minz;
    int maxx, maxy, maxz;
    Plane planes[MAPFILE_MAX_PLANES];
    char texture[FGD_VALUE_LEN];     /* first face texture  */
    char top_texture[FGD_VALUE_LEN]; /* face looking up     */
} Brush;

static void brush_reset(Brush *brush)
{
    brush->count = 0;
    brush->have = 0;
    brush->minx = brush->miny = brush->minz = 0;
    brush->maxx = brush->maxy = brush->maxz = 0;
    brush->texture[0] = '\0';
    brush->top_texture[0] = '\0';
}

static void brush_grow(Brush *brush, const int xyz[3])
{
    if (!brush->have) {
        brush->minx = brush->maxx = xyz[0];
        brush->miny = brush->maxy = xyz[1];
        brush->minz = brush->maxz = xyz[2];
        brush->have = 1;
        return;
    }
    if (xyz[0] < brush->minx) {
        brush->minx = xyz[0];
    }
    if (xyz[0] > brush->maxx) {
        brush->maxx = xyz[0];
    }
    if (xyz[1] < brush->miny) {
        brush->miny = xyz[1];
    }
    if (xyz[1] > brush->maxy) {
        brush->maxy = xyz[1];
    }
    if (xyz[2] < brush->minz) {
        brush->minz = xyz[2];
    }
    if (xyz[2] > brush->maxz) {
        brush->maxz = xyz[2];
    }
}

/* "( x y z )" */
static int read_point(Reader *r, int xyz[3])
{
    char token[64];
    int axis;

    if (rd_token(r, token, (int)sizeof(token)) == TOK_END) {
        return 0;
    }
    if (strcmp(token, "(") != 0) {
        return 0;
    }
    for (axis = 0; axis < 3; axis++) {
        if (!read_number(r, &xyz[axis])) {
            return 0;
        }
    }
    if (rd_token(r, token, (int)sizeof(token)) == TOK_END) {
        return 0;
    }
    return strcmp(token, ")") == 0;
}

/* Reads "( x y z ) ( x y z ) ( x y z ) TEXTURE shift rot sx sy" into 'brush'. */
static int read_face(Reader *r, Brush *brush)
{
    int points[3][3];
    char texture[FGD_VALUE_LEN];
    float ax, ay, az, bx, by, bz, nx, ny, nz;
    int i;
    int axis;

    for (i = 0; i < 3; i++) {
        if (!read_point(r, points[i])) {
            return 0;
        }
        brush_grow(brush, points[i]);
    }

    if (rd_token(r, texture, (int)sizeof(texture)) == TOK_END) {
        return 0;
    }
    for (axis = 0; axis < 5; axis++) {
        char token[64];

        if (rd_token(r, token, (int)sizeof(token)) == TOK_END) {
            return 0;
        }
    }

    ax = (float)(points[0][0] - points[1][0]);
    ay = (float)(points[0][1] - points[1][1]);
    az = (float)(points[0][2] - points[1][2]);
    bx = (float)(points[2][0] - points[1][0]);
    by = (float)(points[2][1] - points[1][1]);
    bz = (float)(points[2][2] - points[1][2]);
    nx = ay * bz - az * by;
    ny = az * bx - ax * bz;
    nz = ax * by - ay * bx;

    if (nx == 0.0f && ny == 0.0f && nz == 0.0f) {
        return 1; /* degenerate face: skip it but keep the brush */
    }
    if (brush->count < MAPFILE_MAX_PLANES) {
        Plane *plane = &brush->planes[brush->count++];

        plane->nx = nx;
        plane->ny = ny;
        plane->nz = nz;
        plane->d = nx * (float)points[0][0] + ny * (float)points[0][1] +
                   nz * (float)points[0][2];
    }

    if (brush->texture[0] == '\0') {
        fs_copy(brush->texture, FGD_VALUE_LEN, texture);
    }
    if (brush->top_texture[0] == '\0' && nz > 0.0f) {
        fs_copy(brush->top_texture, FGD_VALUE_LEN, texture);
    }
    return 1;
}

/* "x y z" -> three ints; needs at least two of them */
static int parse_xyz(const char *text, int *x, int *y, int *z)
{
    int values[3];
    int found = 0;
    int i = 0;

    values[0] = values[1] = values[2] = 0;
    while (found < 3) {
        int sign = 1;
        int n = 0;
        int digits = 0;

        while (text[i] == ' ' || text[i] == '\t') {
            i++;
        }
        if (text[i] == '\0') {
            break;
        }
        if (text[i] == '-') {
            sign = -1;
            i++;
        } else if (text[i] == '+') {
            i++;
        }
        while (text[i] >= '0' && text[i] <= '9') {
            n = n * 10 + (text[i] - '0');
            i++;
            digits++;
        }
        if (!digits) {
            break;
        }
        if (text[i] == '.') { /* drop any fraction: cells are whole units */
            i++;
            while (text[i] >= '0' && text[i] <= '9') {
                i++;
            }
        }
        values[found++] = sign * n;
    }
    if (found >= 2) {
        *x = values[0];
        *y = values[1];
        *z = found >= 3 ? values[2] : 0;
        return found;
    }
    return 0;
}

/* units -> cell index, rounded down (may fall outside 0 .. MAP_COLS-1) */
static int cell_index(int units)
{
    int v = units + MAP_HALF_UNITS;

    return v >= 0 ? v / BLOCK_UNITS : -((-v + BLOCK_UNITS - 1) / BLOCK_UNITS);
}

static int point_in_brush(const Brush *brush, float x, float y, float z)
{
    int i;

    for (i = 0; i < brush->count; i++) {
        const Plane *plane = &brush->planes[i];

        if (plane->nx * x + plane->ny * y + plane->nz * z - plane->d > 0.5f) {
            return 0;
        }
    }
    return brush->count > 0;
}

/* Anything solid at the middle height of a cell turns that cell on: the floor
 * and ceiling slabs sit outside that plane, and the blocks themselves are
 * caught by their own planes. */
static void paint_brush(Grid *grid, const Brush *brush, const Wad *wad,
                        MapFile *info)
{
    const char *texture =
        brush->top_texture[0] != '\0' ? brush->top_texture : brush->texture;
    int cx0;
    int cy0;
    int cx1;
    int cy1;
    int painted = 0;
    int tex;
    int x;
    int y;

    if (!brush->have) {
        info->skipped++;
        return;
    }
    cx0 = cell_index(brush->minx);
    cy0 = cell_index(brush->miny);
    cx1 = cell_index(brush->maxx - 1);
    cy1 = cell_index(brush->maxy - 1);
    if (cx0 < 0) {
        cx0 = 0;
    }
    if (cy0 < 0) {
        cy0 = 0;
    }
    if (cx1 >= MAP_COLS) {
        cx1 = MAP_COLS - 1;
    }
    if (cy1 >= MAP_ROWS) {
        cy1 = MAP_ROWS - 1;
    }
    if (cx1 < cx0 || cy1 < cy0) {
        info->skipped++;
        return;
    }

    tex = mapfile_tex_index(wad, texture);
    if (tex < 0) {
        info->tex_missing++;
    }
    for (y = cy0; y <= cy1; y++) {
        for (x = cx0; x <= cx1; x++) {
            float px = (float)(x * BLOCK_UNITS + BLOCK_UNITS / 2 - MAP_HALF_UNITS);
            float py = (float)(y * BLOCK_UNITS + BLOCK_UNITS / 2 - MAP_HALF_UNITS);

            if (!point_in_brush(brush, px, py, (float)(BLOCK_HEIGHT_UNITS / 2))) {
                continue;
            }
            grid_paint(grid, x, y, tex);
            info->cells++;
            painted++;
        }
    }
    if (painted > 0) {
        info->brushes++;
    } else {
        info->skipped++;
    }
}

static void store_pair(ValueStore *values, int cx, int cy, const Pair *pair)
{
    if (strcmp(pair->key, "classname") == 0 || strcmp(pair->key, "origin") == 0) {
        return;
    }
    if (pair->value[0] == '\0') {
        return;
    }
    values_set(values, cx, cy, pair->key, pair->value);
}

int mapfile_peek_wad(const char *path, char *out, int cap)
{
    static char buffer[MAPFILE_PEEK_LEN + 1];
    FILE *file;
    size_t got;
    size_t i;

    if (out == NULL || cap <= 0) {
        return 0;
    }
    out[0] = '\0';
    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    got = fread(buffer, 1, MAPFILE_PEEK_LEN, file);
    fclose(file);
    buffer[got] = '\0';

    for (i = 0; i + 5 < got; i++) {
        if (buffer[i] != '"') {
            continue;
        }
        if (!(buffer[i + 1] == 'w' || buffer[i + 1] == 'W') ||
            !(buffer[i + 2] == 'a' || buffer[i + 2] == 'A') ||
            !(buffer[i + 3] == 'd' || buffer[i + 3] == 'D') || buffer[i + 4] != '"') {
            continue;
        }
        i += 5;
        while (i < got && (buffer[i] == ' ' || buffer[i] == '\t' ||
                           buffer[i] == '\r' || buffer[i] == '\n')) {
            i++;
        }
        if (i >= got || buffer[i] != '"') {
            return 0;
        }
        i++;
        {
            int n = 0;

            while (i < got && buffer[i] != '"' && n < cap - 1) {
                out[n++] = buffer[i++];
            }
            out[n] = '\0';
        }
        return out[0] != '\0';
    }
    return 0;
}

int mapfile_load(const char *path, Grid *grid, const Fgd *fgd,
                 ValueStore *values, const Wad *wad, MapFile *info)
{
    static Pair pairs[MAPFILE_MAX_PAIRS];
    static Brush brush;
    Reader reader;
    char token[FGD_VALUE_LEN];
    int pair_count = 0;
    int in_entity = 0;
    int ok = 1;

    if (info != NULL) {
        memset(info, 0, sizeof(*info));
        fs_copy(info->error, (int)sizeof(info->error), "");
    }
    if (grid == NULL || path == NULL) {
        return 0;
    }

    reader.file = fopen(path, "rb");
    reader.count = 0;
    if (reader.file == NULL) {
        if (info != NULL) {
            fs_copy(info->error, (int)sizeof(info->error), "CANNOT OPEN THE FILE");
        }
        return 0;
    }

    for (;;) {
        int kind = rd_token(&reader, token, (int)sizeof(token));

        if (kind == TOK_END) {
            break;
        }

        if (kind == TOK_PUNCT && strcmp(token, "{") == 0) {
            if (!in_entity) {
                in_entity = 1;
                pair_count = 0;
                memset(pairs, 0, sizeof(pairs));
                continue;
            }
            /* a brush: read its faces, then the closing brace */
            {
                int done = 0;

                brush_reset(&brush);
                while (!done) {
                    int c;

                    if (!rd_skip(&reader)) {
                        ok = 0;
                        done = 1;
                        break;
                    }
                    c = rd_get(&reader);
                    if (c == EOF) {
                        ok = 0;
                        done = 1;
                        break;
                    }
                    if (c == '}') {
                        done = 1;
                        break;
                    }
                    rd_unget(&reader, c);
                    if (!read_face(&reader, &brush)) {
                        ok = 0;
                        done = 1;
                    }
                }
                if (!ok) {
                    break;
                }
                paint_brush(grid, &brush, wad, info);
            }
            continue;
        }

        if (kind == TOK_PUNCT && strcmp(token, "}") == 0) {
            /* end of the entity: place it */
            if (in_entity) {
                const char *classname = NULL;
                int i;
                int cls = -1;
                int cx = VALUES_WORLD_CX;
                int cy = VALUES_WORLD_CY;
                int is_world = 0;

                for (i = 0; i < pair_count; i++) {
                    if (strcmp(pairs[i].key, "classname") == 0) {
                        classname = pairs[i].value;
                    }
                }
                if (classname == NULL || strcmp(classname, "worldspawn") == 0) {
                    is_world = 1;
                } else if (fgd != NULL) {
                    cls = fgd_find(fgd, classname);
                }

                if (!is_world && cls >= 0) {
                    for (i = 0; i < pair_count; i++) {
                        if (strcmp(pairs[i].key, "origin") == 0) {
                            int x = 0, y = 0, z = 0;

                            if (parse_xyz(pairs[i].value, &x, &y, &z) >= 2) {
                                cx = cell_index(x);
                                cy = cell_index(y);
                                if (cx < 0) {
                                    cx = 0;
                                }
                                if (cy < 0) {
                                    cy = 0;
                                }
                                if (cx >= MAP_COLS) {
                                    cx = MAP_COLS - 1;
                                }
                                if (cy >= MAP_ROWS) {
                                    cy = MAP_ROWS - 1;
                                }
                            }
                        }
                    }
                    grid_set_entity(grid, cx, cy, cls);
                    if (info != NULL) {
                        info->entities++;
                    }
                }

                if (values != NULL) {
                    int vx = is_world ? VALUES_WORLD_CX : cx;
                    int vy = is_world ? VALUES_WORLD_CY : cy;

                    for (i = 0; i < pair_count; i++) {
                        if (is_world && strcmp(pairs[i].key, "wad") == 0) {
                            if (info != NULL) {
                                fs_copy(info->wad, PATH_LEN, pairs[i].value);
                            }
                            continue;
                        }
                        if (!is_world && cls < 0) {
                            continue;
                        }
                        store_pair(values, vx, vy, &pairs[i]);
                    }
                }
                in_entity = 0;
            }
            continue;
        }

        if (kind == TOK_STRING && in_entity) {
            char value[FGD_VALUE_LEN];
            int vkind = rd_token(&reader, value, (int)sizeof(value));

            if (vkind != TOK_STRING) {
                continue;
            }
            if (pair_count < MAPFILE_MAX_PAIRS) {
                fs_copy(pairs[pair_count].key, FGD_NAME_LEN, token);
                fs_copy(pairs[pair_count].value, FGD_VALUE_LEN, value);
                pair_count++;
            }
            continue;
        }
    }

    fclose(reader.file);

    if (!ok) {
        if (info != NULL) {
            fs_copy(info->error, (int)sizeof(info->error), "BROKEN .MAP FILE");
        }
        return 0;
    }
    if (info != NULL) {
        info->ok = 1;
    }
    return 1;
}
