/*
 * Kabluk Mapper Portable - values.h
 *
 * Per entity key/value pairs, the values the right panel lets you type.
 *
 * There is no dynamic allocation anywhere (PSP rule), so the store is a fixed
 * pool of entries. Every entry belongs to one entity: either a map cell that
 * holds a point entity, or worldspawn (the pseudo cell -1:-1, which is what
 * "nothing selected" means). Lookups are short linear scans - a map can have
 * at most a few hundred entities and the panel is the only heavy user.
 */
#ifndef KMP_VALUES_H
#define KMP_VALUES_H

#include "config.h"
#include "fgd.h"

/* one entry is ~90 bytes, so the whole pool stays under 50 KB */
#define VALUES_MAX 512

/* worldspawn is not a map cell */
#define VALUES_WORLD_CX (-1)
#define VALUES_WORLD_CY (-1)

typedef struct EntityValue {
    signed char cx; /* map cell, or -1 for worldspawn */
    signed char cy;
    unsigned char used;
    char key[FGD_NAME_LEN];
    char value[FGD_VALUE_LEN];
} EntityValue;

typedef struct ValueStore {
    EntityValue items[VALUES_MAX];
    int count; /* entries in use */
} ValueStore;

void values_init(ValueStore *store);

/* NULL when the entity does not override that key (the fgd default applies) */
const char *values_get(const ValueStore *store, int cx, int cy, const char *key);

/* Sets (or adds) one value. An empty value removes the override.
 * Returns 1 on success, 0 when the pool is full. */
int values_set(ValueStore *store, int cx, int cy, const char *key,
               const char *value);

/* Removes every value of one entity (used when it is erased or replaced). */
void values_clear_cell(ValueStore *store, int cx, int cy);

/* Enumeration for the exporter and the panel. */
int values_count_cell(const ValueStore *store, int cx, int cy);
const EntityValue *values_cell_at(const ValueStore *store, int cx, int cy,
                                  int nth);

#endif /* KMP_VALUES_H */
