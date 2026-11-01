/*
 * Kabluk Mapper Portable - main.c
 *
 * A very small Quake 1 map editor, written so it can be ported to the PSP
 * with pspsdk + SDL2 later:
 *   - fixed 480x272 resolution (PSP LCD), all layout in config.h
 *   - plain C, no WinAPI, no SDL_ttf, no dynamic memory
 *   - entity definitions come from Quake.fgd, textures from the .wad chosen
 *     in the editor settings
 *   - one cell is one 128x128x128 unit block; the exporter adds the floor,
 *     the ceiling and the walls that seal the map (compile.c), so the grid
 *     starts empty and nothing can leak into the void
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
 *   F2                compile map window (PSP: START)
 *   ESC               quit
 */
#include <SDL.h>
#include <stdio.h>

#include "compile.h"
#include "compileui.h"
#include "config.h"
#include "fgd.h"
#include "gfx.h"
#include "grid.h"
#include "keyboard.h"
#include "keys.h"
#include "layout.h"
#include "menu.h"
#include "picker.h"
#include "props.h"
#include "settings.h"
#include "textures.h"
#include "values.h"
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
    ValueStore values;
    Keyboard kbd;
    CompileJob job;
    CompileUi compile_ui;

    int running;
    int tool;
    int hover_tool;
    int left_open;
    int right_open;
    int panel_focus; /* arrows + X work on the property list */
    int mouse_x;
    int mouse_y;

    int current_tex;   /* texture used for new blocks, -1 = none */
    int entity_class;  /* last point entity class picked, -1 = none */
    char loaded_wad[PATH_LEN];
} App;

/* The editor state is a few hundred kilobytes (fgd text, tex cache), so it
 * lives in static memory instead of the stack - PSP stacks are small. */
static App app;

/* who asked for the on screen keyboard */
#define KBD_FOR_PROPERTY 0
#define KBD_FOR_MAP_NAME 1
#define KBD_FOR_FOLDER 2

static int kbd_for = KBD_FOR_PROPERTY;

/* Values the exporter would use anyway are put into the store, so the right
 * panel shows them and they can be edited like everything else. */
static void app_seed_defaults(void)
{
    char text[16];

    sprintf(text, "%d", DEFAULT_MINLIGHT);
    values_set(&app.values, VALUES_WORLD_CX, VALUES_WORLD_CY, "light", text);
    if (DEFAULT_MAP_MESSAGE[0] != '\0') {
        values_set(&app.values, VALUES_WORLD_CX, VALUES_WORLD_CY, "message",
                   DEFAULT_MAP_MESSAGE);
    }
}

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

/* index of a texture name in the loaded wad, -1 when it is not there */
static int app_tex_index(const char *name)
{
    int i;

    if (name == NULL || name[0] == '\0') {
        return -1;
    }
    for (i = 0; i < app.wad.count && i < WAD_MAX_TEX; i++) {
        const char *tex = app.wad.names[i];
        int c = 0;

        while (tex[c] != '\0' && name[c] != '\0' &&
               (tex[c] == name[c] ||
                (tex[c] | 0x20) == (name[c] | 0x20))) {
            c++;
        }
        if (tex[c] == '\0' && name[c] == '\0') {
            return i;
        }
    }
    return -1;
}

/* The menu asked for a keyboard, the texture list, or a save. */
static void app_handle_menu_request(void)
{
    int request = app.menu.request;

    if (request == MENU_REQUEST_NONE) {
        return;
    }
    app.menu.request = MENU_REQUEST_NONE;

    switch (request) {
    case MENU_REQUEST_NAME:
        kbd_for = KBD_FOR_MAP_NAME;
        keyboard_open(&app.kbd, "MAP NAME", app.settings.map_name, 0, -1, -1);
        break;
    case MENU_REQUEST_FOLDER:
        kbd_for = KBD_FOR_FOLDER;
        keyboard_open(&app.kbd, "FOLDER NAME", "", 0, -1, -1);
        break;
    case MENU_REQUEST_FLOOR:
    case MENU_REQUEST_CEILING:
        if (app.wad.count <= 0) {
            SDL_Log("No wad loaded: pick one in the settings first");
            break;
        }
        app.picker.target = request == MENU_REQUEST_FLOOR ? PICK_TARGET_FLOOR
                                                          : PICK_TARGET_CEILING;
        picker_open_textures(&app.picker, &app.wad,
                             app_tex_index(request == MENU_REQUEST_FLOOR
                                               ? app.settings.floor_tex
                                               : app.settings.ceiling_tex));
        break;
    case MENU_REQUEST_SAVE:
        if (compile_is_running(&app.job)) {
            break;
        }
        compile_start(&app.job, &app.grid, &app.fgd, &app.settings, &app.wad,
                      1);
        break;
    default:
        break;
    }
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

/* Opens the on screen keyboard for one property of the shown entity. */
static void app_edit_property(int index)
{
    const FgdProp *prop;
    const char *value;
    int cx;
    int cy;

    if (!app.fgd.loaded || index < 0 || index >= app.fgd.prop_count) {
        return;
    }
    prop = &app.fgd.props[index];
    props_target_cell(&app.grid, &cx, &cy);
    value = props_value(prop, &app.values, cx, cy);
    app.props.sel = index;
    kbd_for = KBD_FOR_PROPERTY;
    keyboard_open(&app.kbd, prop->key, value, props_is_numeric(prop), cx, cy);
}

static void app_handle_keyboard_event(const SDL_Event *event)
{
    int result = keyboard_handle_event(&app.kbd, event);

    if (result == KBD_APPLY) {
        if (kbd_for == KBD_FOR_MAP_NAME) {
            menu_set_map_name(&app.menu, app.kbd.value);
        } else if (kbd_for == KBD_FOR_FOLDER) {
            menu_create_folder(&app.menu, app.kbd.value);
        } else {
            values_set(&app.values, app.kbd.cx, app.kbd.cy, app.kbd.key,
                       app.kbd.value);
        }
        keyboard_close(&app.kbd);
    } else if (result == KBD_CANCEL) {
        keyboard_close(&app.kbd);
    }
}

/* arrows / X while the right panel has the focus */
static void app_panel_key(SDL_Keycode sym)
{
    int count = app.fgd.prop_count;

    if (sym == KEY_CANCEL) {
        app.panel_focus = 0;
        return;
    }
    if (count <= 0) {
        return;
    }
    if (app.props.sel < 0) {
        app.props.sel = 0;
    }
    if (sym == KEY_DOWN) {
        if (app.props.sel < count - 1) {
            app.props.sel++;
        }
    } else if (sym == KEY_UP) {
        if (app.props.sel > 0) {
            app.props.sel--;
        }
    } else if (sym == KEY_CONFIRM) {
        app_edit_property(app.props.sel);
        return;
    }
    /* keep the highlighted row on screen */
    if (app.props.sel < app.props.scroll) {
        app.props.scroll = app.props.sel;
    }
    if (app.props.sel >= app.props.scroll + props_visible_rows()) {
        app.props.scroll = app.props.sel - props_visible_rows() + 1;
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
            /* another class in this cell means the values typed for the old
             * one no longer apply */
            if (grid_entity_at(&app.grid, cx, cy) != app.entity_class) {
                values_clear_cell(&app.values, cx, cy);
            }
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
        values_clear_cell(&app.values, cx, cy);
        break;
    default:
        grid_erase(&app.grid, cx, cy);
        break;
    }
}

static void app_handle_key(const SDL_KeyboardEvent *key)
{
    if (app.panel_focus) {
        app_panel_key(key->keysym.sym);
        return;
    }

    switch (key->keysym.sym) {
    case KEY_QUIT:
        app.running = 0;
        break;

    case KEY_OPEN_SETTINGS:
        menu_open_settings(&app.menu);
        break;

    case KEY_OPEN_COMPILE:
        compileui_open(&app.compile_ui);
        break;

    case KEY_PANEL_LEFT:
        app.left_open = !app.left_open;
        app_refresh_layout();
        break;

    case KEY_PANEL_RIGHT:
        app.right_open = !app.right_open;
        app.panel_focus = 0;
        app_refresh_layout();
        break;

    case KEY_PROP_FOCUS:
        if (!app.right_open) {
            app.right_open = 1;
            app_refresh_layout();
        }
        app.panel_focus = 1;
        if (app.props.sel < 0 && app.fgd.prop_count > 0) {
            app.props.sel = 0;
        }
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
        app.picker.target = PICK_TARGET_BLOCK;
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
    } else if (app.picker.target == PICK_TARGET_FLOOR ||
               app.picker.target == PICK_TARGET_CEILING) {
        /* the settings window is still open under the list */
        if (app.picker.count > 0) {
            menu_set_texture(&app.menu, app.picker.target == PICK_TARGET_CEILING,
                             app.wad.names[app.picker.cur]);
        }
        app.picker.target = PICK_TARGET_BLOCK;
        picker_close(&app.picker);
        return;
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

    /* the keyboard sits on top of every other modal screen */
    if (keyboard_is_open(&app.kbd)) {
        app_handle_keyboard_event(event);
        return;
    }

    /* modal screens take everything while they are open; the list can sit on
     * top of the settings window, so it is asked first */
    if (picker_is_open(&app.picker)) {
        app_handle_picker_result(picker_handle_event(&app.picker, event));
        return;
    }
    if (menu_is_open(&app.menu)) {
        menu_handle_event(&app.menu, event);
        app_handle_menu_request();
        return;
    }
    if (compileui_is_open(&app.compile_ui)) {
        int action = compileui_handle_event(&app.compile_ui, event, &app.job);

        if (action == COMPILEUI_COMPILE) {
            compile_start(&app.job, &app.grid, &app.fgd, &app.settings,
                          &app.wad, 0);
        } else if (action == COMPILEUI_BROWSE) {
            menu_open_browse(&app.menu, BROWSE_DIR, MENU_CLOSED);
        } else if (action == COMPILEUI_CLOSE) {
            compileui_close(&app.compile_ui);
        }
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
        app.props.hover = app_over_right_panel()
                              ? props_hit_row(&app.fgd, &app.props, app.mouse_x,
                                              app.mouse_y)
                              : -1;
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
            int row = props_hit_row(&app.fgd, &app.props, app.mouse_x,
                                    app.mouse_y);

            app.panel_focus = 1;
            if (row >= 0) {
                app.props.sel = row;
                app_edit_property(row);
            }
            break;
        }
        app.panel_focus = 0;

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
        props_draw(&app.gfx, &app.grid, &app.fgd, &app.props, &app.values);
    }
    compileui_draw(&app.gfx, &app.compile_ui, &app.job, &app.settings);
    menu_draw(&app.gfx, &app.menu);
    /* the texture / entity list can sit on top of the settings window */
    picker_draw(&app.gfx, &app.picker, &app.fgd, &app.textures, &app.wad);
    keyboard_draw(&app.gfx, &app.kbd);

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
    menu_init(&app.menu, &app.settings, &app.job);
    picker_init(&app.picker);
    props_view_init(&app.props);
    values_init(&app.values);
    keyboard_init(&app.kbd);
    app_seed_defaults();
    compile_init(&app.job);
    compileui_init(&app.compile_ui);

    app.running = 1;
    app.tool = TOOL_SELECT;
    app.hover_tool = -1;
    app.left_open = 1;
    app.right_open = 1;
    app.panel_focus = 0;
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

        /* the compile job runs a small chunk per frame */
        if (compile_is_running(&app.job)) {
            compile_step(&app.job, &app.grid, &app.fgd, &app.settings, &app.wad,
                         &app.values);
        }

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
