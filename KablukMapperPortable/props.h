/*
 * Kabluk Mapper Portable - props.h
 *
 * Right panel: the standard property list of an entity, straight from the
 * .fgd. Nothing selected means worldspawn, selecting a point entity with the
 * SELECT tool shows that entity's properties.
 *
 * Clicking a property (or pressing X with the panel focused) opens the on
 * screen keyboard for it: the numpad when the fgd type is a number, the full
 * keyboard for everything else.
 */
#ifndef KMP_PROPS_H
#define KMP_PROPS_H

#include "fgd.h"
#include "gfx.h"
#include "grid.h"
#include "values.h"

typedef struct PropsView {
    int scroll;
    int sel;   /* property index being edited / highlighted, -1 = none */
    int hover; /* property index under the mouse, -1 = none */
} PropsView;

void props_view_init(PropsView *view);

/* Number of property rows that fit in the panel. */
int props_visible_rows(void);

void props_scroll_by(Fgd *fgd, PropsView *view, int delta);

/* Property index under the mouse, -1 when the point is not on a row. */
int props_hit_row(const Fgd *fgd, const PropsView *view, int mx, int my);

/* Where the values of the shown class live: a map cell, or -1:-1 for
 * worldspawn. */
void props_target_cell(const Grid *grid, int *cx, int *cy);

/* 1 when the fgd type only holds a number (numpad layout). */
int props_is_numeric(const FgdProp *prop);

/* Current value of a property: the entity's own value if it has one, else the
 * fgd default (which may be empty). NULL when neither exists. */
const char *props_value(const FgdProp *prop, const ValueStore *values, int cx,
                        int cy);

void props_draw(Gfx *g, const Grid *grid, Fgd *fgd, PropsView *view,
                const ValueStore *values);

#endif /* KMP_PROPS_H */
