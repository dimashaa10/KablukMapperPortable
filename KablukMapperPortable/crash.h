/*
 * Kabluk Mapper Portable - crash.h
 *
 * A console that dies with nothing in any log is the hardest kind of bug to
 * chase, and the PSP is good at that: a CPU exception takes the whole app
 * back to the XMB without a word. This is the safety net for it.
 *
 * crash_install() puts a handler under the CPU exceptions on the PSP, and the
 * handler writes kabluk-crash.txt - the exception code, EPC, cause, every
 * register and a stack trace - before it lets the app go. crash_phase() is
 * what makes that file useful: it keeps a short line in RAM naming what the
 * app was doing, so the dump says "phase: save step 4" and not just an
 * address.
 *
 * On the desktop both functions do nothing: a host crash is a core file and a
 * debugger's job.
 */
#ifndef KMP_CRASH_H
#define KMP_CRASH_H

/* Installs the exception handler (PSP only, no-op elsewhere). Returns 1 when
 * a handler is in place. */
int crash_install(void);

/* Names what the app is doing right now. 'phase' must be a string literal or
 * something that outlives the call - it is stored, not copied. */
void crash_phase(const char *phase);

/* The phase last set, for the dump and for the tests. */
const char *crash_phase_name(void);

#endif /* KMP_CRASH_H */
