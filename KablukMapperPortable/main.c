/*
 * Kabluk Mapper Portable - main.c
 *
 * A very small Quake 1 map editor, written so it can be ported to the PSP
 * with pspsdk + SDL2 later:
 *   - fixed 480x272 resolution (PSP LCD), all layout in config.h
 *   - plain C, no WinAPI, no SDL_ttf, no dynamic memory
 *   - entity definitions come from Quake.fgd, textures from the .wad chosen
 *     in the editor settings
 *   - one cell is one 128x128x128 unit block and the map always keeps its
 *     floor/ceiling shell, so nothing can leak into the void
 *
 * Tools (left panel):
 *   SELECT   - click a point entity to show its properties on the right
 *   BLOCK    - place a block with the current texture, right click erases
 *   ENTITY   - pops the point entity list, then click places one
 *   TEXTURE  - pops the texture list of the selected wad
 *
 * Controls (desktop now, PSP buttons later - see keys.h):
 *   mouse / d-pad     move the grid cursor
 *   X / Z             confirm / cancel (cross / circle on the PSP)
 *   1..4 / TAB        pick a tool
 *   Q / E             fold the left / right panel (PSP: L / R)
 *   ENTER             editor settings (PSP: SELECT)
 *   ESC               quit
 */
#include <SDL.h>

#include "config.h"
#include "fgd.h"
#include "gfx.h"
#include "grid.h"
#include "keys.h"
#include "layout.h"
#include "menu.h"
#include "picker.h"
#include "props.h"
#include "settings.h"
#include "textures.h"
#include "ui.h"
#include "wad.h"

typedef struct App {
    SDL_Window *window;
    Gfx gfx;
    Grid grid;
    Layout layout;
    Settings settings;
    Menu menu;
    Fgd fgd;
    Wad wad;
    Textures textures;
    Picker picker;
    PropsView props;

    int running;
    int tool;
    int hover_tool;
    int left_open;
    int right_open;
    int mouse_x;
    int mouse_y;

    int current_tex;   /* texture used for new blocks, -1 = none */
    int entity_class;  /* last point entity class picked, -1 = none */
    char loaded_wad[PATH_LEN];
} App;

/* The editor state is a few hundred kilobytes (fgd text, tex cache), so it
 * lives in static memory instead of the stack - PSP stacks are small. */
static App app;

static void app_refresh_layout(void)
{
    layout_compute(&app.layout, app.left_open, app.right_open);
    grid_clamp_camera(&app.grid, &app.layout);
}

/* Mouse position in window pixels -> 480x272 logical pixels. */
static void app_update_mouse(void)
{
    int mx = app.mouse_x;
    int my = app.mouse_y;
    int ww = 0;
    int wh = 0;

    SDL_GetWindowSize(app.window, &ww, &wh);
    if (ww > 0 && wh > 0 && (ww != SCREEN_W || wh != SCREEN_H)) {
        mx = mx * SCREEN_W / ww;
        my = my * SCREEN_H / wh;
    }
    app.mouse_x = mx;
    app.mouse_y = my;
}

static int app_over_left_panel(void)
{
    return app.left_open && app.mouse_x < PANEL_W;
}

static int app_over_right_panel(void)
{
    return app.right_open && app.mouse_x >= SCREEN_W - RIGHT_PANEL_W;
}

/* Reloads the wad + texture cache when the path changed in the settings. */
static void app_reload_wad(void)
{
    int i;

    for (i = 0; i < PATH_LEN; i++) {
        if (app.settings.wad_path[i] != app.loaded_wad[i]) {
            break;
        }
        if (app.settings.wad_path[i] == '\0') {
            return;
        }
    }
    if (i == PATH_LEN) {
        return;
    }

    for (i = 0; i < PATH_LEN; i++) {
        app.loaded_wad[i] = app.settings.wad_path[i];
    }
    textures_reset(&app.textures);
    if (wad_load(&app.wad, app.settings.wad_path) > 0) {
        app.current_tex = 0;
        SDL_Log("WAD loaded: %d textures, palette %s", app.wad.count,
                app.wad.has_palette ? "found" : "missing (grey preview)");
    } else {
        app.wad.count = 0;
        app.current_tex = -1;
        if (app.settings.wad_path[0] != '\0') {
            SDL_Log("WAD not readable: %s", app.settings.wad_path);
        }
    }
    picker_close(&app.picker);
}

static void app_select_tool(int tool)
{
    app.tool = tool;
    if (tool == TOOL_ENTITY) {
        picker_open_entities(&app.picker, &app.fgd, app.entity_class);
    } else if (tool == TOOL_TEXTURE) {
        picker_open_textures(&app.picker, &app.wad, app.current_tex);
    }
}

static void app_move_cursor_to_mouse(void)
{
    int cx;
    int cy;

    if (grid_screen_to_cell(&app.grid, &app.layout, app.mouse_x, app.mouse_y, &cx,
                            &cy)) {
        app.grid.cursor_x = cx;
        app.grid.cursor_y = cy;
        grid_keep_cursor_visible(&app.grid, &app.layout);
    }
}

/* X / left click */
static void app_primary_action(void)
{
    int cx = app.grid.cursor_x;
    int cy = app.grid.cursor_y;

    switch (app.tool) {
    case TOOL_SELECT:
        grid_select(&app.grid, cx, cy);
        break;
    case TOOL_ENTITY:
        if (app.entity_class >= 0) {
            grid_set_entity(&app.grid, cx, cy, app.entity_class);
            grid_select(&app.grid, cx, cy);
        }
        break;
    default: /* BLOCK and TEXTURE tools place blocks */
        grid_paint(&app.grid, cx, cy, app.current_tex);
        break;
    }
}

/* Z / right click */
static void app_secondary_action(void)
{
    int cx = app.grid.cursor_x;
    int cy = app.grid.cursor_y;

    switch (app.tool) {
    case TOOL_SELECT:
        grid_select_none(&app.grid);
        break;
    case TOOL_ENTITY:
        grid_set_entity(&app.grid, cx, cy, -1);
        break;
    default:
        grid_erase(&app.grid, cx, cy);
        break;
    }
}

static void app_handle_key(const SDL_KeyboardEvent *key)
{
    switch (key->keysym.sym) {
    case KEY_QUIT:
        app.running = 0;
        break;

    case KEY_OPEN_SETTINGS:
        menu_open_settings(&app.menu);
        break;

    case KEY_PANEL_LEFT:
        app.left_open = !app.left_open;
        app_refresh_layout();
        break;

    case KEY_PANEL_RIGHT:
        app.right_open = !app.right_open;
        app_refresh_layout();
        break;

    case KEY_LEFT:
        grid_move_cursor(&app.grid, &app.layout, -1, 0);
        break;
    case KEY_RIGHT:
        grid_move_cursor(&app.grid, &app.layout, 1, 0);
        break;
    case KEY_UP:
        grid_move_cursor(&app.grid, &app.layout, 0, -1);
        break;
    case KEY_DOWN:
        grid_move_cursor(&app.grid, &app.layout, 0, 1);
        break;

    case KEY_CONFIRM:
        app_primary_action();
        break;
    case KEY_CANCEL:
        app_secondary_action();
        break;

    case KEY_TOOL_1:
        app_select_tool(TOOL_SELECT);
        break;
    case KEY_TOOL_2:
        app_select_tool(TOOL_BLOCK);
        break;
    case KEY_TOOL_3:
        app_select_tool(TOOL_ENTITY);
        break;
    case KEY_TOOL_4:
        app_select_tool(TOOL_TEXTURE);
        break;
    case KEY_TOOL_NEXT:
        app_select_tool((app.tool + 1) % PANEL_BUTTONS);
        break;

    default:
        break;
    }
}

static void app_handle_picker_result(int result)
{
    if (result == PICKER_EVENT_CANCEL) {
        picker_close(&app.picker);
        return;
    }
    if (result != PICKER_EVENT_CONFIRM) {
        return;
    }
    if (app.picker.kind == PICK_ENTITY) {
        if (app.picker.count > 0) {
            app.entity_class = app.picker.index[app.picker.cur];
        }
        app.tool = TOOL_ENTITY;
    } else {
        if (app.picker.count > 0) {
            app.current_tex = app.picker.cur;
        }
        app.tool = TOOL_BLOCK; /* a picked texture is used right away */
    }
    picker_close(&app.picker);
}

static void app_handle_event(const SDL_Event *event)
{
    int hit;

    if (event->type == SDL_QUIT) {
        app.running = 0;
        return;
    }

    /* modal screens take everything while they are open */
    if (menu_is_open(&app.menu)) {
        menu_handle_event(&app.menu, event);
        return;
    }
    if (picker_is_open(&app.picker)) {
        app_handle_picker_result(picker_handle_event(&app.picker, event));
        return;
    }

    switch (event->type) {
    case SDL_KEYDOWN:
        app_handle_key(&event->key);
        break;

    case SDL_MOUSEMOTION:
        app.mouse_x = event->motion.x;
        app.mouse_y = event->motion.y;
        app_update_mouse();
        app.hover_tool =
            app_over_left_panel() ? ui_hit_test(app.mouse_x, app.mouse_y) : -1;
        app_move_cursor_to_mouse();
        /* dragging while a button is held paints continuously */
        if ((event->motion.state & SDL_BUTTON_LMASK) != 0) {
            if (app.tool == TOOL_BLOCK || app.tool == TOOL_TEXTURE) {
                app_primary_action();
            }
        } else if ((event->motion.state & SDL_BUTTON_RMASK) != 0) {
            if (app.tool == TOOL_BLOCK || app.tool == TOOL_TEXTURE) {
                app_secondary_action();
            }
        }
        break;

    case SDL_MOUSEBUTTONDOWN:
        app.mouse_x = event->button.x;
        app.mouse_y = event->button.y;
        app_update_mouse();

        if (app_over_left_panel()) {
            hit = ui_hit_test(app.mouse_x, app.mouse_y);
            if (hit >= 0) {
                app_select_tool(hit);
            }
            break;
        }
        if (app_over_right_panel()) {
            break; /* properties are read-only for now */
        }

        app_move_cursor_to_mouse();
        if (event->button.button == SDL_BUTTON_LEFT) {
            app_primary_action();
        } else if (event->button.button == SDL_BUTTON_RIGHT) {
            app_secondary_action();
        }
        break;

    case SDL_MOUSEWHEEL:
        if (app_over_right_panel()) {
            props_scroll_by(&app.fgd, &app.props, event->wheel.y > 0 ? -1 : 1);
        }
        break;

    default:
        break;
    }
}

static void app_render(void)
{
    gfx_frame_begin(&app.gfx, COL_BG);

    grid_draw(&app.gfx, &app.grid, &app.layout, &app.textures, &app.wad);
    if (app.left_open) {
        ui_draw_panel(&app.gfx, app.tool, app.hover_tool);
    }
    if (app.right_open) {
        props_draw(&app.gfx, &app.grid, &app.fgd, &app.props);
    }
    picker_draw(&app.gfx, &app.picker, &app.fgd, &app.textures, &app.wad);
    menu_draw(&app.gfx, &app.menu);

    gfx_frame_end(&app.gfx);
}

int main(int argc, char *argv[])
{
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
    picker_init(&app.picker);
    props_view_init(&app.props);

    app.running = 1;
    app.tool = TOOL_SELECT;
    app.hover_tool = -1;
    app.left_open = 1;
    app.right_open = 1;
    app.mouse_x = 0;
    app.mouse_y = 0;
    app.current_tex = -1;
    app.entity_class = -1;
    app.loaded_wad[0] = '\0';

    if (fgd_load(&app.fgd, NULL)) {
        SDL_Log("FGD loaded: %d classes", fgd_count(&app.fgd));
    } else {
        SDL_Log("FGD not found (%s) - put it next to the executable",
                FGD_FILE_NAME);
    }

    app_refresh_layout();

    while (app.running) {
        frame_start = SDL_GetTicks();

        app_reload_wad();

        while (SDL_PollEvent(&event)) {
            app_handle_event(&event);
        }

        app_render();

        /* ~60 fps cap: SDL_Delay keeps the CPU idle (and the PSP happy) */
        frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_MS) {
            SDL_Delay(FRAME_MS - frame_time);
        }
    }

    textures_reset(&app.textures);
    gfx_shutdown(&app.gfx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
