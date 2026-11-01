/*
 * Kabluk Mapper Portable - compileui.h
 *
 * The START menu: output path, the compile log and the two buttons.
 * It only reports what the user asked for, main.c does the work.
 */
#ifndef KMP_COMPILEUI_H
#define KMP_COMPILEUI_H

#include <SDL.h>

#include "compile.h"
#include "gfx.h"
#include "settings.h"

#define COMPILEUI_NONE 0
#define COMPILEUI_COMPILE 1
#define COMPILEUI_BROWSE 2
#define COMPILEUI_CLOSE 3

typedef struct CompileUi {
    int open;
    int hover; /* 0 = none, see COMPILEUI_* */
} CompileUi;

void compileui_init(CompileUi *ui);
int compileui_is_open(const CompileUi *ui);
void compileui_open(CompileUi *ui);
void compileui_close(CompileUi *ui);

/* Returns one of the COMPILEUI_* actions. */
int compileui_handle_event(CompileUi *ui, const SDL_Event *event, CompileJob *job);
void compileui_draw(Gfx *g, const CompileUi *ui, const CompileJob *job,
                    const Settings *settings);

#endif /* KMP_COMPILEUI_H */
