/*
 * Kabluk Mapper Portable - keyboard.c
 *
 * On screen keyboard, drawn with the same 5x7 font as the rest of the editor
 * (no SDL_ttf, no widgets: it has to work on the PSP as well).
 *
 * The layouts are plain strings, one row each, so a new key is one character
 * in a string and the navigation code never has to know about it. The bottom
 * row holds the command keys and is described separately because those keys
 * are wider than one column.
 */
#include "keyboard.h"

#include <string.h>

#include "config.h"
#include "keys.h"

/* ------------------------------------------------------------------ */
/* Geometry                                                            */
/* ------------------------------------------------------------------ */
#define KBD_X 6
#define KBD_Y 22
#define KBD_W (SCREEN_W - KBD_X * 2) /* 468 */
#define KBD_H (SCREEN_H - KBD_Y - 8) /* 242 */

#define KBD_PAD 6
#define KBD_TITLE_H 18
#define KBD_FIELD_Y (KBD_Y + KBD_TITLE_H + 6)
#define KBD_FIELD_H 18
#define KBD_GRID_Y (KBD_FIELD_Y + KBD_FIELD_H + 8)
#define KBD_GRID_H (KBD_Y + KBD_H - KBD_PAD - KBD_GRID_Y)
#define KBD_KEY_GAP 3

#define KBD_ROWS 5

/* ------------------------------------------------------------------ */
/* Layout data                                                         */
/* ------------------------------------------------------------------ */
/* four rows of characters + one row of command keys */
static const char *const text_rows[4] = {"1234567890", "QWERTYUIOP",
                                         "ASDFGHJKL-", "ZXCVBNM,./"};
static const char *const number_rows[4] = {"123", "456", "789", "-0."};

/* command keys, one entry per column of the bottom row */
#define K_CMD_DEL 1
#define K_CMD_OK 2
#define K_CMD_CANCEL 3

typedef struct KbdCommand {
    const char *label;
    int span;
    int code;
} KbdCommand;

static const KbdCommand text_commands[4] = {{"SPACE", 4, ' '},
                                            {"DEL", 2, K_CMD_DEL},
                                            {"OK", 2, K_CMD_OK},
                                            {"CANCEL", 2, K_CMD_CANCEL}};
static const KbdCommand number_commands[3] = {{"DEL", 1, K_CMD_DEL},
                                              {"OK", 1, K_CMD_OK},
                                              {"CANCEL", 1, K_CMD_CANCEL}};

/* ------------------------------------------------------------------ */
/* Small helpers                                                       */
/* ------------------------------------------------------------------ */
static int kbd_cols(const Keyboard *kbd)
{
    return kbd->numeric ? 3 : 10;
}

static int kbd_clamp(int v, int lo, int hi)
{
    if (v < lo) {
        return lo;
    }
    if (v > hi) {
        return hi;
    }
    return v;
}

static const char *kbd_row_chars(const Keyboard *kbd, int row)
{
    if (kbd->numeric) {
        return number_rows[row];
    }
    return text_rows[row];
}

static int kbd_command_count(const Keyboard *kbd)
{
    return kbd->numeric ? 3 : 4;
}

static const KbdCommand *kbd_command(const Keyboard *kbd, int nth)
{
    if (kbd->numeric) {
        return &number_commands[nth];
    }
    return &text_commands[nth];
}

/* The command key that owns 'col', or NULL. */
static const KbdCommand *kbd_command_at(const Keyboard *kbd, int col,
                                        int *first_col)
{
    int i;
    int x = 0;

    for (i = 0; i < kbd_command_count(kbd); i++) {
        const KbdCommand *cmd = kbd_command(kbd, i);

        if (col >= x && col < x + cmd->span) {
            if (first_col != NULL) {
                *first_col = x;
            }
            return cmd;
        }
        x += cmd->span;
    }
    return NULL;
}

/* Key rect on screen. Returns 0 when (row,col) is not a key. */
static int kbd_key_rect(const Keyboard *kbd, int row, int col, int *x, int *y,
                        int *w, int *h)
{
    int cols = kbd_cols(kbd);
    int cell_w = (KBD_W - KBD_PAD * 2) / cols;
    int cell_h = KBD_GRID_H / KBD_ROWS;
    int first_col = col;

    if (row < 0 || row >= KBD_ROWS || col < 0 || col >= cols) {
        return 0;
    }
    if (row == KBD_ROWS - 1) {
        if (kbd_command_at(kbd, col, &first_col) == NULL) {
            return 0;
        }
        *x = KBD_X + KBD_PAD + first_col * cell_w;
        *w = cell_w * kbd_command_at(kbd, col, NULL)->span;
    } else {
        *x = KBD_X + KBD_PAD + col * cell_w;
        *w = cell_w;
    }
    *y = KBD_GRID_Y + row * cell_h;
    *h = cell_h;
    return 1;
}

static int kbd_hit(const Keyboard *kbd, int mx, int my)
{
    int cols = kbd_cols(kbd);
    int cell_w = (KBD_W - KBD_PAD * 2) / cols;
    int cell_h = KBD_GRID_H / KBD_ROWS;
    int row;
    int col;

    if (mx < KBD_X + KBD_PAD || mx >= KBD_X + KBD_W - KBD_PAD) {
        return -1;
    }
    if (my < KBD_GRID_Y || my >= KBD_GRID_Y + cell_h * KBD_ROWS) {
        return -1;
    }
    col = (mx - (KBD_X + KBD_PAD)) / cell_w;
    row = (my - KBD_GRID_Y) / cell_h;
    if (col < 0 || col >= cols || row < 0 || row >= KBD_ROWS) {
        return -1;
    }
    return row * cols + col;
}

static void kbd_append(Keyboard *kbd, char c)
{
    if (kbd->len >= FGD_VALUE_LEN - 1) {
        return;
    }
    kbd->value[kbd->len++] = c;
    kbd->value[kbd->len] = '\0';
}

static void kbd_backspace(Keyboard *kbd)
{
    if (kbd->len > 0) {
        kbd->len--;
        kbd->value[kbd->len] = '\0';
    }
}

static int kbd_accepts(const Keyboard *kbd, char c)
{
    if (!kbd->numeric) {
        return (c >= ' ' && c < 127);
    }
    return (c >= '0' && c <= '9') || c == '-' || c == '.';
}

static char kbd_lower(char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
}

/* ------------------------------------------------------------------ */
/* Life cycle                                                          */
/* ------------------------------------------------------------------ */
void keyboard_init(Keyboard *kbd)
{
    memset(kbd, 0, sizeof(*kbd));
    kbd->hover = -1;
    kbd->cx = -1;
    kbd->cy = -1;
}

int keyboard_is_open(const Keyboard *kbd)
{
    return kbd->open;
}

void keyboard_open(Keyboard *kbd, const char *key, const char *value,
                   int numeric, int cx, int cy)
{
    int i;

    memset(kbd, 0, sizeof(*kbd));
    kbd->open = 1;
    kbd->numeric = numeric ? 1 : 0;
    kbd->hover = -1;
    kbd->row = 0;
    kbd->col = 0;
    kbd->cx = cx;
    kbd->cy = cy;

    kbd->key[0] = '\0';
    if (key != NULL) {
        for (i = 0; i < FGD_NAME_LEN - 1 && key[i] != '\0'; i++) {
            kbd->key[i] = key[i];
        }
        kbd->key[i] = '\0';
    }

    kbd->value[0] = '\0';
    kbd->len = 0;
    if (value != NULL) {
        for (i = 0; i < FGD_VALUE_LEN - 1 && value[i] != '\0'; i++) {
            kbd->value[i] = value[i];
        }
        kbd->value[i] = '\0';
        kbd->len = i;
    }
    /* the cursor starts on OK so X on a fresh keyboard just accepts */
    if (kbd->len == 0) {
        kbd->row = KBD_ROWS - 1;
        kbd->col = kbd->numeric ? 1 : 6;
    }
}

void keyboard_close(Keyboard *kbd)
{
    kbd->open = 0;
    kbd->hover = -1;
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */
static int kbd_move(Keyboard *kbd, int dx, int dy)
{
    int cols = kbd_cols(kbd);
    int row = kbd->row;
    int col = kbd->col;

    if (dy != 0) {
        row = kbd_clamp(row + dy, 0, KBD_ROWS - 1);
        col = kbd_clamp(col, 0, cols - 1);
    }
    if (dx != 0) {
        col += dx;
        if (col < 0) {
            /* wrap to the other end of the same row */
            col = cols - 1;
        } else if (col >= cols) {
            col = 0;
        }
    }
    kbd->row = row;
    kbd->col = col;
    return KBD_NONE;
}

/* X on the highlighted key */
static int kbd_press_cursor(Keyboard *kbd)
{
    if (kbd->row == KBD_ROWS - 1) {
        const KbdCommand *cmd = kbd_command_at(kbd, kbd->col, NULL);

        if (cmd == NULL) {
            return KBD_NONE;
        }
        if (cmd->code == K_CMD_OK) {
            return KBD_APPLY;
        }
        if (cmd->code == K_CMD_CANCEL) {
            return KBD_CANCEL;
        }
        kbd_backspace(kbd);
        return KBD_NONE;
    }
    if (kbd->row < 4) {
        const char *chars = kbd_row_chars(kbd, kbd->row);

        if (kbd->col >= 0 && chars[kbd->col] != '\0') {
            kbd_append(kbd, kbd_lower(chars[kbd->col]));
        }
    }
    return KBD_NONE;
}

int keyboard_handle_event(Keyboard *kbd, const SDL_Event *event)
{
    if (!kbd->open) {
        return KBD_NONE;
    }

    switch (event->type) {
    case SDL_KEYDOWN: {
        SDL_Keycode sym = event->key.keysym.sym;

        switch (sym) {
        case KEY_LEFT:
            return kbd_move(kbd, -1, 0);
        case KEY_RIGHT:
            return kbd_move(kbd, 1, 0);
        case KEY_UP:
            return kbd_move(kbd, 0, -1);
        case KEY_DOWN:
            return kbd_move(kbd, 0, 1);
        case KEY_CONFIRM:
            return kbd_press_cursor(kbd);
        case KEY_OPEN_SETTINGS: /* START / ENTER saves, like the OK key */
            return KBD_APPLY;
        case KEY_CANCEL:
        case KEY_QUIT:
            return KBD_CANCEL;
        case SDLK_BACKSPACE: {
            kbd_backspace(kbd);
            return KBD_NONE;
        }
        default:
            break;
        }

        /* desktop convenience: type on the real keyboard as well.
         * (x and z stay reserved for confirm / cancel) */
        if (sym >= 32 && sym < 127) {
            char c = (char)sym;

            if (kbd_accepts(kbd, c)) {
                kbd_append(kbd, kbd_lower(c));
            }
        }
        return KBD_NONE;
    }

    case SDL_MOUSEMOTION:
        kbd->hover = kbd_hit(kbd, event->motion.x, event->motion.y);
        return KBD_NONE;

    case SDL_MOUSEBUTTONDOWN: {
        int hit;
        int cols;

        if (event->button.button != SDL_BUTTON_LEFT) {
            return KBD_CANCEL;
        }
        hit = kbd_hit(kbd, event->button.x, event->button.y);
        if (hit < 0) {
            return KBD_CANCEL; /* clicking outside closes */
        }
        cols = kbd_cols(kbd);
        kbd->row = hit / cols;
        kbd->col = hit % cols;
        return kbd_press_cursor(kbd);
    }

    default:
        return KBD_NONE;
    }
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void kbd_draw_key(Gfx *g, const Keyboard *kbd, int row, int col,
                         const char *label, int active, int hover)
{
    int x;
    int y;
    int w;
    int h;
    int tx;
    int ty;
    unsigned int fill = COL_BTN;
    unsigned int edge = COL_BTN_EDGE;

    if (!kbd_key_rect(kbd, row, col, &x, &y, &w, &h)) {
        return;
    }
    if (active) {
        fill = COL_BTN_ACTIVE;
        edge = COL_BTN_EDGE_ACTIVE;
    } else if (hover) {
        fill = COL_BTN_HOVER;
    }
    gfx_rect(g, x, y, w - KBD_KEY_GAP, h - KBD_KEY_GAP, fill);
    gfx_outline(g, x, y, w - KBD_KEY_GAP, h - KBD_KEY_GAP, edge);

    tx = x + (w - KBD_KEY_GAP - gfx_text_width(label, 1)) / 2;
    ty = y + (h - KBD_KEY_GAP - 7) / 2;
    gfx_text(g, tx, ty, label, active ? COL_TEXT : COL_TEXT_DIM, 1);
}

void keyboard_draw(Gfx *g, const Keyboard *kbd)
{
    int cols;
    int row;
    int col;
    int x;
    int y;
    int w;
    int h;
    char label[2];
    char shown[FGD_VALUE_LEN + 2];
    int i;

    if (!kbd->open) {
        return;
    }

    gfx_rect_alpha(g, 0, 0, SCREEN_W, SCREEN_H, 0x000000, 170);

    gfx_rect(g, KBD_X, KBD_Y, KBD_W, KBD_H, COL_PANEL);
    gfx_outline(g, KBD_X, KBD_Y, KBD_W, KBD_H, COL_ACCENT);
    gfx_rect(g, KBD_X + 1, KBD_Y + 1, KBD_W - 2, KBD_TITLE_H, COL_BTN);

    gfx_text(g, KBD_X + 8, KBD_Y + 6,
             kbd->numeric ? "NUMBER" : "TEXT", COL_ACCENT, 1);
    gfx_text(g, KBD_X + 62, KBD_Y + 6, kbd->key, COL_TEXT, 1);
    gfx_text_right(g, KBD_X + KBD_W - 8, KBD_Y + 6, "[X] TYPE  [Z] CANCEL",
                   COL_TEXT_DIM, 1);

    /* value field, with a caret at the end */
    gfx_rect(g, KBD_X + KBD_PAD, KBD_FIELD_Y, KBD_W - KBD_PAD * 2, KBD_FIELD_H,
             COL_FIELD);
    gfx_outline(g, KBD_X + KBD_PAD, KBD_FIELD_Y, KBD_W - KBD_PAD * 2,
                KBD_FIELD_H, COL_BTN_EDGE);
    for (i = 0; i < kbd->len; i++) {
        shown[i] = kbd->value[i];
    }
    shown[kbd->len] = '_';
    shown[kbd->len + 1] = '\0';
    gfx_text_clip(g, KBD_X + KBD_PAD + 6, KBD_FIELD_Y + 6, KBD_W - KBD_PAD * 2 - 12,
                  shown, COL_TEXT, 1);

    cols = kbd_cols(kbd);
    for (row = 0; row < KBD_ROWS; row++) {
        for (col = 0; col < cols; col++) {
            int is_active;

            if (!kbd_key_rect(kbd, row, col, &x, &y, &w, &h)) {
                continue;
            }
            is_active = (row == kbd->row && col == kbd->col);
            if (row == KBD_ROWS - 1) {
                const KbdCommand *cmd = kbd_command_at(kbd, col, NULL);
                int first_col = col;
                int hover;

                if (cmd == NULL) {
                    continue;
                }
                kbd_command_at(kbd, col, &first_col);
                hover = (kbd->hover == row * cols + first_col);
                is_active = (row == kbd->row && kbd->col >= first_col &&
                             kbd->col < first_col + cmd->span);
                kbd_draw_key(g, kbd, row, first_col, cmd->label, is_active, hover);
                col = first_col + cmd->span - 1;
            } else {
                const char *chars = kbd_row_chars(kbd, row);

                if (chars[col] == '\0') {
                    continue;
                }
                label[0] = chars[col];
                label[1] = '\0';
                kbd_draw_key(g, kbd, row, col, label, is_active,
                             kbd->hover == row * cols + col);
            }
        }
    }
}
