/* VCPE system: HOME / sleep callbacks, CPU clock, files next to the EBOOT. */
#include <pspkernel.h>
#include <psppower.h>
#include <string.h>
#include <stdio.h>
#include "vcpe_sys.h"

volatile int vcpe_running = 1;
volatile int vcpe_resume_gen;
static char base_dir[256];

static int exit_cb(int a, int b, void *c)
{
    (void)a; (void)b; (void)c;
    vcpe_running = 0;
    sceKernelExitGame();
    return 0;
}

static int power_cb(int unknown, int flags, void *arg)
{
    (void)unknown; (void)arg;
    if (flags & PSP_POWER_CB_RESUME_COMPLETE)
        vcpe_resume_gen++;   /* file readers reopen their files */
    return 0;
}

static int cb_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    int cb = sceKernelCreateCallback("exit", exit_cb, 0);
    sceKernelRegisterExitCallback(cb);
    int pcb = sceKernelCreateCallback("power", power_cb, 0);
    if (scePowerRegisterCallback(-1, pcb) < 0)
        scePowerRegisterCallback(0, pcb);
    sceKernelSleepThreadCB();
    return 0;
}

void vcpe_sys_init(int argc, char **argv)
{
    int th = sceKernelCreateThread("cb", cb_thread, 0x11, 0xFA0, 0, 0);
    if (th >= 0)
        sceKernelStartThread(th, 0, 0);
    scePowerSetClockFrequency(333, 333, 166);
    base_dir[0] = 0;
    if (argc > 0 && argv[0]) {
        strncpy(base_dir, argv[0], sizeof base_dir - 1);
        char *slash = strrchr(base_dir, '/');
        if (slash)
            slash[1] = 0;
        else
            base_dir[0] = 0;
    }
}

void vcpe_path(char *out, int n, const char *file)
{
    snprintf(out, n, "%s%s", base_dir, file);
}

void vcpe_quit(void) { vcpe_running = 0; }
