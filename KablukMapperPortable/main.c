/*
 * Kabluk Mapper Portable - main.c
 *
 * A very small Quake 1 map editor, written so it can be ported to the PSP
 * with pspsdk + SDL2 later:
 *   - fixed 480x272 resolution (PSP LCD), all layout in config.h
 *   - plain C, no WinAPI, no SDL_ttf, no dynamic memory
 *   - mouse OR d-pad/keyboard drive the same cursor, so the editor is fully
 *     usable without a mouse
 *   - one cell in the grid is one 128x128x128 unit block, the map always has
 *     a floor/ceiling shell so nothing can leak into the void
 *
 * Controls (desktop now, PSP buttons later - see keys.h):
 *   mouse / d-pad     move the grid cursor
 *   left click / X    place a block
 *   right click / Z   erase a block (the outer shell cannot be erased)
 *   1..4 / TAB        pick a tool in the left panel
 *   Q / E             fold the left / right panel  (PSP: L / R)
 *   ENTER             editor settings              (PSP: SELECT)
 *   ESC               quit
 */
#include <SDL.h>

#include "config.h"
#include "gfx.h"
#include "grid.h"
#include "keys.h"
#include "layout.h"
#include "menu.h"
#include "props.h"
#include "settings.h"
#include "ui.h"

typedef struct App {
    SDL_Window *window;
    Gfx gfx;
    Grid grid;
    Layout layout;
    Settings settings;
    Menu menu;
    int running;
    int tool;
    int hover_tool;
    int left_open;
    int right_open;
    int mouse_x;
    int mouse_y;
} App;

static void app_refresh_layout(App *app)
{
    layout_compute(&app->layout, app->left_open, app->right_open);
    grid_clamp_camera(&app->grid, &app->layout);
}

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

/* The grid is only under the space between the panels, so this also tells
 * us whether the mouse is over the map at all. */
static void app_move_cursor_to_mouse(App *app)
{
    int cx;
    int cy;

    if (grid_screen_to_cell(&app->grid, &app->layout, app->mouse_x, app->mouse_y,
                            &cx, &cy)) {
        app->grid.cursor_x = cx;
        app->grid.cursor_y = cy;
    }
}

static int app_over_left_panel(const App *app)
{
    return app->left_open && app->mouse_x < PANEL_W;
}

static int app_over_right_panel(const App *app)
{
    return app->right_open && app->mouse_x >= SCREEN_W - RIGHT_PANEL_W;
}

static void app_handle_key(App *app, const SDL_KeyboardEvent *key)
{
    switch (key->keysym.sym) {
    case KEY_QUIT:
        app->running = 0;
        break;

    case KEY_OPEN_SETTINGS:
        menu_open_settings(&app->menu);
        break;

    case KEY_PANEL_LEFT:
        app->left_open = !app->left_open;
        app_refresh_layout(app);
        break;

    case KEY_PANEL_RIGHT:
        app->right_open = !app->right_open;
        app_refresh_layout(app);
        break;

    case KEY_LEFT:
        grid_move_cursor(&app->grid, &app->layout, -1, 0);
        break;
    case KEY_RIGHT:
        grid_move_cursor(&app->grid, &app->layout, 1, 0);
        break;
    case KEY_UP:
        grid_move_cursor(&app->grid, &app->layout, 0, -1);
        break;
    case KEY_DOWN:
        grid_move_cursor(&app->grid, &app->layout, 0, 1);
        break;

    case KEY_CONFIRM:
        app_place(app, CELL_SOLID);
        break;
    case KEY_CANCEL:
        app_place(app, CELL_EMPTY);
        break;

    case KEY_TOOL_1:
        app->tool = 0;
        break;
    case KEY_TOOL_2:
        app->tool = 1;
        break;
    case KEY_TOOL_3:
        app->tool = 2;
        break;
    case KEY_TOOL_4:
        app->tool = 3;
        break;
    case KEY_TOOL_NEXT:
        app->tool = (app->tool + 1) % PANEL_BUTTONS;
        break;

    default:
        break;
    }
}

static void app_handle_event(App *app, const SDL_Event *event)
{
    int hit;

    if (event->type == SDL_QUIT) {
        app->running = 0;
        return;
    }

    /* the modal screens take everything while they are open */
    if (menu_is_open(&app->menu)) {
        menu_handle_event(&app->menu, event);
        return;
    }

    switch (event->type) {
    case SDL_KEYDOWN:
        app_handle_key(app, &event->key);
        break;

    case SDL_MOUSEMOTION:
        app->mouse_x = event->motion.x;
        app->mouse_y = event->motion.y;
        app_update_mouse(app);
        app->hover_tool = app_over_left_panel(app)
                              ? ui_hit_test(app->mouse_x, app->mouse_y)
                              : -1;
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

        if (app_over_left_panel(app)) {
            hit = ui_hit_test(app->mouse_x, app->mouse_y);
            if (hit >= 0) {
                app->tool = hit;
            }
            break;
        }
        if (app_over_right_panel(app)) {
            break; /* properties are read-only for now */
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

    grid_draw(&app->gfx, &app->grid, &app->layout);
    if (app->left_open) {
        ui_draw_panel(&app->gfx, app->tool, app->hover_tool);
    }
    if (app->right_open) {
        props_draw(&app->gfx, &app->grid, &app->settings);
    }
    menu_draw(&app->gfx, &app->menu);

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
    settings_defaults(&app.settings);
    menu_init(&app.menu, &app.settings);

    app.running = 1;
    app.tool = TOOL_SELECT;
    app.hover_tool = -1;
    app.left_open = 1;
    app.right_open = 1;
    app.mouse_x = 0;
    app.mouse_y = 0;

    app_refresh_layout(&app);

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
