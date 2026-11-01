/*
 * Kabluk Mapper Portable - fgd.h
 *
 * Reader for Worldcraft/Quake .fgd definition files (resources/Quake.fgd).
 * The file is loaded once into a static buffer; classes are only indexed, the
 * properties of a class are parsed on demand (fgd_resolve) so the whole
 * definition set costs very little RAM on the PSP.
 */
#ifndef KMP_FGD_H
#define KMP_FGD_H

#include "config.h"

typedef struct FgdClass {
    char name[FGD_NAME_LEN];
    char desc[FGD_DESC_LEN];
    unsigned char kind; /* FGD_KIND_* */
    int body_start;     /* offsets into Fgd.text, -1 = no body */
    int body_end;
    char bases[FGD_MAX_BASES][FGD_NAME_LEN];
    int base_count;
} FgdClass;

typedef struct FgdProp {
    char key[FGD_NAME_LEN];
    char type[FGD_TYPE_LEN];
    char value[FGD_VALUE_LEN];
    char desc[FGD_DESC_LEN];
} FgdProp;

typedef struct Fgd {
    char text[FGD_TEXT_MAX];
    int text_len;
    int loaded;

    FgdClass classes[FGD_MAX_CLASSES];
    int class_count;

    /* properties of the class currently shown in the right panel */
    FgdProp props[FGD_MAX_PROPS];
    int prop_count;
    int props_class;  /* class index the props belong to, -1 = none */
} Fgd;

/* Searches for the fgd next to the executable / in the repo folders when
 * 'path' is NULL. Returns 1 on success. */
int fgd_load(Fgd *fgd, const char *path);

const FgdClass *fgd_class(const Fgd *fgd, int index);
int fgd_find(const Fgd *fgd, const char *name);
int fgd_count(const Fgd *fgd);

/* Indices of all @PointClass entities, in file order. Returns the count. */
int fgd_point_list(const Fgd *fgd, int *out, int max);

/* Parses the properties of 'index' (including its base classes) into
 * fgd->props / fgd->prop_count. Returns the property count. */
int fgd_resolve(Fgd *fgd, int index);

#endif /* KMP_FGD_H */
