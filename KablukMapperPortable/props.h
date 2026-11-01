/*
 * Kabluk Mapper Portable - props.h
 *
 * Right panel: the standard property list of an entity, straight from the
 * .fgd. Nothing selected means worldspawn, selecting a point entity with the
 * SELECT tool shows that entity's properties.
 */
#ifndef KMP_PROPS_H
#define KMP_PROPS_H

#include "fgd.h"
#include "gfx.h"
#include "grid.h"

typedef struct PropsView {
    int scroll;
} PropsView;

void props_view_init(PropsView *view);
void props_scroll_by(Fgd *fgd, PropsView *view, int delta);
void props_draw(Gfx *g, const Grid *grid, Fgd *fgd, PropsView *view);

#endif /* KMP_PROPS_H */
