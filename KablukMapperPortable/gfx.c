/*
 * Kabluk Mapper Portable - gfx.c
 *
 * All SDL_Renderer calls of the project live here.
 */
#include "gfx.h"

#include "config.h"
#include "font.h"

/* The biggest texture the editor builds is a 64x64 block preview. */
#define GFX_CONVERT_MAX 64

/* Textures are built from ARGB8888 pixels, but a renderer only takes the
 * formats it advertises - the PSP one works in ABGR and does not list
 * ARGB8888 at all, and SDL refuses a format the renderer does not know. So
 * the format is negotiated once, and the pixels are converted on upload. */
static unsigned int gfx_convert32[GFX_CONVERT_MAX * GFX_CONVERT_MAX];
static unsigned short gfx_convert16[GFX_CONVERT_MAX * GFX_CONVERT_MAX];

static unsigned int gfx_choose_format(SDL_Renderer *renderer)
{
    static const Uint32 wanted[] = {
        SDL_PIXELFORMAT_ARGB8888, SDL_PIXELFORMAT_ABGR8888,
        SDL_PIXELFORMAT_ABGR4444, SDL_PIXELFORMAT_ABGR1555,
        SDL_PIXELFORMAT_ARGB4444
    };
    SDL_RendererInfo info;
    int i;
    int f;

    if (SDL_GetRendererInfo(renderer, &info) != 0) {
        return SDL_PIXELFORMAT_ARGB8888;
    }
    for (i = 0; i < (int)(sizeof(wanted) / sizeof(wanted[0])); i++) {
        for (f = 0; f < (int)info.num_texture_formats; f++) {
            if (info.texture_formats[f] == wanted[i]) {
                return wanted[i];
            }
        }
    }
    return SDL_PIXELFORMAT_ARGB8888;
}

/* ARGB8888 (0xAARRGGBB) -> the renderer's format. Returns the pitch, or 0
 * when the texture is bigger than the scratch buffers. */
static int gfx_convert_pixels(const Gfx *g, const unsigned int *pixels, int w,
                              int h, const void **out)
{
    int count;
    int i;

    if (g->tex_format == SDL_PIXELFORMAT_ARGB8888) {
        *out = pixels;
        return w * 4;
    }
    if (w > GFX_CONVERT_MAX || h > GFX_CONVERT_MAX) {
        *out = NULL;
        return 0;
    }
    count = w * h;

    if (g->tex_format == SDL_PIXELFORMAT_ABGR8888) {
        for (i = 0; i < count; i++) {
            unsigned int c = pixels[i];

            gfx_convert32[i] = (c & 0xFF00FF00u) | ((c & 0x00FF0000u) >> 16) |
                               ((c & 0x000000FFu) << 16);
        }
        *out = gfx_convert32;
        return w * 4;
    }

    /* 16 bit: R, G and B lose the low bits, and alpha is on or off in 1555 */
    for (i = 0; i < count; i++) {
        unsigned int c = pixels[i];
        unsigned int a = (c >> 24) & 0xFFu;
        unsigned int r = (c >> 16) & 0xFFu;
        unsigned int green = (c >> 8) & 0xFFu;
        unsigned int b = c & 0xFFu;

        if (g->tex_format == SDL_PIXELFORMAT_ABGR1555) {
            gfx_convert16[i] = (unsigned short)((a >= 0x80 ? 0x8000u : 0u) |
                                                ((b >> 3) << 10) |
                                                ((green >> 3) << 5) | (r >> 3));
        } else {
            gfx_convert16[i] = (unsigned short)(((a >> 4) << 12) |
                                                ((b >> 4) << 8) |
                                                ((green >> 4) << 4) |
                                                (r >> 4));
        }
    }
    *out = gfx_convert16;
    return w * 2;
}

int gfx_init(Gfx *g, SDL_Window *window)
{
    g->renderer = NULL;
    g->width = SCREEN_W;
    g->height = SCREEN_H;
    g->tex_format = SDL_PIXELFORMAT_ARGB8888;

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
    g->tex_format = gfx_choose_format(g->renderer);
    SDL_Log("renderer textures: format 0x%08X", (unsigned int)g->tex_format);
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
    const void *data = NULL;
    int pitch;

    if (pixels == NULL || w <= 0 || h <= 0) {
        return NULL;
    }
    pitch = gfx_convert_pixels(g, pixels, w, h, &data);
    if (pitch == 0 || data == NULL) {
        SDL_Log("texture %dx%d does not fit the converter", w, h);
        return NULL;
    }
    texture = SDL_CreateTexture(g->renderer, g->tex_format,
                                SDL_TEXTUREACCESS_STATIC, w, h);
    if (texture == NULL) {
        return NULL;
    }
    if (SDL_UpdateTexture(texture, NULL, data, pitch) != 0) {
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

void gfx_blit_fit(Gfx *g, SDL_Texture *texture, int x, int y, int w, int h)
{
    unsigned int format = 0;
    int tex_w = 0;
    int tex_h = 0;
    int draw_w;
    int draw_h;

    if (texture == NULL || w <= 0 || h <= 0) {
        return;
    }
    if (SDL_QueryTexture(texture, &format, NULL, &tex_w, &tex_h) != 0) {
        return;
    }
    if (tex_w <= 0 || tex_h <= 0) {
        return;
    }

    /* the limiting side decides the scale, the other one follows it */
    if (w * tex_h <= h * tex_w) {
        draw_w = w;
        draw_h = (w * tex_h) / tex_w;
    } else {
        draw_h = h;
        draw_w = (h * tex_w) / tex_h;
    }
    if (draw_w < 1) {
        draw_w = 1;
    }
    if (draw_h < 1) {
        draw_h = 1;
    }
    gfx_blit(g, texture, x + (w - draw_w) / 2, y + (h - draw_h) / 2, draw_w, draw_h);
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
