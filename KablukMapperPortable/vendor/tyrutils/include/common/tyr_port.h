/*
 * tyr_port.h - small portability shims added by Kabluk Mapper.
 *
 * This is the ONLY extra header the vendored TyrUtils sources need in order
 * to be compiled into the editor instead of as stand-alone programs.
 *
 *   - TYR_PRINTF_ATTR / TYR_NORETURN_ATTR hide the GCC attribute syntax that
 *     MSVC does not understand.
 *   - tyr_log_hook / tyr_fatal_exit are implemented by bspchain.c: the first
 *     one feeds the compile log window, the second turns the tools' exit(1)
 *     into a longjmp back into the editor instead of killing the process.
 */
#ifndef TYR_PORT_H
#define TYR_PORT_H

#if defined(_MSC_VER)
#define TYR_PRINTF_ATTR(fmt, first)
#define TYR_NORETURN_ATTR
#define TYR_NOINLINE __declspec(noinline)
/* no POSIX strcasecmp in the MSVC CRT */
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#define TYR_PRINTF_ATTR(fmt, first) __attribute__((format(printf, fmt, first)))
#define TYR_NORETURN_ATTR __attribute__((noreturn))
#define TYR_NOINLINE __attribute__((noinline))
#endif

#if defined(_WIN32)
#define TYR_PATHSEP '\\'
#else
#define TYR_PATHSEP '/'
#endif

/* The three tools keep their own main(), just under a private name. */
#define TYR_QBSP_ENTRY tyr_qbsp_main
#define TYR_LIGHT_ENTRY tyr_light_main
#define TYR_VIS_ENTRY tyr_vis_main

/* Set by the editor, NULL when the tools run stand-alone. */
extern void (*tyr_log_hook)(const char *text);

/* Never returns when the editor is driving (longjmps back to bspchain.c). */
void tyr_fatal_exit(void) TYR_NORETURN_ATTR;

/* Free memory in KB, -1 when the platform cannot say. Implemented by
 * bspchain.c (sceKernelMaxFreeMemSize() on the PSP). The tools print it next
 * to the size of the file they are about to load: a load that dies on the
 * console is usually a load that had no room, and this is the number that
 * says so.
 *
 * On the PSP it reads 512 KB and stays there, because pspdev takes the whole
 * user partition for the malloc heap the first time anything allocates and
 * leaves exactly that as a reserve. The budget the tools really have is the
 * "user memory before startup" line the editor writes to its own log. */
int tyr_free_kb(void);

#endif /* TYR_PORT_H */
