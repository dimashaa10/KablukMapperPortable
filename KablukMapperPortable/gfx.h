/*
 * Kabluk Mapper Portable - gfx.h
 *
 * Thin drawing layer on top of SDL_Renderer. Everything the editor draws
 * goes through these few functions, so the whole thing can be swapped to a
 * software renderer (or replaced by gu/pspgu) when porting to the PSP.
 */
#ifndef KMP_GFX_H
#define KMP_GFX_H

#include <SDL.h>

typedef struct Gfx {
    SDL_Renderer *renderer;
    int width;
    int height;
} Gfx;

/* Returns 1 on success, 0 on failure. */
int gfx_init(Gfx *g, SDL_Window *window);
void gfx_shutdown(Gfx *g);

/* Frame */
void gfx_frame_begin(Gfx *g, unsigned int rgb);
void gfx_frame_end(Gfx *g);

/* Solid primitives (call gfx_set_color() first) */
void gfx_set_color(Gfx *g, unsigned int rgb);
void gfx_set_color_alpha(Gfx *g, unsigned int rgb, unsigned char alpha);
void gfx_fill_rect(Gfx *g, int x, int y, int w, int h);
void gfx_draw_rect(Gfx *g, int x, int y, int w, int h);
void gfx_draw_hline(Gfx *g, int x, int y, int w);
void gfx_draw_vline(Gfx *g, int x, int y, int h);

/* One-shot helpers that set their own color */
void gfx_rect(Gfx *g, int x, int y, int w, int h, unsigned int rgb);
void gfx_outline(Gfx *g, int x, int y, int w, int h, unsigned int rgb);
void gfx_rect_alpha(Gfx *g, int x, int y, int w, int h, unsigned int rgb,
                    unsigned char alpha);

/* Text: built-in 5x7 font, no SDL_ttf needed (important for the PSP port).
 * scale 1 = 5x7 px per glyph, scale 2 = 10x14 px. Returns the x after the
 * last glyph, so calls can be chained. */
int gfx_text_width(const char *text, int scale);
int gfx_text(Gfx *g, int x, int y, const char *text, unsigned int rgb, int scale);
int gfx_text_int(Gfx *g, int x, int y, int value, unsigned int rgb, int scale);

/* Right aligned text: 'x_right' is the right edge. */
void gfx_text_right(Gfx *g, int x_right, int y, const char *text, unsigned int rgb,
                    int scale);

/* Text cut to max_w pixels, ending with ".." when it does not fit. */
int gfx_text_clip(Gfx *g, int x, int y, int max_w, const char *text,
                  unsigned int rgb, int scale);

#endif /* KMP_GFX_H */
