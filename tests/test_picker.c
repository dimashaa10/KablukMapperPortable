/*
 * Host test: the wad reader behind the previews, and the geometry of the
 * texture picker.
 *
 * Two things are checked here.
 *
 *   1. wad.c. The test writes a small WAD2 in the scratch folder and reads it
 *      back: the texture count, the palette (the lump is spelled "PALETTE" in
 *      capitals, which the reader used to miss - every texture then came out
 *      as a grey ramp), the "which mip level fits in a 40 px cell" helper and
 *      one decompressed pixel per level.
 *
 *   2. picker.c. The drawing functions are stubs that record what they were
 *      asked to draw, so the grid can be measured without a window: how many
 *      cells, how big a thumbnail is, whether everything stays on the screen,
 *      and where the arrows and a click land.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "keys.h"
#include "picker.h"
#include "textures.h"
#include "wad.h"

#define TEST_CWD_ENV "KMP_TEST_CWD"

static int failures;

static void check(int ok, const char *what)
{
    if (ok) {
        printf("%-50s ok\n", what);
    } else {
        printf("%-50s FAIL\n", what);
        failures++;
    }
}

/* ------------------------------------------------------------------ */
/* drawing stubs: they record instead of drawing                       */
/* ------------------------------------------------------------------ */
typedef struct RectRec {
    int x;
    int y;
    int w;
    int h;
} RectRec;

typedef struct TextRec {
    int x;
    int y;
    int max_w;
    char text[64];
} TextRec;

#define REC_MAX 512

static RectRec filled[REC_MAX];
static int filled_count;
static RectRec framed[REC_MAX];
static int framed_count;
static TextRec texts[REC_MAX];
static int text_count;
static TextRec clipped[REC_MAX];
static int clipped_count;

static void rec_reset(void)
{
    filled_count = 0;
    framed_count = 0;
    text_count = 0;
    clipped_count = 0;
}

void gfx_set_color(Gfx *g, unsigned int rgb)
{
    (void)g;
    (void)rgb;
}
void gfx_set_color_alpha(Gfx *g, unsigned int rgb, unsigned char alpha)
{
    (void)g;
    (void)rgb;
    (void)alpha;
}
void gfx_fill_rect(Gfx *g, int x, int y, int w, int h)
{
    (void)g;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}
void gfx_draw_rect(Gfx *g, int x, int y, int w, int h)
{
    (void)g;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
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
    (void)rgb;
    if (filled_count < REC_MAX) {
        filled[filled_count].x = x;
        filled[filled_count].y = y;
        filled[filled_count].w = w;
        filled[filled_count].h = h;
        filled_count++;
    }
}
void gfx_outline(Gfx *g, int x, int y, int w, int h, unsigned int rgb)
{
    (void)g;
    (void)rgb;
    if (framed_count < REC_MAX) {
        framed[framed_count].x = x;
        framed[framed_count].y = y;
        framed[framed_count].w = w;
        framed[framed_count].h = h;
        framed_count++;
    }
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
SDL_Texture *gfx_texture_from_argb(Gfx *g, const unsigned int *pixels, int w, int h)
{
    (void)g;
    (void)pixels;
    (void)w;
    (void)h;
    return NULL;
}
void gfx_texture_destroy(SDL_Texture *texture)
{
    (void)texture;
}
void gfx_blit(Gfx *g, SDL_Texture *texture, int x, int y, int w, int h)
{
    (void)g;
    (void)texture;
    (void)x;
    (void)y;
    (void)w;
    (void)h;
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
    (void)rgb;
    (void)scale;
    if (text_count < REC_MAX) {
        texts[text_count].x = x;
        texts[text_count].y = y;
        texts[text_count].max_w = 0;
        strncpy(texts[text_count].text, text, sizeof(texts[0].text) - 1);
        texts[text_count].text[sizeof(texts[0].text) - 1] = '\0';
        text_count++;
    }
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
void gfx_text_right(Gfx *g, int x_right, int y, const char *text, unsigned int rgb,
                    int scale)
{
    (void)g;
    (void)x_right;
    (void)y;
    (void)text;
    (void)rgb;
    (void)scale;
}
int gfx_text_clip(Gfx *g, int x, int y, int max_w, const char *text,
                  unsigned int rgb, int scale)
{
    (void)g;
    (void)rgb;
    (void)scale;
    if (clipped_count < REC_MAX) {
        clipped[clipped_count].x = x;
        clipped[clipped_count].y = y;
        clipped[clipped_count].max_w = max_w;
        strncpy(clipped[clipped_count].text, text, sizeof(clipped[0].text) - 1);
        clipped[clipped_count].text[sizeof(clipped[0].text) - 1] = '\0';
        clipped_count++;
    }
    return x;
}

/* ------------------------------------------------------------------ */
/* a small WAD2 to read                                                */
/* ------------------------------------------------------------------ */
#define TEX_COUNT 4
#define PAL_VAL(i) ((unsigned char)((i) * 7 + 1))

static const char *tex_names[TEX_COUNT] = { "T64", "T32", "T24", "T32X64" };
static const int tex_w[TEX_COUNT] = { 64, 32, 24, 32 };
static const int tex_h[TEX_COUNT] = { 64, 32, 24, 64 };

static void put32(unsigned char *p, unsigned int v)
{
    p[0] = (unsigned char)(v & 0xFF);
    p[1] = (unsigned char)((v >> 8) & 0xFF);
    p[2] = (unsigned char)((v >> 16) & 0xFF);
    p[3] = (unsigned char)((v >> 24) & 0xFF);
}

/* one RLE run per row: [count][pixels...] */
static int write_mip_level(unsigned char *out, int w, int h, int level)
{
    int row;
    int col;
    int len = 0;

    for (row = 0; row < h; row++) {
        out[len++] = (unsigned char)w;
        for (col = 0; col < w; col++) {
            out[len++] = (unsigned char)((row + col + level) & 0xFF);
        }
    }
    return len;
}

static int write_wad(const char *path)
{
    /* a 64x64 lump is 40 + 64*65 + 32*33 + 16*17 + 8*9 bytes of RLE data */
    static unsigned char lumpdata[TEX_COUNT][8192];
    static int lumpsize[TEX_COUNT];
    unsigned char palette[768];
    unsigned char header[12];
    unsigned char info[32];
    FILE *file;
    unsigned int pos;
    int i;

    for (i = 0; i < 768; i++) {
        palette[i] = PAL_VAL(i);
    }
    for (i = 0; i < TEX_COUNT; i++) {
        unsigned char *lump = lumpdata[i];
        int ofs = WAD_HEADER_SIZE;
        int level;
        int name_len = (int)strlen(tex_names[i]);

        memset(lump, 0, WAD_HEADER_SIZE);
        memcpy(lump, tex_names[i], (size_t)(name_len < 15 ? name_len : 15));
        put32(lump + 16, (unsigned int)tex_w[i]);
        put32(lump + 20, (unsigned int)tex_h[i]);
        for (level = 0; level < 4; level++) {
            int w = tex_w[i] >> level;
            int h = tex_h[i] >> level;

            put32(lump + 24 + level * 4, (unsigned int)ofs);
            ofs += write_mip_level(lump + ofs, w, h, level);
        }
        lumpsize[i] = ofs;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        return 0;
    }
    memcpy(header, "WAD2", 4);
    put32(header + 4, TEX_COUNT + 1); /* four textures and the palette */
    put32(header + 8, 12);
    fwrite(header, 1, sizeof(header), file);

    pos = 12 + (TEX_COUNT + 1) * 32;
    for (i = 0; i <= TEX_COUNT; i++) {
        memset(info, 0, sizeof(info));
        if (i < TEX_COUNT) {
            put32(info, pos);
            put32(info + 4, (unsigned int)lumpsize[i]);
            put32(info + 8, (unsigned int)lumpsize[i]);
            info[12] = WAD_LUMP_MIPTEX;
            memcpy(info + 16, tex_names[i], strlen(tex_names[i]));
            pos += (unsigned int)lumpsize[i];
        } else {
            /* capital letters on purpose: Quake tools wrote it both ways */
            put32(info, pos);
            put32(info + 4, 768);
            put32(info + 8, 768);
            info[12] = 0x40;
            memcpy(info + 16, "PALETTE", 7);
            pos += 768;
        }
        fwrite(info, 1, sizeof(info), file);
    }
    for (i = 0; i < TEX_COUNT; i++) {
        fwrite(lumpdata[i], 1, (size_t)lumpsize[i], file);
    }
    fwrite(palette, 1, sizeof(palette), file);
    fclose(file);
    return 1;
}

static void test_wad(void)
{
    Wad wad;
    unsigned char pixels[64 * 64];
    int w = 0;
    int h = 0;
    int count;

    printf("== wad reader\n");
    check(write_wad("picker_test.wad") == 1, "writes a wad to read back");

    count = wad_load(&wad, "picker_test.wad");
    check(count == TEX_COUNT, "the palette lump is not a texture");
    check(wad.count == TEX_COUNT && strcmp(wad.names[0], "T64") == 0,
          "texture names are stored");

    check(wad.has_palette == 1, "a PALETTE lump is found (any case)");
    check(wad.has_palette && wad.palette[3] == PAL_VAL(3),
          "the palette bytes are the ones in the wad");
    check(wad_palette_rgb(&wad, 3) ==
              (0xFF000000u | ((unsigned int)PAL_VAL(9) << 16) |
               ((unsigned int)PAL_VAL(10) << 8) | (unsigned int)PAL_VAL(11)),
          "palette index 3 becomes a colour");

    /* the picker asks for "a mip level that is not bigger than 40 px" */
    check(wad_fit_level(&wad, 0, 40) == 1, "64x64 -> level 1 (32x32) at 40 px");
    check(wad_fit_level(&wad, 0, 64) == 0, "64x64 -> level 0 at 64 px");
    check(wad_fit_level(&wad, 0, 8) == 3, "64x64 -> level 3 (8x8) at 8 px");
    check(wad_fit_level(&wad, 1, 40) == 0, "32x32 stays at level 0");
    check(wad_fit_level(&wad, 2, 40) == 0, "24x24 stays at level 0");
    check(wad_fit_level(&wad, 3, 40) == 1, "32x64 -> level 1 (16x32)");

    check(wad_mip_size(&wad, 3, 0) == 64, "a 32x64 texture is 64 px tall");
    check(wad_mip_size(&wad, 0, 1) == 32, "level 1 of 64x64 is 32 px");

    check(wad_read_mip(&wad, 0, 1, pixels, (int)sizeof(pixels), &w, &h) == 1 &&
              w == 32 && h == 32,
          "level 1 of a 64x64 texture is 32x32");
    check(pixels[3 * 32 + 5] == (unsigned char)((3 + 5 + 1) & 0xFF),
          "decompressed pixels match the wad");
    check(wad_read_mip(&wad, 3, 1, pixels, (int)sizeof(pixels), &w, &h) == 1 &&
              w == 16 && h == 32,
          "a 32x64 texture keeps its shape at level 1");

    /* the small preview the blocks use is still the 8 px one */
    check(wad_preview_level(&wad, 0) == 3, "block previews stay small");
    wad_close(&wad);
}

/* ------------------------------------------------------------------ */
/* the picker grid                                                     */
/* ------------------------------------------------------------------ */
static Wad fake_wad;
static Textures textures;

static void fake_wad_init(int count)
{
    int i;
    char name[16];

    memset(&fake_wad, 0, sizeof(fake_wad));
    fake_wad.kind = WAD_KIND_WAD2;
    fake_wad.count = count;
    fake_wad.opened = 1;
    for (i = 0; i < count; i++) {
        sprintf(name, "TEX%d", i);
        strncpy(fake_wad.names[i], name, WAD_NAME_LEN - 1);
        fake_wad.names[i][WAD_NAME_LEN - 1] = '\0';
        fake_wad.tex_w[i] = 64;
        fake_wad.tex_h[i] = 64;
    }
}

static int press(Picker *picker, SDL_Keycode key)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = key;
    return picker_handle_event(picker, &event);
}

/* the popup frame is framed too - a cell is the small square */
static int collect_cells(void)
{
    int i;
    int cells = 0;

    for (i = 0; i < framed_count; i++) {
        if (framed[i].w == framed[i].h && framed[i].w <= 64) {
            framed[cells++] = framed[i];
        }
    }
    return cells;
}

static void click(Picker *picker, int x, int y)
{
    SDL_Event event;

    memset(&event, 0, sizeof(event));
    event.type = SDL_MOUSEBUTTONDOWN;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = (Sint32)x;
    event.button.y = (Sint32)y;
    picker_handle_event(picker, &event);
}

static void test_picker(void)
{
    Picker picker;
    Gfx gfx;
    int i;
    int cells;
    int cols = 0;
    int rows = 0;
    int seen_x[32];
    int seen_y[32];
    int text_calls;

    printf("\n== texture picker grid\n");
    fake_wad_init(60);
    memset(&textures, 0, sizeof(textures));
    memset(&gfx, 0, sizeof(gfx));
    picker_init(&picker);
    picker_open_textures(&picker, &fake_wad, 0);

    check(picker_is_open(&picker) == 1, "the picker opens on the texture list");
    check(picker.count == 60, "every texture of the wad is offered");

    rec_reset();
    picker_draw(&gfx, &picker, NULL, &textures, &fake_wad);

    cells = collect_cells();
    check(cells >= 30, "a full page of thumbnails is drawn (>=30)");

    {
        int outside = 0;

        for (i = 0; i < cells; i++) {
            if (framed[i].x < 0 || framed[i].y < 0 ||
                framed[i].x + framed[i].w > SCREEN_W ||
                framed[i].y + framed[i].h > SCREEN_H) {
                outside++;
            }
        }
        check(outside == 0, "every thumbnail stays on the screen");
    }

    check(framed[0].w >= 34 && framed[0].h >= 34,
          "thumbnails are large (at least 34 px)");

    /* how many cells per row and per column, from where they were drawn */
    seen_x[0] = framed[0].x;
    cols = 1;
    for (i = 1; i < cells; i++) {
        if (framed[i].y != framed[0].y) {
            break;
        }
        if (seen_x[cols - 1] != framed[i].x) {
            seen_x[cols++] = framed[i].x;
        }
    }
    seen_y[0] = framed[0].y;
    rows = 1;
    for (i = 0; i < cells; i++) {
        if (framed[i].x != framed[0].x) {
            continue;
        }
        if (seen_y[rows - 1] != framed[i].y) {
            seen_y[rows++] = framed[i].y;
        }
    }
    check(cols >= 8, "the grid is at least 8 cells wide");
    check(rows == 4, "the grid is 4 rows high");
    check(cols * rows == cells, "the cells fill the whole page");

    /* one caption per cell, underneath the picture */
    text_calls = 0;
    for (i = 0; i < clipped_count; i++) {
        if (clipped[i].max_w >= 32 && clipped[i].text[0] == 'T') {
            text_calls++;
        }
    }
    check(text_calls == cells, "every cell draws its texture name");
    check(clipped[0].y > framed[0].y, "the name is below the thumbnail");
    check(cols < cells && clipped[0].y + 7 <= framed[cols].y,
          "the name does not reach the next row");

    /* the arrows move by cells */
    picker.cur = 0;
    picker.top = 0;
    press(&picker, KEY_RIGHT);
    check(picker.cur == 1, "right moves one cell");
    press(&picker, KEY_DOWN);
    check(picker.cur == 1 + cols, "down moves one row");
    press(&picker, KEY_LEFT);
    check(picker.cur == cols, "left moves one cell back");
    press(&picker, KEY_UP);
    check(picker.cur == 0, "up moves one row back");
    press(&picker, KEY_LEFT);
    check(picker.cur == 0, "left at the edge stays put");
    press(&picker, KEY_UP);
    check(picker.cur == 0, "up at the top stays put");

    /* a page scrolls by whole rows */
    for (i = 0; i < 5; i++) {
        press(&picker, KEY_DOWN);
    }
    check(picker.cur == 5 * cols, "five rows down is five cells down");
    check(picker.top % cols == 0, "the page starts on a whole row");
    check(picker.top > 0, "moving past the page scrolls it");

    /* a click lands on the cell it is over */
    rec_reset();
    picker.cur = 0;
    picker.top = 0;
    picker_draw(&gfx, &picker, NULL, &textures, &fake_wad);
    cells = collect_cells();
    check(clipped_count >= 2, "the page draws captions to click on");
    click(&picker, framed[3].x + framed[3].w / 2, framed[3].y + framed[3].h / 2);
    check(picker.cur == 3, "clicking a cell selects that texture");

    /* an empty cell of the last page is not a texture */
    picker_open_textures(&picker, &fake_wad, 59);
    rec_reset();
    picker_draw(&gfx, &picker, NULL, &textures, &fake_wad);
    cells = collect_cells();
    click(&picker, framed[cells - 1].x + framed[cells - 1].w / 2,
          framed[cells - 1].y + framed[cells - 1].h / 2);
    check(picker.cur == 59, "the last texture is reachable by a click");

    /* a wad with no palette says so instead of showing a grey name line */
    fake_wad.has_palette = 0;
    picker_open_textures(&picker, &fake_wad, 0);
    rec_reset();
    picker_draw(&gfx, &picker, NULL, &textures, &fake_wad);
    check(clipped_count > 0 &&
              strstr(clipped[clipped_count - 1].text, "PALETTE") != NULL,
          "the footer warns when there is no palette");
}

int main(void)
{
    const char *cwd = getenv(TEST_CWD_ENV);

    if (cwd != NULL) {
        if (chdir(cwd) != 0) {
            printf("cannot enter %s\n", cwd);
            return 1;
        }
    }

    test_wad();
    test_picker();

    printf("\n%s (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
