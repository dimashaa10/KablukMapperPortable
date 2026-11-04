/*
 * Host-side stub of pspsdk's pspdebug.h.
 *
 * crash.c writes a register dump when the PSP takes a CPU exception, and the
 * host test build only needs the declarations to type check: the two structs
 * are copied field for field from pspsdk (src/debug/pspdebug.h), because a
 * wrong layout here would hide a wrong layout there.
 */
#ifndef PSPDEBUG_H_STUB
#define PSPDEBUG_H_STUB

typedef unsigned int u32;

typedef struct _PspDebugRegBlock {
    u32 frame[6];
    u32 r[32];
    u32 status;
    u32 lo;
    u32 hi;
    u32 badvaddr;
    u32 cause;
    u32 epc;
    float fpr[32];
    u32 fsr;
    u32 fir;
    u32 frame_ptr;
    u32 unused;
    u32 index;
    u32 random;
    u32 entrylo0;
    u32 entrylo1;
    u32 context;
    u32 pagemask;
    u32 wired;
    u32 cop0_7;
    u32 cop0_8;
    u32 cop0_9;
    u32 entryhi;
    u32 cop0_11;
    u32 cop0_12;
    u32 cop0_13;
    u32 cop0_14;
    u32 prid;
    u32 padding[100];
} PspDebugRegBlock;

typedef void (*PspDebugErrorHandler)(PspDebugRegBlock *regs);

typedef struct _PspDebugStackTrace {
    u32 call_addr;
    u32 func_addr;
} PspDebugStackTrace;

int pspDebugInstallErrorHandler(PspDebugErrorHandler handler);
int pspDebugGetStackTrace2(PspDebugRegBlock *regs, PspDebugStackTrace *trace,
                           int max);

#endif /* PSPDEBUG_H_STUB */
