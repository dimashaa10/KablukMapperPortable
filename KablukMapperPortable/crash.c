/*
 * Kabluk Mapper Portable - crash.c
 *
 * See crash.h: a breadcrumb file, no pspsdk exception API. The file is opened
 * per line rather than kept open, because the thing being written to is a
 * memory stick and a crash must not take an unflushed buffer with it.
 */
#include "crash.h"

#include <stdio.h>
#include <string.h>

#if defined(KMP_HAVE_PSP) || defined(__PSP__)
#define CRASH_HAVE_PSP 1
#else
#define CRASH_HAVE_PSP 0
#endif

#define CRASH_STEP_LEN 40
#define CRASH_FILE_NAME "kabluk-phase.txt"

static char crash_last[CRASH_STEP_LEN] = "";

int crash_init(void)
{
    FILE *file;

#if !CRASH_HAVE_PSP
    /* the desktop has debuggers and core files; a stray file in the working
     * directory would only get committed by accident */
    return 0;
#endif

    file = fopen(CRASH_FILE_NAME, "w");
    if (file == NULL) {
        return 0;
    }
    fprintf(file, "Kabluk Mapper Portable - last steps\n");
    fprintf(file, "The console died right after the last line below.\n\n");
    fflush(file);
    fclose(file);
    crash_last[0] = '\0';
    return 1;
}

void crash_step(const char *step)
{
    FILE *file;
    int i = 0;

    if (step == NULL) {
        return;
    }
    if (strcmp(step, crash_last) == 0) {
        return; /* nothing new: no write, no memory stick traffic */
    }
    while (step[i] != '\0' && i < CRASH_STEP_LEN - 1) {
        crash_last[i] = step[i];
        i++;
    }
    crash_last[i] = '\0';

#if CRASH_HAVE_PSP
    file = fopen(CRASH_FILE_NAME, "a");
    if (file == NULL) {
        return;
    }
    fprintf(file, "%s\n", crash_last);
    fflush(file);
    fclose(file);
#else
    (void)file;
#endif
}

const char *crash_step_name(void)
{
    return crash_last;
}
