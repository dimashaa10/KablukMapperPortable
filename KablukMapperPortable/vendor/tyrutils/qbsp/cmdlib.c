/*
    Copyright (C) 1996-1997  Id Software, Inc.
    Copyright (C) 1997       Greg Lewis

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
// cmdlib.c

#include "qbsp.h"

#include <sys/types.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <sys/time.h>
#endif
#include <string.h>

#define PATHSEPERATOR   '/'

char *
copystring(const char *s)
{
    char *b;

    b = AllocMem(OTHER, strlen(s) + 1, true);
    strcpy(b, s);
    return b;
}



/*
================
I_FloatTime
================
*/
double
I_FloatTime(void)
{
#if defined(_WIN32)
    return (double)GetTickCount() / 1000.0;
#else
    struct timeval tv;

    gettimeofday(&tv, NULL);

    return (double)tv.tv_sec + (tv.tv_usec / 1000000.0);
#endif
}


/*
=============================================================================

				MISC FUNCTIONS

=============================================================================
*/


void
DefaultExtension(char *path, const char *extension)
{
    char *src;

//
// if path doesn't have a .EXT, append extension
// (extension should include the .)
//
    src = path + strlen(path) - 1;

    while (*src != PATHSEPERATOR && src != path) {
	if (*src == '.')
	    return;		// it has an extension
	src--;
    }

    strcat(path, extension);
}


void
StripExtension(char *path)
{
    int length;

    length = strlen(path) - 1;
    while (length > 0 && path[length] != '.') {
	length--;
	if (path[length] == PATHSEPERATOR)
	    return;		// no extension
    }
    if (length)
	path[length] = 0;
}

void
StripFilename(char *path)
{
    int length;

    length = strlen(path) - 1;
    while (length > 0 && path[length] != PATHSEPERATOR)
        length--;
    path[length] = '\0';
}

int
IsAbsolutePath(const char *path)
{
    const char *p;

    if (path[0] == PATHSEPERATOR || path[0] == '\\')
	return true;

    /* Kabluk Mapper: the PSP spells absolute paths "ms0:/PSP/GAME/...", so a
     * drive letter is not the only thing that can come before the separator.
     * Anything with a ':' before the first separator is absolute: "C:/maps",
     * "ms0:/PSP/GAME/KABLUK/quake.wad". Without this the wad path from the
     * worldspawn key was glued onto -wadpath twice on the PSP and every
     * texture disappeared from the .bsp. */
    for (p = path; *p != '\0'; p++) {
	if (*p == PATHSEPERATOR || *p == '\\')
	    return false;
	if (*p == ':')
	    return true;
    }
    return false;
}
