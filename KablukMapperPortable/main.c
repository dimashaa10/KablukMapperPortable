/*
 * Kabluk Mapper Portable - main.c
 *
 * A very small Quake 1 map editor, written so it can be ported to the PSP
 * with pspsdk + SDL2 later:
 *   - fixed 480x272 resolution (PSP LCD), all layout in config.h
 *   - plain C, no WinAPI, no SDL_ttf, no dynamic memory
 *   - mouse OR d-pad/keyboard drive the same cursor, so the editor is fully
 *     usable without a mouse
 *
 * Controls (desktop now, d-pad later):
 *   mouse / arrows     move the grid cursor
 *   left click / X     place a solid cell
 *   right click / Z    erase a cell
 *   1..4 / TAB         pick a tool in the left panel
 *   ESC                quit
 */
#include <SDL.h>

#include "config.h"
#include "gfx.h"
#include "grid.h"
#include "ui.h"

typedef struct App {
    SDL_Window *window;
    Gfx gfx;
    Grid grid;
    int running;
    int tool;
    int hover_tool;
    int mouse_x;
    int mouse_y;
} App;

static void app_place(App *app, unsigned char value)
{
    grid_set_cell(&app->grid, app->grid.cursor_x, app->grid.cursor_y, value);
}

/* Mouse position in window pixels -> 480x272 logical pixels. */
static void app_update_mouse(App *app)
{
    int mx;
    int my;
    int ww;
    int wh;

    mx = app->mouse_x;
    my = app->mouse_y;
    SDL_GetWindowSize(app->window, &ww, &wh);
    if (ww > 0 && wh > 0 && (ww != SCREEN_W || wh != SCREEN_H)) {
        mx = mx * SCREEN_W / ww;
        my = my * SCREEN_H / wh;
    }
    app->mouse_x = mx;
    app->mouse_y = my;
}

static void app_move_cursor_to_mouse(App *app)
{
    int cx;
    int cy;

    if (grid_screen_to_cell(&app->grid, app->mouse_x, app->mouse_y, &cx, &cy)) {
        app->grid.cursor_x = cx;
        app->grid.cursor_y = cy;
    }
}

static void app_handle_key(App *app, const SDL_KeyboardEvent *key)
{
    switch (key->keysym.sym) {
    case SDLK_ESCAPE:
        app->running = 0;
        break;
    case SDLK_LEFT:
        grid_move_cursor(&app->grid, -1, 0);
        break;
    case SDLK_RIGHT:
        grid_move_cursor(&app->grid, 1, 0);
        break;
    case SDLK_UP:
        grid_move_cursor(&app->grid, 0, -1);
        break;
    case SDLK_DOWN:
        grid_move_cursor(&app->grid, 0, 1);
        break;
    case SDLK_x:
    case SDLK_SPACE:
        app_place(app, CELL_SOLID);
        break;
    case SDLK_z:
    case SDLK_BACKSPACE:
        app_place(app, CELL_EMPTY);
        break;
    case SDLK_1:
        app->tool = 0;
        break;
    case SDLK_2:
        app->tool = 1;
        break;
    case SDLK_3:
        app->tool = 2;
        break;
    case SDLK_4:
        app->tool = 3;
        break;
    case SDLK_TAB:
        /* handy on the PSP where number keys do not exist */
        app->tool = (app->tool + 1) % PANEL_BUTTONS;
        break;
    default:
        break;
    }
}

static void app_handle_event(App *app, const SDL_Event *event)
{
    int hit;

    switch (event->type) {
    case SDL_QUIT:
        app->running = 0;
        break;

    case SDL_KEYDOWN:
        app_handle_key(app, &event->key);
        break;

    case SDL_MOUSEMOTION:
        app->mouse_x = event->motion.x;
        app->mouse_y = event->motion.y;
        app_update_mouse(app);
        app->hover_tool = ui_hit_test(app->mouse_x, app->mouse_y);
        app_move_cursor_to_mouse(app);
        /* dragging while a button is held paints continuously */
        if ((event->motion.state & SDL_BUTTON_LMASK) != 0) {
            app_place(app, CELL_SOLID);
        } else if ((event->motion.state & SDL_BUTTON_RMASK) != 0) {
            app_place(app, CELL_EMPTY);
        }
        break;

    case SDL_MOUSEBUTTONDOWN:
        app->mouse_x = event->button.x;
        app->mouse_y = event->button.y;
        app_update_mouse(app);
        hit = ui_hit_test(app->mouse_x, app->mouse_y);
        if (hit >= 0) {
            app->tool = hit;
            break;
        }
        app_move_cursor_to_mouse(app);
        if (event->button.button == SDL_BUTTON_LEFT) {
            app_place(app, CELL_SOLID);
        } else if (event->button.button == SDL_BUTTON_RIGHT) {
            app_place(app, CELL_EMPTY);
        }
        break;

    default:
        break;
    }
}

static void app_render(App *app)
{
    gfx_frame_begin(&app->gfx, COL_BG);

    ui_draw_panel(&app->gfx, app->tool, app->hover_tool);
    grid_draw(&app->gfx, &app->grid);

    gfx_frame_end(&app->gfx);
}

int main(int argc, char *argv[])
{
    App app;
    SDL_Window *window;
    SDL_Event event;
    unsigned int frame_start;
    unsigned int frame_time;

    (void)argc;
    (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL init failed: %s", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, SCREEN_W, SCREEN_H,
                              SDL_WINDOW_SHOWN);
    if (window == NULL) {
        SDL_Log("Window not created: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    if (!gfx_init(&app.gfx, window)) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    app.window = window;
    grid_init(&app.grid);
    app.running = 1;
    app.tool = TOOL_SELECT;
    app.hover_tool = -1;
    app.mouse_x = 0;
    app.mouse_y = 0;

    while (app.running) {
        frame_start = SDL_GetTicks();

        while (SDL_PollEvent(&event)) {
            app_handle_event(&app, &event);
        }

        app_render(&app);

        /* ~60 fps cap: SDL_Delay keeps the CPU idle (and the PSP happy) */
        frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_MS) {
            SDL_Delay(FRAME_MS - frame_time);
        }
    }

    gfx_shutdown(&app.gfx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
