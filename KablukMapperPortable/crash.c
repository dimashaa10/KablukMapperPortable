/*
 * Kabluk Mapper Portable - crash.c
 *
 * See crash.h. Everything here is PSP specific; on any other platform the two
 * entry points are empty so the editor code can call them without #ifdefs.
 */
#include "crash.h"

#include <stddef.h>
#include <stdio.h>

#if defined(KMP_HAVE_PSP) || defined(__PSP__)
#define CRASH_HAVE_PSP 1
#else
#define CRASH_HAVE_PSP 0
#endif

#if CRASH_HAVE_PSP
#include <pspkernel.h>
#include <pspdebug.h>
#endif

#define CRASH_PHASE_LEN 40

static char crash_phase_buf[CRASH_PHASE_LEN] = "startup";

void crash_phase(const char *phase)
{
    int i = 0;

    if (phase == NULL) {
        return;
    }
    while (phase[i] != '\0' && i < CRASH_PHASE_LEN - 1) {
        crash_phase_buf[i] = phase[i];
        i++;
    }
    crash_phase_buf[i] = '\0';
}

const char *crash_phase_name(void)
{
    return crash_phase_buf;
}

#if CRASH_HAVE_PSP

/* The dump goes next to the EBOOT (the app chdir()s there at startup), and
 * says what the app was doing, where it died, and what the stack looked
 * like - psp-addr2line against KablukMapper.elf turns the addresses into
 * function names. */
static void crash_dump(PspDebugRegBlock *regs)
{
    static PspDebugStackTrace trace[16];
    FILE *file;
    int frames;
    int i;

    frames = pspDebugGetStackTrace2(regs, trace, 16);

    file = fopen("kabluk-crash.txt", "w");
    if (file == NULL) {
        return;
    }
    fprintf(file, "Kabluk Mapper Portable - CPU exception\n\n");
    fprintf(file, "phase:    %s\n", crash_phase_buf);
    fprintf(file, "epc:      %08X  (where it died)\n", (unsigned int)regs->epc);
    fprintf(file, "cause:    %08X  (exception %d)\n", (unsigned int)regs->cause,
            (int)((regs->cause >> 2) & 0x1F));
    fprintf(file, "badvaddr: %08X  (address it touched)\n",
            (unsigned int)regs->badvaddr);
    fprintf(file, "status:   %08X\n", (unsigned int)regs->status);
    fprintf(file, "ra:       %08X  (caller)\n", (unsigned int)regs->r[31]);
    fprintf(file, "sp:       %08X\n", (unsigned int)regs->r[29]);

    fprintf(file, "\nregisters:\n");
    for (i = 0; i < 32; i++) {
        fprintf(file, "r%-2d %08X%s", i, (unsigned int)regs->r[i],
                (i % 4 == 3) ? "\n" : "  ");
    }

    fprintf(file, "\nstack trace (%d frame(s), innermost first):\n", frames);
    for (i = 0; i < frames; i++) {
        fprintf(file, "  %08X  called from %08X\n",
                (unsigned int)trace[i].func_addr,
                (unsigned int)trace[i].call_addr);
    }
    fflush(file);
    fclose(file);
}

static void crash_handler(PspDebugRegBlock *regs)
{
    crash_dump(regs);
    /* No point carrying on with a corrupted stack: leave the XMB an EBOOT it
     * can start again, with the dump on the stick. */
    sceKernelExitGame();
    for (;;) {
        /* sceKernelExitGame does not return */
    }
}

int crash_install(void)
{
    return pspDebugInstallErrorHandler(crash_handler) >= 0;
}

#else /* !CRASH_HAVE_PSP */

int crash_install(void)
{
    return 0;
}

#endif /* CRASH_HAVE_PSP */
