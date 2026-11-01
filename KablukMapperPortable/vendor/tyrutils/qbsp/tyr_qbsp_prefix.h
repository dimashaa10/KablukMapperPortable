/*
 * tyr_qbsp_prefix.h - Kabluk Mapper addition.
 *
 * qbsp ships its own private copies of cmdlib, mathlib, file and bspfile
 * (that is how upstream keeps it self-contained). When qbsp, light and vis
 * are linked into a single editor binary those copies collide with
 * common/*.c and with light/entities.c, so every clashing qbsp symbol gets a
 * "q_" prefix here. The header is included first by qbsp.h, and all qbsp
 * translation units include qbsp.h, so the rename is consistent.
 *
 * The list is generated, see the note at the bottom.
 */
#ifndef TYR_QBSP_PREFIX_H
#define TYR_QBSP_PREFIX_H

#include <common/tyr_port.h>

/* qbsp/bspfile.c vs common/bspfile.c */
#define LoadBSPFile q_LoadBSPFile
#define WriteBSPFile q_WriteBSPFile
#define PrintBSPFileSizes q_PrintBSPFileSizes

/* qbsp/cmdlib.c + qbsp/file.c + qbsp/util.c vs common/cmdlib.c */
#define copystring q_copystring
#define I_FloatTime q_I_FloatTime
#define DefaultExtension q_DefaultExtension
#define StripExtension q_StripExtension
#define StripFilename q_StripFilename
#define LoadFile q_LoadFile
#define Error q_Error

/* qbsp/mathlib.c + qbsp/globals.c vs common/mathlib.c */
#define VectorLength q_VectorLength
#define VectorCompare q_VectorCompare
#define VectorNormalize q_VectorNormalize
#define CrossProduct q_CrossProduct
#define vec3_origin q_vec3_origin

/* qbsp/map.c vs light/entities.c */
#define GetVectorForKey q_GetVectorForKey
#define ValueForKey q_ValueForKey
#define WriteEntitiesToString q_WriteEntitiesToString

/* qbsp/winding.c vs vis/vis.c */
#define CopyWinding q_CopyWinding

/*
 * Regenerate after an upstream sync with:
 *   nm --defined-only -g build/qbsp/*.o build/light/*.o build/vis/*.o build/common/*.o
 * and rename every symbol defined in both a qbsp object and any other one.
 */

#endif /* TYR_QBSP_PREFIX_H */
