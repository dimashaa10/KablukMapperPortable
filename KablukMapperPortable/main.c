/*
 * Kabluk Mapper Portable - main.c
 *
 * A very small Quake 1 map editor, written so it can be ported to the PSP
 * with pspsdk + SDL2 later:
 *   - fixed 480x272 resolution (PSP LCD), all layout in config.h
 *   - plain C, no WinAPI, no SDL_ttf, no dynamic memory
 *   - entity definitions come from Quake.fgd, textures from the .wad chosen
 *     in the editor settings
 *   - one cell is one 128x128x128 unit block; the exporter adds the floor
 *     and the ceiling (compile.c) and nothing else, so the grid starts empty
 *     and a map is as closed as the border the user painted
 *
 * The program opens on the start screen (menu.c, MENU_MAIN): CREATE MAP goes
 * to an empty grid, LOAD MAP reads a .map back through mapfile.c, QUIT leaves.
 *
 * Tools (left panel):
 *   SELECT   - click a point entity to show its properties on the right
 *   BLOCK    - place a block with the current texture, right click erases
 *   ENTITY   - pops the point entity list, then click places one
 *   TEXTURE  - pops the texture list of the selected wad
 *
 * Controls (desktop now, PSP buttons too - see keys.h and input.c):
 *   mouse / d-pad     move the grid cursor
 *   X / Z             confirm / cancel (cross / circle on the PSP)
 *   1..4 / TAB        pick a tool (TAB steps to the next one)
 *   Q / E             hide / show the tool / property panel (PSP: L / R)
 *   ENTER             editor settings (PSP: SELECT)
 *   F2                compile map window (PSP: START)
 *   ESC               back to the start screen (QUIT is there)
 *
 * A panel that is on screen owns the arrows and cross (panel_focus): showing
 * the tool panel makes the D-pad walk the four tools, showing the property
 * panel walks its rows, and hiding either one gives them back to the map.
 *
 * The start screen is not the only way out: the settings window has a
 * MAIN MENU button, and circle / Z on the start screen goes back to the map
 * that was being edited, so opening it never costs the work.
 */
#include <SDL.h>
#include <stdio.h>
#include <string.h>

/* "Quit" has to leave the game on the PSP so the XMB comes back; on the
 * desktop the main loop just stops. The module is only pulled in when a PSP
 * toolchain does the build, so nothing changes for the other targets. */
#if defined(__PSP__) || defined(_PSP) || defined(PSP)
#include <pspkernel.h>
#define KMP_HAVE_PSP 1
#endif

#include "compile.h"
#include "compileui.h"
#include "config.h"
#include "crash.h"
#include "prefs.h"
#include "fgd.h"
#include "gfx.h"
#include "grid.h"
#include "input.h"
#include "keyboard.h"
#include "keys.h"
#include "layout.h"
#include "mapfile.h"
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
    Input input; /* the pad, turned into keys and mouse moves */

    int running;
    int tool;
    int hover_tool;
    int left_open;
    int right_open;
    int panel_focus; /* FOCUS_*: what the arrows and cross act on */
    int tool_sel;    /* row the arrows sit on in the tool panel   */
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

/* What the settings file holds right now: the app compares against it once a
 * frame and writes only when something changed, so the floor and ceiling
 * textures (and the wad, the folder and the map name) are kept without every
 * place that can change them having to remember to save. */
static Settings settings_on_disk;

static void app_save_settings_if_changed(void)
{
    if (prefs_same(&app.settings, &settings_on_disk)) {
        return;
    }
    if (prefs_save(&app.settings)) {
        settings_on_disk = app.settings;
        SDL_Log("settings saved: %s", PREFS_FILE_NAME);
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
        SDL_Log("WAD loaded: %d textures, %s, palette %s", app.wad.count,
                wad_kind_text(&app.wad),
                app.wad.has_palette ? "found" : "missing (grey preview)");
        SDL_Log("WAD first texture: %s %dx%d", app.wad.names[0],
                (int)app.wad.tex_w[0], (int)app.wad.tex_h[0]);
    } else {
        app.wad.count = 0;
        app.current_tex = -1;
        if (app.settings.wad_path[0] != '\0') {
            SDL_Log("WAD not readable: %s", app.settings.wad_path);
        }
    }
    picker_close(&app.picker);
}

#ifdef KMP_HAVE_PSP
#include <unistd.h>

/* The PSP starts homebrew somewhere else than its own folder, and every path
 * in the editor (Quake.fgd, the .wad, maps/) is relative: move to the folder
 * EBOOT.PBP lives in, which the loader passes as argv[0]. */
static void app_chdir_to_eboot(const char *arg0)
{
    char dir[PATH_LEN];
    int i;
    int cut = -1;

    if (arg0 != NULL) {
        fs_copy(dir, PATH_LEN, arg0);
        for (i = 0; dir[i] != '\0'; i++) {
            if (dir[i] == '/' || dir[i] == '\\') {
                cut = i;
            }
        }
        if (cut > 0) {
            dir[cut] = '\0';
            if (chdir(dir) == 0) {
                return;
            }
        }
    }
    chdir(DEFAULT_PSP_DIR); /* the folder the readme tells people to use */
}
#endif

/* Leave the editor: XMB on the PSP, the desktop just closes. */
static void app_quit(void)
{
#ifdef KMP_HAVE_PSP
    sceKernelExitGame();
#endif
    app.running = 0;
}

static int app_file_exists(const char *path)
{
    FILE *file;

    if (path == NULL || path[0] == '\0') {
        return 0;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        return 0;
    }
    fclose(file);
    return 1;
}

/* "maps/myroom.map" -> "myroom" */
static void app_map_name_from_path(const char *path, char *out, int cap)
{
    const char *base = fs_basename(path);
    int n = 0;

    while (base[n] != '\0' && base[n] != '.' && n < cap - 1) {
        out[n] = base[n];
        n++;
    }
    out[n] = '\0';
}

/* The .map remembers its wad. Try it as written (that is how the editor saved
 * it), then next to the map, then one folder above the map: the compiled maps
 * live in their own folder but the wad usually does not. */
static void app_resolve_wad_path(const char *map_path, const char *wad, char *out,
                                 int cap)
{
    char dir[PATH_LEN];
    char candidate[PATH_LEN];

    fs_copy(out, cap, wad);
    if (wad[0] == '\0' || app_file_exists(wad)) {
        return;
    }
    fs_copy(dir, PATH_LEN, map_path);
    fs_parent(dir, PATH_LEN);
    fs_join(candidate, PATH_LEN, dir, wad);
    if (app_file_exists(candidate)) {
        fs_copy(out, cap, candidate);
        return;
    }
    fs_parent(dir, PATH_LEN);
    fs_join(candidate, PATH_LEN, dir, wad);
    if (app_file_exists(candidate)) {
        fs_copy(out, cap, candidate);
    }
}

/* Start screen: an empty project. */
static void app_new_map(void)
{
    if (compile_is_running(&app.job)) {
        menu_set_status(&app.menu, "WAIT FOR THE COMPILE TO FINISH");
        return;
    }
    grid_init(&app.grid);
    values_init(&app.values);
    props_view_init(&app.props);
    app_seed_defaults();
    app.panel_focus = FOCUS_GRID;
    app.tool = TOOL_SELECT;
    app.tool_sel = TOOL_SELECT;
    app.menu.resume_editor = 1;
    menu_close(&app.menu);
}

/* Start screen: a .map from disk. */
static void app_load_map(const char *path)
{
    static MapFile info;
    char wad[PATH_LEN];
    char line[MENU_STATUS_LEN];

    if (compile_is_running(&app.job)) {
        menu_set_status(&app.menu, "WAIT FOR THE COMPILE TO FINISH");
        return;
    }
    grid_init(&app.grid);
    values_init(&app.values);
    props_view_init(&app.props);
    app.panel_focus = FOCUS_GRID;
    app.tool_sel = app.tool;

    if (mapfile_peek_wad(path, wad, PATH_LEN)) {
        char resolved[PATH_LEN];

        app_resolve_wad_path(path, wad, resolved, PATH_LEN);
        if (resolved[0] != '\0' && strcmp(resolved, app.settings.wad_path) != 0) {
            fs_copy(app.settings.wad_path, PATH_LEN, resolved);
            app_reload_wad();
        }
    }

    if (!mapfile_load(path, &app.grid, &app.fgd, &app.values, &app.wad, &info)) {
        sprintf(line, "LOAD FAILED: %.32s",
                info.error[0] != '\0' ? info.error : "UNKNOWN");
        menu_set_status(&app.menu, line);
        app_seed_defaults(); /* the half read project is thrown away */
        return;
    }

    /* keep saving / compiling next to the file that was loaded */
    app_map_name_from_path(path, app.settings.map_name, FS_NAME_LEN);
    fs_copy(app.settings.out_dir, PATH_LEN, path);
    fs_parent(app.settings.out_dir, PATH_LEN);

    app.menu.resume_editor = 1;
    menu_close(&app.menu);
    SDL_Log("map loaded: %d brushes, %d cells, %d entities, %d textures missing",
            info.brushes, info.cells, info.entities, info.tex_missing);
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

/* qbsp, light and vis get the whole heap for a few seconds each, and every
 * preview texture (plus the picker thumbnails) is a few hundred KB the
 * compile does not need. They are rebuilt from the wad on demand, so they are
 * dropped before the tools start - that is memory the light stage can have. */
static void app_release_previews(void)
{
    textures_reset(&app.textures);
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
        /* no app_release_previews() here: writing a .map needs no memory, and
         * dropping every preview texture only makes the next frame rebuild all
         * of them from the wad. Only the tool chain gets that memory back. */
        crash_step("save");
        SDL_Log("saving the .map: %s", app.settings.out_dir);
        compile_start(&app.job, &app.grid, &app.fgd, &app.settings, &app.wad,
                      1);
        break;
    case MENU_REQUEST_NEW_MAP:
        app_new_map();
        break;
    case MENU_REQUEST_LOAD_MAP:
        app_load_map(app.menu.picked);
        break;
    case MENU_REQUEST_MAIN:
        /* the settings window asks for the start screen; the map stays as it
         * is, and circle/Z on that screen comes back to it */
        menu_open_main(&app.menu);
        app.menu.resume_editor = 1;
        break;
    case MENU_REQUEST_QUIT:
        app_quit();
        break;
    default:
        break;
    }
}

static void app_select_tool(int tool)
{
    app.tool = tool;
    app.tool_sel = tool;
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

/* Brings the tool panel on screen if it is hidden and hands it the pad. */
static void app_focus_tools(void)
{
    if (!app.left_open) {
        app.left_open = 1;
        app_refresh_layout();
    }
    app.panel_focus = FOCUS_TOOLS;
    app.tool_sel = app.tool; /* the arrows start on the tool in use */
}

/* The same for the property panel. */
static void app_focus_props(void)
{
    if (!app.right_open) {
        app.right_open = 1;
        app_refresh_layout();
    }
    app.panel_focus = FOCUS_PROPS;
    if (app.props.sel < 0 && app.fgd.prop_count > 0) {
        app.props.sel = 0;
    }
}

/* L: hide / show the tool panel. Showing it hands the pad to the four tools,
 * hiding it gives the pad back to the map, so the arrows always act on what is
 * on screen. Pressed while the panel is on screen but the map has the pad, it
 * takes the pad instead of hiding the panel. */
static void app_toggle_left_panel(void)
{
    if (app.left_open && app.panel_focus == FOCUS_TOOLS) {
        app.left_open = 0;
        app.panel_focus = FOCUS_GRID;
        app_refresh_layout();
        return;
    }
    app_focus_tools();
}

/* R: the same for the property panel. */
static void app_toggle_right_panel(void)
{
    if (app.right_open && app.panel_focus == FOCUS_PROPS) {
        app.right_open = 0;
        app.panel_focus = FOCUS_GRID;
        app_refresh_layout();
        return;
    }
    app_focus_props();
}

/* arrows / X while the tool palette has the focus: they walk the four tool
 * buttons, cross picks the highlighted one. The tool itself only changes on
 * cross, so stepping over ENTITY or TEXTURE does not open their list. */
static void app_tools_key(SDL_Keycode sym)
{
    if (sym == KEY_CANCEL) {
        app_toggle_left_panel(); /* circle as well: hide it, the map gets the pad */
        return;
    }
    if (sym == KEY_DOWN) {
        app.tool_sel = (app.tool_sel + 1) % PANEL_BUTTONS;
    } else if (sym == KEY_UP) {
        app.tool_sel = (app.tool_sel + PANEL_BUTTONS - 1) % PANEL_BUTTONS;
    } else if (sym == KEY_CONFIRM) {
        app_select_tool(app.tool_sel);
    }
}

/* arrows / X while the right panel has the focus */
static void app_props_key(SDL_Keycode sym)
{
    int count = app.fgd.prop_count;

    if (sym == KEY_CANCEL) {
        app_toggle_right_panel(); /* circle: hide it, back to the map */
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
    SDL_Keycode sym = key->keysym.sym;

    /* these work whatever has the focus: the windows, the two panels, the
     * tools themselves */
    switch (sym) {
    case KEY_QUIT:
        menu_open_main(&app.menu); /* Quit lives on the start screen */
        return;

    case KEY_OPEN_SETTINGS:
        menu_open_settings(&app.menu);
        return;

    case KEY_OPEN_COMPILE:
        compileui_open(&app.compile_ui);
        return;

    case KEY_PANEL_LEFT:
        app_toggle_left_panel();
        return;

    case KEY_PANEL_RIGHT:
        app_toggle_right_panel();
        return;

    case KEY_PROP_FOCUS:
        app_focus_props();
        return;

    case KEY_TOOL_1:
        app_select_tool(TOOL_SELECT);
        return;
    case KEY_TOOL_2:
        app_select_tool(TOOL_BLOCK);
        return;
    case KEY_TOOL_3:
        app_select_tool(TOOL_ENTITY);
        return;
    case KEY_TOOL_4:
        app_select_tool(TOOL_TEXTURE);
        return;
    case KEY_TOOL_NEXT:
        app_select_tool((app.tool + 1) % PANEL_BUTTONS);
        return;

    default:
        break;
    }

    if (app.panel_focus == FOCUS_TOOLS) {
        app_tools_key(sym);
        return;
    }
    if (app.panel_focus == FOCUS_PROPS) {
        app_props_key(sym);
        return;
    }

    switch (sym) {
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
            app_release_previews();
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

            app.panel_focus = FOCUS_PROPS;
            if (row >= 0) {
                app.props.sel = row;
                app_edit_property(row);
            }
            break;
        }
        app.panel_focus = FOCUS_GRID;

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
        int focused = app.panel_focus == FOCUS_TOOLS;

        ui_draw_panel(&app.gfx, app.tool,
                      focused ? app.tool_sel : app.hover_tool, focused);
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
    int partition_kb;

#ifdef KMP_HAVE_PSP
    app_chdir_to_eboot(argc > 0 ? argv[0] : NULL);
    /* Kabluk Mapper: pspdev hands malloc one block of the user partition,
     * sized from what is free the first time anything allocates and leaving a
     * fixed 512 KB reserve behind. So this is the whole budget the editor and
     * the tool chain share, and it has to be read before SDL_Init() takes its
     * half - afterwards the number is the reserve and never moves again, which
     * is why the tool logs look frozen at 512 KB. */
    partition_kb = (int)(sceKernelMaxFreeMemSize() / 1024);
#else
    (void)argc;
    (void)argv;
    partition_kb = -1;
#endif

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        SDL_Log("SDL init failed: %s", SDL_GetError());
        return 1;
    }
    if (partition_kb >= 0) {
        SDL_Log("user memory before startup: %d KB", partition_kb);
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
    input_init(&app.input);
    grid_init(&app.grid);
    settings_defaults(&app.settings);
    {
        int loaded = prefs_load(&app.settings);

        SDL_Log(loaded > 0 ? "settings loaded from %s (%d)" : "no settings file (%s)",
                PREFS_FILE_NAME, loaded);
    }
    settings_on_disk = app.settings;
    menu_init(&app.menu, &app.settings, &app.job);
    picker_init(&app.picker);
    props_view_init(&app.props);
    values_init(&app.values);
    keyboard_init(&app.kbd);
    app_seed_defaults();
    compile_init(&app.job);
    compileui_init(&app.compile_ui);
    menu_open_main(&app.menu); /* the editor starts from the start screen */

    app.running = 1;
    app.tool = TOOL_SELECT;
    app.hover_tool = -1;
    app.left_open = 1;
    app.right_open = 1;
    app.panel_focus = FOCUS_GRID;
    app.tool_sel = TOOL_SELECT;
    app.mouse_x = 0;
    app.mouse_y = 0;
    app.current_tex = -1;
    app.entity_class = -1;
    app.loaded_wad[0] = '\0';

    SDL_Log("phase log: %s", crash_init() ? "kabluk-phase.txt" : "not writable");
    crash_step("startup");

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
            if (!compile_is_running(&app.job)) {
                /* saved or compiled: the editor is what runs from here */
                crash_step("editor");
            }
        }

        while (SDL_PollEvent(&event)) {
            app_handle_event(&event);
        }
        app_save_settings_if_changed();

        /* The PSP pad (and any gamepad on the desktop) is polled: the events
         * it produces walk through the very same handler as real input. */
        {
            SDL_Event pad_events[INPUT_EVENT_MAX];
            int pad_count = input_pump(&app.input, pad_events,
                                       (int)(sizeof(pad_events) /
                                             sizeof(pad_events[0])));
            int pad_index;

            for (pad_index = 0; pad_index < pad_count; pad_index++) {
                app_handle_event(&pad_events[pad_index]);
            }
        }

        app_render();

        /* ~60 fps cap: SDL_Delay keeps the CPU idle (and the PSP happy) */
        frame_time = SDL_GetTicks() - frame_start;
        if (frame_time < FRAME_MS) {
            SDL_Delay(FRAME_MS - frame_time);
        }
    }

    input_shutdown(&app.input);
    textures_reset(&app.textures);
    gfx_shutdown(&app.gfx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
