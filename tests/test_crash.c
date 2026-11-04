/*
 * Host test: the breadcrumb file a dead console leaves behind (crash.c).
 *
 * The interesting part is not that a line gets written, but that the file is
 * flushed as it goes and that the last line is the step the app entered. The
 * test build passes -DKMP_HAVE_PSP, because the writing half only exists for
 * the console; the editor on the desktop has debuggers and no need for it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "crash.h"

#define TEST_CWD_ENV "KMP_TEST_CWD"
#define FILE_NAME "kabluk-phase.txt"

static int failures;

static void check(int ok, const char *what)
{
    if (ok) {
        printf("%-52s ok\n", what);
    } else {
        printf("%-52s FAIL\n", what);
        failures++;
    }
}

/* the last line of the file, "none" when it has none */
static void last_line(char *out, int cap)
{
    FILE *file;
    char line[128];
    char last[128];

    out[0] = '\0';
    strcpy(last, "");
    file = fopen(FILE_NAME, "r");
    if (file == NULL) {
        strncpy(out, "none", (size_t)(cap - 1));
        out[cap - 1] = '\0';
        return;
    }
    while (fgets(line, (int)sizeof(line), file) != NULL) {
        strncpy(last, line, sizeof(last) - 1);
        last[sizeof(last) - 1] = '\0';
    }
    fclose(file);
    {
        int len = (int)strlen(last);
        while (len > 0 && (last[len - 1] == '\n' || last[len - 1] == '\r')) {
            last[--len] = '\0';
        }
    }
    strncpy(out, last, (size_t)(cap - 1));
    out[cap - 1] = '\0';
}

static int count_line(const char *want)
{
    FILE *file;
    char line[128];
    int n = 0;

    file = fopen(FILE_NAME, "r");
    if (file == NULL) {
        return 0;
    }
    while (fgets(line, (int)sizeof(line), file) != NULL) {
        int len = (int)strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }
        if (strcmp(line, want) == 0) {
            n++;
        }
    }
    fclose(file);
    return n;
}

int main(void)
{
    const char *cwd = getenv(TEST_CWD_ENV);
    char last[128];
    char too_long[200];

    if (cwd != NULL) {
        if (chdir(cwd) != 0) {
            printf("cannot enter %s\n", cwd);
            return 1;
        }
    }
    remove(FILE_NAME);

    printf("== the phase breadcrumbs\n");
    check(crash_init() == 1, "the file starts on a writable folder");
    check(crash_step_name()[0] == '\0', "and knows no steps yet");

    crash_step("save start");
    crash_step("save step 0");
    crash_step("save step 0"); /* twice: one line, not two */
    crash_step("save step 1");
    last_line(last, (int)sizeof(last));
    check(strcmp(last, "save step 1") == 0, "the last line is the last step");
    check(count_line("save step 0") == 1, "a repeated step is written once");
    check(strcmp(crash_step_name(), "save step 1") == 0,
          "the step name is remembered in memory too");

    crash_step("editor");
    last_line(last, (int)sizeof(last));
    check(strcmp(last, "editor") == 0, "a later step replaces the last one");
    check(count_line("save step 1") == 1, "the history stays in the file");

    /* a rogue long name is cut, not spilled over the buffer */
    memset(too_long, 'x', sizeof(too_long) - 1);
    too_long[sizeof(too_long) - 1] = '\0';
    crash_step(too_long);
    last_line(last, (int)sizeof(last));
    check(strlen(last) == 39, "a long step name is truncated to 39 bytes");

    crash_step(NULL);
    last_line(last, (int)sizeof(last));
    check(strlen(last) == 39, "a NULL step changes nothing");

    check(crash_init() == 1, "a second start resets the file");
    check(count_line("The console died right after the last line below.") == 1,
          "the fresh file starts with its header again");
    check(count_line("editor") == 0, "and the steps of the last run are gone");

    printf("\n%s (%d failures)\n", failures == 0 ? "PASS" : "FAIL", failures);
    return failures == 0 ? 0 : 1;
}
