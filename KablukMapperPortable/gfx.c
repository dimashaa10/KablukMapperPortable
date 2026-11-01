/*
 * Kabluk Mapper Portable - gfx.c
 *
 * All SDL_Renderer calls of the project live here.
 */
#include "gfx.h"

#include "config.h"
#include "font.h"

int gfx_init(Gfx *g, SDL_Window *window)
{
    g->renderer = NULL;
    g->width = SCREEN_W;
    g->height = SCREEN_H;

    /* Accelerated + vsync if we can, otherwise whatever is available
     * (the PSP SDL port may only offer the software renderer). */
    g->renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (g->renderer == NULL) {
        g->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    }
    if (g->renderer == NULL) {
        g->renderer = SDL_CreateRenderer(window, -1, 0);
    }
    if (g->renderer == NULL) {
        SDL_Log("Renderer not created: %s", SDL_GetError());
        return 0;
    }

    /* Draw in 480x272 no matter what the window actually is. */
    SDL_RenderSetLogicalSize(g->renderer, SCREEN_W, SCREEN_H);
    SDL_SetRenderDrawBlendMode(g->renderer, SDL_BLENDMODE_BLEND);
    return 1;
}

void gfx_shutdown(Gfx *g)
{
    if (g->renderer != NULL) {
        SDL_DestroyRenderer(g->renderer);
        g->renderer = NULL;
    }
}

void gfx_frame_begin(Gfx *g, unsigned int rgb)
{
    gfx_set_color(g, rgb);
    SDL_RenderClear(g->renderer);
}

void gfx_frame_end(Gfx *g)
{
    SDL_RenderPresent(g->renderer);
}

void gfx_set_color(Gfx *g, unsigned int rgb)
{
    SDL_SetRenderDrawColor(g->renderer, (Uint8)((rgb >> 16) & 0xFF),
                           (Uint8)((rgb >> 8) & 0xFF), (Uint8)(rgb & 0xFF), 255);
}

void gfx_set_color_alpha(Gfx *g, unsigned int rgb, unsigned char alpha)
{
    SDL_SetRenderDrawColor(g->renderer, (Uint8)((rgb >> 16) & 0xFF),
                           (Uint8)((rgb >> 8) & 0xFF), (Uint8)(rgb & 0xFF), alpha);
}

void gfx_fill_rect(Gfx *g, int x, int y, int w, int h)
{
    SDL_Rect r;

    if (w <= 0 || h <= 0) {
        return;
    }
    r.x = (Sint16)x;
    r.y = (Sint16)y;
    r.w = (Uint16)w;
    r.h = (Uint16)h;
    SDL_RenderFillRect(g->renderer, &r);
}

void gfx_draw_rect(Gfx *g, int x, int y, int w, int h)
{
    if (w <= 0 || h <= 0) {
        return;
    }
    SDL_RenderDrawLine(g->renderer, x, y, x + w - 1, y);
    SDL_RenderDrawLine(g->renderer, x, y + h - 1, x + w - 1, y + h - 1);
    SDL_RenderDrawLine(g->renderer, x, y, x, y + h - 1);
    SDL_RenderDrawLine(g->renderer, x + w - 1, y, x + w - 1, y + h - 1);
}

void gfx_draw_hline(Gfx *g, int x, int y, int w)
{
    if (w <= 0) {
        return;
    }
    SDL_RenderDrawLine(g->renderer, x, y, x + w - 1, y);
}

void gfx_draw_vline(Gfx *g, int x, int y, int h)
{
    if (h <= 0) {
        return;
    }
    SDL_RenderDrawLine(g->renderer, x, y, x, y + h - 1);
}

void gfx_rect(Gfx *g, int x, int y, int w, int h, unsigned int rgb)
{
    gfx_set_color(g, rgb);
    gfx_fill_rect(g, x, y, w, h);
}

void gfx_outline(Gfx *g, int x, int y, int w, int h, unsigned int rgb)
{
    gfx_set_color(g, rgb);
    gfx_draw_rect(g, x, y, w, h);
}

void gfx_rect_alpha(Gfx *g, int x, int y, int w, int h, unsigned int rgb,
                    unsigned char alpha)
{
    gfx_set_color_alpha(g, rgb, alpha);
    gfx_fill_rect(g, x, y, w, h);
    gfx_set_color(g, COL_BG); /* restore opaque drawing */
}

SDL_Texture *gfx_texture_from_argb(Gfx *g, const unsigned int *pixels, int w, int h)
{
    SDL_Texture *texture;

    if (pixels == NULL || w <= 0 || h <= 0) {
        return NULL;
    }
    texture = SDL_CreateTexture(g->renderer, SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STATIC, w, h);
    if (texture == NULL) {
        return NULL;
    }
    if (SDL_UpdateTexture(texture, NULL, pixels, w * 4) != 0) {
        SDL_DestroyTexture(texture);
        return NULL;
    }
    return texture;
}

void gfx_texture_destroy(SDL_Texture *texture)
{
    if (texture != NULL) {
        SDL_DestroyTexture(texture);
    }
}

void gfx_blit(Gfx *g, SDL_Texture *texture, int x, int y, int w, int h)
{
    SDL_Rect dst;

    if (texture == NULL || w <= 0 || h <= 0) {
        return;
    }
    dst.x = (Sint16)x;
    dst.y = (Sint16)y;
    dst.w = (Uint16)w;
    dst.h = (Uint16)h;
    SDL_RenderCopy(g->renderer, texture, NULL, &dst);
}

int gfx_text_width(const char *text, int scale)
{
    int len = 0;

    if (text == NULL) {
        return 0;
    }
    while (text[len] != '\0') {
        len++;
    }
    if (len == 0) {
        return 0;
    }
    /* one blank column between glyphs, none after the last one */
    return len * (FONT_W + 1) * scale - scale;
}

int gfx_text(Gfx *g, int x, int y, const char *text, unsigned int rgb, int scale)
{
    int i;
    int col;
    int row;
    int len;
    int run;
    int px;
    const unsigned char *glyph;

    if (text == NULL || scale < 1) {
        return x;
    }

    len = 0;
    while (text[len] != '\0') {
        len++;
    }

    gfx_set_color(g, rgb);
    px = x;
    for (i = 0; i < len; i++) {
        glyph = font_glyph(text[i]);
        for (col = 0; col < FONT_W; col++) {
            row = 0;
            while (row < FONT_H) {
                if ((glyph[col] & (1 << row)) == 0) {
                    row++;
                    continue;
                }
                /* merge vertical runs of pixels into a single rect */
                run = 0;
                while (row + run < FONT_H && (glyph[col] & (1 << (row + run)))) {
                    run++;
                }
                gfx_fill_rect(g, px + col * scale, y + row * scale, scale, run * scale);
                row += run;
            }
        }
        px += (FONT_W + 1) * scale;
    }
    return px;
}

void gfx_text_right(Gfx *g, int x_right, int y, const char *text, unsigned int rgb,
                    int scale)
{
    gfx_text(g, x_right - gfx_text_width(text, scale), y, text, rgb, scale);
}

int gfx_text_clip(Gfx *g, int x, int y, int max_w, const char *text,
                  unsigned int rgb, int scale)
{
    char buf[64];
    int limit;
    int len = 0;
    int i;

    if (text == NULL || scale < 1 || max_w <= 0) {
        return x;
    }
    limit = max_w / ((FONT_W + 1) * scale);
    if (limit < 1) {
        return x;
    }
    if (limit > (int)sizeof(buf) - 1) {
        limit = (int)sizeof(buf) - 1;
    }
    while (text[len] != '\0' && len < limit) {
        len++;
    }
    if (text[len] == '\0') {
        return gfx_text(g, x, y, text, rgb, scale);
    }
    /* truncate: keep limit-2 glyphs and append ".." */
    if (len > 2) {
        len -= 2;
    } else {
        len = 0;
    }
    for (i = 0; i < len; i++) {
        buf[i] = text[i];
    }
    buf[len++] = '.';
    buf[len++] = '.';
    buf[len] = '\0';
    return gfx_text(g, x, y, buf, rgb, scale);
}

int gfx_text_int(Gfx *g, int x, int y, int value, unsigned int rgb, int scale)
{
    char buf[12];
    int n = 0;
    int i;
    int neg = 0;
    char tmp;

    if (value < 0) {
        neg = 1;
        value = -value;
    }
    do {
        buf[n++] = (char)('0' + (value % 10));
        value /= 10;
    } while (value > 0 && n < 10);
    if (neg) {
        buf[n++] = '-';
    }
    for (i = 0; i < n / 2; i++) {
        tmp = buf[i];
        buf[i] = buf[n - 1 - i];
        buf[n - 1 - i] = tmp;
    }
    buf[n] = '\0';
    return gfx_text(g, x, y, buf, rgb, scale);
}
