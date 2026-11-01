/*
 * Kabluk Mapper Portable - values.c
 *
 * Fixed pool of entity key/value pairs (see values.h).
 */
#include "values.h"

#include <string.h>

#include "config.h"

static void value_copy(char *dst, int cap, const char *src)
{
    int i;

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    for (i = 0; i < cap - 1 && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

static int value_same_cell(const EntityValue *item, int cx, int cy)
{
    return item->used && (int)item->cx == cx && (int)item->cy == cy;
}

static int value_same_key(const char *a, const char *b)
{
    int i;

    for (i = 0; a[i] != '\0' && b[i] != '\0'; i++) {
        char ca = a[i];
        char cb = b[i];

        if (ca >= 'A' && ca <= 'Z') {
            ca = (char)(ca - 'A' + 'a');
        }
        if (cb >= 'A' && cb <= 'Z') {
            cb = (char)(cb - 'A' + 'a');
        }
        if (ca != cb) {
            return 0;
        }
    }
    return a[i] == b[i];
}

/* the slot of one key, NULL when it is not set */
static EntityValue *value_find(ValueStore *store, int cx, int cy,
                               const char *key)
{
    int i;

    if (key == NULL || key[0] == '\0') {
        return NULL;
    }
    for (i = 0; i < VALUES_MAX; i++) {
        EntityValue *item = &store->items[i];

        if (value_same_cell(item, cx, cy) && value_same_key(item->key, key)) {
            return item;
        }
    }
    return NULL;
}

static EntityValue *value_new(ValueStore *store)
{
    int i;

    for (i = 0; i < VALUES_MAX; i++) {
        if (!store->items[i].used) {
            store->items[i].used = 1;
            store->items[i].cx = (signed char)VALUES_WORLD_CX;
            store->items[i].cy = (signed char)VALUES_WORLD_CY;
            store->items[i].key[0] = '\0';
            store->items[i].value[0] = '\0';
            store->count++;
            return &store->items[i];
        }
    }
    return NULL;
}

void values_init(ValueStore *store)
{
    memset(store, 0, sizeof(*store));
}

const char *values_get(const ValueStore *store, int cx, int cy, const char *key)
{
    int i;

    if (key == NULL || key[0] == '\0') {
        return NULL;
    }
    for (i = 0; i < VALUES_MAX; i++) {
        const EntityValue *item = &store->items[i];

        if (value_same_cell(item, cx, cy) && value_same_key(item->key, key) &&
            item->value[0] != '\0') {
            return item->value;
        }
    }
    return NULL;
}

int values_set(ValueStore *store, int cx, int cy, const char *key,
               const char *value)
{
    EntityValue *item;

    if (key == NULL || key[0] == '\0') {
        return 0;
    }
    item = value_find(store, cx, cy, key);
    if (value == NULL || value[0] == '\0') {
        /* an emptied value means "use the fgd default again" */
        if (item != NULL) {
            item->used = 0;
            item->key[0] = '\0';
            item->value[0] = '\0';
            if (store->count > 0) {
                store->count--;
            }
        }
        return 1;
    }
    if (item == NULL) {
        item = value_new(store);
        if (item == NULL) {
            return 0; /* pool full */
        }
        item->cx = (signed char)cx;
        item->cy = (signed char)cy;
        value_copy(item->key, FGD_NAME_LEN, key);
    }
    value_copy(item->value, FGD_VALUE_LEN, value);
    return 1;
}

void values_clear_cell(ValueStore *store, int cx, int cy)
{
    int i;

    for (i = 0; i < VALUES_MAX; i++) {
        EntityValue *item = &store->items[i];

        if (value_same_cell(item, cx, cy)) {
            item->used = 0;
            item->key[0] = '\0';
            item->value[0] = '\0';
            if (store->count > 0) {
                store->count--;
            }
        }
    }
}

int values_count_cell(const ValueStore *store, int cx, int cy)
{
    int i;
    int n = 0;

    for (i = 0; i < VALUES_MAX; i++) {
        if (value_same_cell(&store->items[i], cx, cy)) {
            n++;
        }
    }
    return n;
}

const EntityValue *values_cell_at(const ValueStore *store, int cx, int cy,
                                  int nth)
{
    int i;
    int n = 0;

    for (i = 0; i < VALUES_MAX; i++) {
        const EntityValue *item = &store->items[i];

        if (!value_same_cell(item, cx, cy)) {
            continue;
        }
        if (n == nth) {
            return item;
        }
        n++;
    }
    return NULL;
}
