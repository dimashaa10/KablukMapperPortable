/*
 * Kabluk Mapper Portable - compileui.c
 *
 * Small compile window: where the map is written to, a scrolling log and the
 * COMPILE MAP / CLOSE buttons.
 */
#include "compileui.h"

#include "config.h"
#include "keys.h"

#define WX 8
#define WY 8
#define WW (SCREEN_W - WX * 2) /* 464 */
#define WH 256

#define PAD 10
#define BTN_H 22

#define OUT_Y (WY + 36)
#define OUT_H 18
#define OUT_W (WW - PAD * 2)

#define BROWSE_W 60
#define LOG_Y (WY + 70)
#define LOG_H 120
#define LOG_TEXT_Y (LOG_Y + 6)
#define LOG_LINE_H 12
#define LOG_VISIBLE 9

#define BAR_Y (WY + 198)
#define STATUS_Y (WY + 210)
#define BUTTON_Y (WY + 222)

#define COMPILE_W 122
#define CLOSE_W 92

static int ui_in_rect(int mx, int my, int x, int y, int w, int h)
{
    return mx >= x && mx < x + w && my >= y && my < y + h;
}

static void rect_browse(int *x, int *y, int *w, int *h)
{
    *x = WX + PAD + OUT_W - BROWSE_W - 2;
    *y = OUT_Y + 2;
    *w = BROWSE_W;
    *h = OUT_H - 4;
}

static void rect_compile(int *x, int *y, int *w, int *h)
{
    *x = WX + PAD;
    *y = BUTTON_Y;
    *w = COMPILE_W;
    *h = BTN_H;
}

static void rect_close(int *x, int *y, int *w, int *h)
{
    *x = WX + WW - PAD - CLOSE_W;
    *y = BUTTON_Y;
    *w = CLOSE_W;
    *h = BTN_H;
}

void compileui_init(CompileUi *ui)
{
    ui->open = 0;
    ui->hover = COMPILEUI_NONE;
}

int compileui_is_open(const CompileUi *ui)
{
    return ui->open;
}

void compileui_open(CompileUi *ui)
{
    ui->open = 1;
    ui->hover = COMPILEUI_NONE;
}

void compileui_close(CompileUi *ui)
{
    ui->open = 0;
    ui->hover = COMPILEUI_NONE;
}

int compileui_handle_event(CompileUi *ui, const SDL_Event *event, CompileJob *job)
{
    int x;
    int y;
    int w;
    int h;

    if (!ui->open) {
        return COMPILEUI_NONE;
    }

    switch (event->type) {
    case SDL_MOUSEMOTION:
        ui->hover = COMPILEUI_NONE;
        rect_browse(&x, &y, &w, &h);
        if (ui_in_rect(event->motion.x, event->motion.y, x, y, w, h)) {
            ui->hover = COMPILEUI_BROWSE;
        }
        rect_compile(&x, &y, &w, &h);
        if (ui_in_rect(event->motion.x, event->motion.y, x, y, w, h)) {
            ui->hover = COMPILEUI_COMPILE;
        }
        rect_close(&x, &y, &w, &h);
        if (ui_in_rect(event->motion.x, event->motion.y, x, y, w, h)) {
            ui->hover = COMPILEUI_CLOSE;
        }
        return COMPILEUI_NONE;

    case SDL_MOUSEBUTTONDOWN:
        if (event->button.button != SDL_BUTTON_LEFT) {
            return COMPILEUI_CLOSE;
        }
        rect_browse(&x, &y, &w, &h);
        if (ui_in_rect(event->button.x, event->button.y, x, y, w, h)) {
            return COMPILEUI_BROWSE;
        }
        rect_compile(&x, &y, &w, &h);
        if (ui_in_rect(event->button.x, event->button.y, x, y, w, h)) {
            return compile_is_running(job) ? COMPILEUI_NONE : COMPILEUI_COMPILE;
        }
        rect_close(&x, &y, &w, &h);
        if (ui_in_rect(event->button.x, event->button.y, x, y, w, h)) {
            return COMPILEUI_CLOSE;
        }
        return COMPILEUI_NONE;

    case SDL_KEYDOWN:
        switch (event->key.keysym.sym) {
        case KEY_CONFIRM:
            return compile_is_running(job) ? COMPILEUI_NONE : COMPILEUI_COMPILE;
        case KEY_CANCEL:
        case KEY_OPEN_COMPILE:
            return COMPILEUI_CLOSE;
        case KEY_UP:
            compile_scroll(job, 1);
            return COMPILEUI_NONE;
        case KEY_DOWN:
            compile_scroll(job, -1);
            return COMPILEUI_NONE;
        default:
            return COMPILEUI_NONE;
        }

    case SDL_MOUSEWHEEL:
        compile_scroll(job, event->wheel.y > 0 ? 1 : -1);
        return COMPILEUI_NONE;

    default:
        return COMPILEUI_NONE;
    }
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
static void draw_button(Gfx *g, int x, int y, int w, int h, const char *label,
                        int active, int hot, int enabled)
{
    unsigned int bg = active ? COL_BTN_ACTIVE : (hot ? COL_BTN_HOVER : COL_BTN);
    unsigned int edge = active ? COL_BTN_EDGE_ACTIVE : COL_BTN_EDGE;
    unsigned int text = enabled ? (active ? COL_ACCENT : COL_TEXT) : COL_TEXT_DIM;
    int label_w = gfx_text_width(label, 1);

    if (!enabled) {
        bg = COL_BTN;
        edge = COL_BTN_EDGE;
    }
    gfx_rect(g, x, y, w, h, bg);
    gfx_outline(g, x, y, w, h, edge);
    gfx_text(g, x + (w - label_w) / 2, y + (h - 7) / 2, label, text, 1);
}

void compileui_draw(Gfx *g, const CompileUi *ui, const CompileJob *job,
                    const Settings *settings)
{
    int x;
    int y;
    int w;
    int h;
    int i;
    int start;
    char status[40];
    int len;
    const char *out_dir;

    if (!ui->open) {
        return;
    }

    gfx_rect_alpha(g, 0, 0, SCREEN_W, SCREEN_H, 0x000000, 170);

    gfx_rect(g, WX, WY, WW, WH, COL_PANEL);
    gfx_outline(g, WX, WY, WW, WH, COL_ACCENT);
    gfx_rect(g, WX + 1, WY + 1, WW - 2, 20, COL_BTN);

    gfx_text(g, WX + PAD, WY + 7, "COMPILE MAP", COL_ACCENT, 1);
    gfx_text_right(g, WX + WW - PAD, WY + 7, "[X] COMPILE   [Z] CLOSE",
                   COL_TEXT_DIM, 1);

    /* output folder */
    out_dir = settings->out_dir[0] != '\0' ? settings->out_dir : ".";
    gfx_text(g, WX + PAD, OUT_Y - 11, "OUTPUT FOLDER", COL_TEXT_DIM, 1);

    gfx_rect(g, WX + PAD, OUT_Y, OUT_W, OUT_H, COL_FIELD);
    gfx_outline(g, WX + PAD, OUT_Y, OUT_W, OUT_H,
                ui->hover == COMPILEUI_BROWSE ? COL_ACCENT : COL_BTN_EDGE);
    gfx_text_clip(g, WX + PAD + 5, OUT_Y + 6, OUT_W - BROWSE_W - 16, out_dir,
                  COL_TEXT, 1);

    rect_browse(&x, &y, &w, &h);
    draw_button(g, x, y, w, h, "BROWSE", 0, ui->hover == COMPILEUI_BROWSE, 1);

    /* log window */
    gfx_text(g, WX + PAD, LOG_Y - 11, "LOG", COL_TEXT_DIM, 1);
    gfx_rect(g, WX + PAD, LOG_Y, OUT_W, LOG_H, COL_FIELD);
    gfx_outline(g, WX + PAD, LOG_Y, OUT_W, LOG_H, COL_BTN_EDGE);

    start = job->line_count - LOG_VISIBLE - job->scroll;
    if (start < 0) {
        start = 0;
    }
    for (i = 0; i < LOG_VISIBLE; i++) {
        int index = start + i;

        if (index >= job->line_count) {
            break;
        }
        gfx_text_clip(g, WX + PAD + 6, LOG_TEXT_Y + i * LOG_LINE_H, OUT_W - 14,
                      job->log[index],
                      index + 1 == job->line_count ? COL_TEXT : COL_TEXT_DIM, 1);
    }
    if (job->line_count == 0) {
        gfx_text(g, WX + PAD + 6, LOG_TEXT_Y, "PRESS COMPILE MAP TO START",
                 COL_TEXT_DIM, 1);
    }

    /* progress bar */
    gfx_rect(g, WX + PAD, BAR_Y, OUT_W, 8, COL_BTN);
    if (job->progress > 0) {
        gfx_rect(g, WX + PAD, BAR_Y, OUT_W * job->progress / 100, 8,
                 job->state == COMPILE_FAILED ? COL_WARN : COL_ACCENT);
    }
    gfx_outline(g, WX + PAD, BAR_Y, OUT_W, 8, COL_BTN_EDGE);

    /* status line */
    len = 0;
    status[len] = '\0';
    {
        const char *text = compile_status_text(job);
        int t = 0;
        while (text[t] != '\0' && len < (int)sizeof(status) - 8) {
            status[len++] = text[t++];
        }
    }
    if (job->state == COMPILE_RUNNING || job->state == COMPILE_DONE) {
        char num[8];
        int n = 0;
        int v = job->progress;

        if (v >= 100) {
            num[n++] = '1';
            num[n++] = '0';
            num[n++] = '0';
        } else {
            if (v >= 10) {
                num[n++] = (char)('0' + v / 10);
            }
            num[n++] = (char)('0' + v % 10);
        }
        num[n] = '\0';
        status[len++] = ' ';
        for (i = 0; i < n; i++) {
            status[len++] = num[i];
        }
        status[len++] = '%';
    }
    status[len] = '\0';
    gfx_text(g, WX + PAD, STATUS_Y, status,
             job->state == COMPILE_FAILED ? COL_WARN
                                          : (job->state == COMPILE_DONE ? COL_OK
                                                                       : COL_TEXT_DIM),
             1);

    /* buttons */
    rect_compile(&x, &y, &w, &h);
    draw_button(g, x, y, w, h, "COMPILE MAP", job->state == COMPILE_DONE || job->state == COMPILE_RUNNING,
                ui->hover == COMPILEUI_COMPILE, !compile_is_running(job));
    rect_close(&x, &y, &w, &h);
    draw_button(g, x, y, w, h, "CLOSE", 0, ui->hover == COMPILEUI_CLOSE, 1);
}
