/* Host-side stub of the pspsdk header, enough to compile main.c for PSP. */
#ifndef PSPKERNEL_H_STUB
#define PSPKERNEL_H_STUB
int sceKernelExitGame(void);
unsigned int sceKernelMaxFreeMemSize(void);
int sceKernelDelayThread(unsigned int usec);
#endif
