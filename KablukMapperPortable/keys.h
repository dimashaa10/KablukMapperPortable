/*
 * Kabluk Mapper Portable - keys.h
 *
 * One place for every key binding. On the desktop these are keyboard
 * stand-ins; when the PSP build appears only this file has to change
 * (PSP L / R / SELECT / cross / square).
 */
#ifndef KMP_KEYS_H
#define KMP_KEYS_H

#include <SDL.h>

#define KEY_LEFT SDLK_LEFT
#define KEY_RIGHT SDLK_RIGHT
#define KEY_UP SDLK_UP
#define KEY_DOWN SDLK_DOWN

#define KEY_CONFIRM SDLK_x /* PSP: cross  */
#define KEY_CANCEL SDLK_z  /* PSP: circle - also closes the lists */

#define KEY_PANEL_LEFT SDLK_q  /* PSP: L - fold the left tool panel    */
#define KEY_PANEL_RIGHT SDLK_e /* PSP: R - fold the right property panel */

#define KEY_OPEN_SETTINGS SDLK_RETURN /* PSP: SELECT - editor settings */

#define KEY_QUIT SDLK_ESCAPE

/* tools */
#define KEY_TOOL_1 SDLK_1
#define KEY_TOOL_2 SDLK_2
#define KEY_TOOL_3 SDLK_3
#define KEY_TOOL_4 SDLK_4
#define KEY_TOOL_NEXT SDLK_TAB

#endif /* KMP_KEYS_H */
