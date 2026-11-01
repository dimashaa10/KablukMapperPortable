/*
 * Kabluk Mapper Portable - props.h
 *
 * Right panel: properties of the map and of the selected entity.
 * The map block is fixed to 128x128x128 units, the map always starts with a
 * floor and a ceiling, so the numbers here are the rules the compiler will
 * check later.
 */
#ifndef KMP_PROPS_H
#define KMP_PROPS_H

#include "gfx.h"
#include "grid.h"
#include "settings.h"

void props_draw(Gfx *g, const Grid *grid, const Settings *settings);

#endif /* KMP_PROPS_H */
