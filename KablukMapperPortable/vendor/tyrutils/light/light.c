/*  Copyright (C) 1996-1997  Id Software, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    See file, 'COPYING', for details.
*/

#include <stdint.h>

#include <light/light.h>
#include <light/entities.h>

float scaledist = 1.0;
float rangescale = 0.5;
float anglescale = 0.5;
float sun_anglescale = 0.5;
float fadegate = EQUAL_EPSILON;
int softsamples = 0;
const vec3_t vec3_white = { 255, 255, 255 };

qboolean addminlight = false;
lightsample_t minlight = { 0, { 255, 255, 255 } };
lightsample_t sunlight = { 0, { 255, 255, 255 } };
vec3_t sunvec = { 0, 0, 16384 };		/* defaults to straight down */

byte *filebase;			// start of lightmap data
static byte *file_p;		// start of free space after data
static byte *file_end;		// end of free space for lightmap data

byte *lit_filebase;		// start of litfile data
static byte *lit_file_p;	// start of free space after litfile data
static byte *lit_file_end;	// end of space for litfile data

static modelinfo_t *modelinfo;
const dmodel_t *const *tracelist;

int oversample = 1;
qboolean write_litfile = false;
qboolean litfile_only = false;

int32_t
AllocateLightDataOffset(int numsamples)
{
    int32_t offset;

    ThreadLock();

    /* align to 4 byte boudaries */
    file_p = (byte *)(((uintptr_t)file_p + 3) & ~3);
    offset = file_p - filebase;

    file_p += numsamples;

    /* .lit space only exists when coloured light was requested */
    if (write_litfile) {
	/* align to 12 byte boundaries to match offets with 3 * lightdata */
	if ((uintptr_t)lit_file_p % 12)
	    lit_file_p += 12 - ((uintptr_t)lit_file_p % 12);
	lit_file_p += numsamples * 3;
    }

    ThreadUnlock();

    if (file_p > file_end)
	Error("%s: overrun", __func__);

    if (write_litfile && lit_file_p > lit_file_end)
	Error("%s: overrun", __func__);

    return offset;
}

byte *
LightDataDest(const bsp2_dface_t *face)
{
    return filebase + face->lightofs;
}

byte *
ColorDataDest(const bsp2_dface_t *face)
{
    return lit_filebase + 3 * face->lightofs;
}

static void *
LightThread(void *arg)
{
    int facenum, i;
    dmodel_t *model;
    const bsp2_t *bsp = arg;

    while (1) {
	facenum = GetThreadWork();
	if (facenum == -1)
	    break;

	/* Find the correct model offset */
	for (i = 0, model = bsp->dmodels; i < bsp->nummodels; i++, model++) {
	    if (facenum < model->firstface)
		continue;
	    if (facenum < model->firstface + model->numfaces)
		break;
	}
	if (i == bsp->nummodels) {
	    logprint("warning: no model has face %d\n", facenum);
	    continue;
	}

	LightFace(bsp->dfaces + facenum, &modelinfo[i], bsp);
    }

    return NULL;
}

static void
FindModelInfo(const bsp2_t *bsp)
{
    int i, shadow, numshadowmodels;
    entity_t *entity;
    char modelname[20];
    const char *attribute;
    const dmodel_t **shadowmodels;
    modelinfo_t *info;

    shadowmodels = malloc(sizeof(dmodel_t *) * (bsp->nummodels + 1));
    /* Kabluk Mapper: the PSP has 32 MB in total, so every allocation here has
     * to fail as a log line instead of a NULL write. */
    if (!shadowmodels)
	Error("%s: allocation of %d bytes failed\n", __func__,
	      (int)(sizeof(dmodel_t *) * (bsp->nummodels + 1)));
    memset(shadowmodels, 0, sizeof(dmodel_t *) * (bsp->nummodels + 1));

    /* The world always casts shadows */
    shadowmodels[0] = &bsp->dmodels[0];
    numshadowmodels = 1;

    memset(modelinfo, 0, sizeof(*modelinfo) * bsp->nummodels);
    modelinfo[0].model = &bsp->dmodels[0];

    for (i = 1, info = modelinfo + 1; i < bsp->nummodels; i++, info++) {
	info->model = &bsp->dmodels[i];

	/* Find the entity for the model */
	snprintf(modelname, sizeof(modelname), "*%d", i);
	entity = FindEntityWithKeyPair("model", modelname);
	if (!entity)
	    Error("%s: Couldn't find entity for model %s.\n", __func__,
		  modelname);

	/* Check if this model will cast shadows (shadow => shadowself) */
	shadow = atoi(ValueForKey(entity, "_shadow"));
	if (shadow) {
	    shadowmodels[numshadowmodels++] = &bsp->dmodels[i];
	} else {
	    shadow = atoi(ValueForKey(entity, "_shadowself"));
	    if (shadow)
		info->shadowself = true;
	}

	/* Set up the offset for rotate_* entities */
	attribute = ValueForKey(entity, "classname");
	if (!strncmp(attribute, "rotate_", 7))
	    GetVectorForKey(entity, "origin", info->offset);

	/* Grab the bmodel minlight values, if any */
	attribute = ValueForKey(entity, "_minlight");
	if (attribute[0])
	    info->minlight.light = atoi(attribute);
	GetVectorForKey(entity, "_mincolor", info->minlight.color);
	if (!VectorCompare(info->minlight.color, vec3_origin)) {
	    if (!write_litfile)
		write_litfile = true;
	} else {
	    VectorCopy(vec3_white, info->minlight.color);
	}
    }

    tracelist = shadowmodels;
}

/*
 * =============
 *  LightWorld
 * =============
 */
/*
 * ================
 * EstimateLightDataSize
 * Sum of the lumels every face needs, plus the per face alignment slack.
 * One light style per face is assumed, which is what maps without styled
 * light entities contain - and the editor never writes styled lights.
 * ================
 */
static int32_t
EstimateLightDataSize(const bsp2_t *bsp)
{
    int32_t size = 0;
    int i, j;

    for (i = 0; i < bsp->nummodels; i++) {
	const dmodel_t *model = &bsp->dmodels[i];

	for (j = 0; j < model->numfaces; j++) {
	    const bsp2_dface_t *face = &bsp->dfaces[model->firstface + j];

	    size += FaceLightmapSamples(face, modelinfo[i].offset, bsp);
	    size += 4; /* AllocateLightDataOffset aligns every map to 4 bytes */
	}
    }

    return size + 16;
}

static void
LightWorld(bsp2_t *bsp)
{
    size_t allocsize;

    if (bsp->dlightdata)
	free(bsp->dlightdata);

    if (!litfile_only) {
	int32_t need = EstimateLightDataSize(bsp);

	if (need > KMP_MAX_MAP_LIGHTING)
	    Error("lightdata %i bytes exceeds the %i byte limit",
		  (int)need, (int)KMP_MAX_MAP_LIGHTING);
	bsp->lightdatasize = need;
    }

    /* 3 extra bytes per lumel only for the coloured .lit file */
    allocsize = (size_t)bsp->lightdatasize * (write_litfile ? 4 : 1) + 16;
    bsp->dlightdata = malloc(allocsize);
    if (!bsp->dlightdata)
	Error("%s: allocation of %i bytes failed.", __func__, (int)allocsize);
    memset(bsp->dlightdata, 0, allocsize);

    /* align filebase to a 4 byte boundary */
    filebase = file_p = (byte *)(((uintptr_t)bsp->dlightdata + 3) & ~3);
    file_end = filebase + bsp->lightdatasize;

    /* litfile data stored in dlightdata, after the white light */
    if (write_litfile) {
	lit_filebase = file_end + 12 - ((uintptr_t)file_end % 12);
	lit_file_p = lit_filebase;
	lit_file_end = lit_filebase + 3 * bsp->lightdatasize;
    } else {
	lit_filebase = lit_file_p = lit_file_end = file_end;
    }

    RunThreadsOn(0, bsp->numfaces, LightThread, bsp);
    logprint("Lighting Completed.\n\n");

    if (!litfile_only)
        bsp->lightdatasize = file_p - filebase;
    logprint("lightdatasize: %i\n", bsp->lightdatasize);
}


/*
 * ==================
 * main
 * light modelfile
 * ==================
 */
int
TYR_LIGHT_ENTRY(int argc, const char **argv)
{
    bspdata_t bspdata;
    bsp2_t *const bsp = &bspdata.data.bsp2;
    int32_t loadversion;
    int i;
    double start;
    double end;
    char source[1024];

    init_log("light.log");
    logprint("---- light / TyrUtils " stringify(TYRUTILS_VERSION) " ----\n");

    numthreads = GetDefaultThreads();

    for (i = 1; i < argc; i++) {
	if (!strcmp(argv[i], "-threads")) {
	    numthreads = atoi(argv[++i]);
	} else if (!strcmp(argv[i], "-extra")) {
	    oversample = 2;
	    logprint("extra 2x2 sampling enabled\n");
	} else if (!strcmp(argv[i], "-extra4")) {
	    oversample = 4;
	    logprint("extra 4x4 sampling enabled\n");
	} else if (!strcmp(argv[i], "-dist")) {
	    scaledist = atof(argv[++i]);
	} else if (!strcmp(argv[i], "-range")) {
	    rangescale = atof(argv[++i]);
	} else if (!strcmp(argv[i], "-gate")) {
	    fadegate = atof(argv[++i]);
	} else if (!strcmp(argv[i], "-light")) {
	    minlight.light = atof(argv[++i]);
	} else if (!strcmp(argv[i], "-addmin")) {
	    addminlight = true;
	} else if (!strcmp(argv[i], "-lit")) {
	    write_litfile = true;
	} else if (!strcmp(argv[i], "-litonly")) {
            write_litfile = true;
	    litfile_only = true;
	} else if (!strcmp(argv[i], "-soft")) {
	    if (i < argc - 2 && isdigit(argv[i + 1][0]))
		softsamples = atoi(argv[++i]);
	    else
		softsamples = -1; /* auto, based on oversampling */
	} else if (!strcmp(argv[i], "-anglescale") || !strcmp(argv[i], "-anglesense")) {
	    if (i < argc - 2 && isdigit(argv[i + 1][0]))
		anglescale = sun_anglescale = atoi(argv[++i]);
	    else
		Error("-anglesense requires a numeric argument (0.0 - 1.0)");
	} else if (argv[i][0] == '-')
	    Error("Unknown option \"%s\"", argv[i]);
	else
	    break;
    }

    if (i != argc - 1) {
	printf("usage: light [-threads num] [-extra|-extra4]\n"
	       "             [-light num] [-addmin] [-anglescale|-anglesense]\n"
	       "             [-dist n] [-range n] [-gate n] [-lit] [-litonly]\n"
	       "             [-soft [n]] bspfile\n");
	tyr_fatal_exit();
    }

    if (numthreads > 1)
	logprint("running with %d threads\n", numthreads);
    if (write_litfile)
	logprint(".lit colored light output requested on command line.\n");
    if (softsamples == -1) {
	switch (oversample) {
	case 2:
	    softsamples = 1;
	    break;
	case 4:
	    softsamples = 2;
	    break;
	default:
	    softsamples = 0;
	    break;
	}
    }

    start = I_FloatTime();

    strcpy(source, argv[i]);
    StripExtension(source);
    DefaultExtension(source, ".bsp");
    LoadBSPFile(source, &bspdata);

    loadversion = bspdata.version;
    if (bspdata.version != BSP2VERSION)
	ConvertBSPFormat(BSP2VERSION, &bspdata);

    LoadEntities(bsp);
    MakeTnodes(bsp);
    modelinfo = malloc(bsp->nummodels * sizeof(*modelinfo));
    if (!modelinfo)
	Error("%s: allocation of %d bytes failed\n", __func__,
	      (int)(bsp->nummodels * sizeof(*modelinfo)));
    FindModelInfo(bsp);
    LightWorld(bsp);
    free(modelinfo);

    WriteEntitiesToString(bsp);

    if (write_litfile)
	WriteLitFile(bsp, source, LIT_VERSION);

    /* Convert data format back if and write out if necessary */
    if (!litfile_only) {
        if (loadversion != BSP2VERSION)
            ConvertBSPFormat(loadversion, &bspdata);

        WriteBSPFile(source, &bspdata);
    }

    end = I_FloatTime();
    logprint("%5.1f seconds elapsed\n", end - start);

    UnloadBSPFile(&bspdata); /* Kabluk Mapper: the editor compiles again */
    close_log();

    return 0;
}
