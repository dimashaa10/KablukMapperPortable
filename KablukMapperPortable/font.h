/*
 * Kabluk Mapper Portable - font.h
 *
 * Built-in 5x7 bitmap font (ASCII 0x20..0x5F). No SDL_ttf / no font files
 * on disk, which keeps the PSP port trivial.
 */
#ifndef KMP_FONT_H
#define KMP_FONT_H

#define FONT_W 5
#define FONT_H 7
#define FONT_FIRST 0x20
#define FONT_LAST 0x5F

/* Returns 5 column bytes for a character; bit N of a byte = row N (top = 0).
 * Characters outside the table return the blank glyph. */
const unsigned char *font_glyph(char c);

#endif /* KMP_FONT_H */
