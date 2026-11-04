/*
 * Kabluk Mapper Portable - crash.h
 *
 * A console that dies with nothing in any log is the hardest kind of bug to
 * chase, and the PSP is good at that: a CPU exception takes the whole app back
 * to the XMB without a word.
 *
 * pspDebugInstallErrorHandler() would give a register dump, but it calls
 * sceKernelRegisterDefaultExceptionHandler(), which lives in
 * ExceptionManagerForKernel - a kernel import. An EBOOT that asks for it is at
 * best a link error (libpspdebug has the call, nothing defines the symbol) and
 * at worst a module that never loads. So this is the version that only needs
 * stdio: every meaningful step the app takes is appended to kabluk-phase.txt
 * and flushed, and the last line in that file says what it was doing when the
 * console died.
 */
#ifndef KMP_CRASH_H
#define KMP_CRASH_H

/* Starts a fresh kabluk-phase.txt next to the EBOOT (PSP) or the working
 * directory. Call once, early. Returns 1 when the file could be written -
 * without it the breadcrumbs are simply lost, which is what happened before. */
int crash_init(void);

/* Names a step: "save step 4", "compile step 7", "editor". The line is
 * written and flushed only when the name changes, so a step may be reported
 * as often as it likes. Cheap enough to call once per compile step, too
 * expensive for a per-frame call. */
void crash_step(const char *step);

/* The step last reported, for the tests. */
const char *crash_step_name(void);

#endif /* KMP_CRASH_H */
