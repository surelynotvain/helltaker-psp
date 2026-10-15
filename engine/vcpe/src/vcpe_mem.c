/* VCPE memory for the 32 MB PSP-1000 (see vcpe_mem.h). */
#include <pspkernel.h>
#include <pspsuspend.h>
#include <malloc.h>
#include "vcpe_sys.h"
#include "vcpe_mem.h"

int vcpe_mem_32mb(void)
{
    static int small = -1;
    if (small < 0)
        small = vcpe_heap_kb() < 36000;
    return small;
}

uint8_t *vcpe_volatile_mem(uint32_t *size)
{
    static int tried;
    static void *mem;
    static int msize;
    if (!tried) {
        tried = 1;
        int st = vcpe_step_begin("Locking volatile memory (4096 KB)");
        if (sceKernelVolatileMemTryLock(0, &mem, &msize) < 0 || !mem || msize <= 0) {
            mem = 0;
            msize = 0;
        }
        vcpe_step_end(st, mem != 0);
    }
    if (size)
        *size = (uint32_t)msize;
    return (uint8_t *)mem;
}

static int peak;
int vcpe_mem_used_kb(void)
{
    struct mallinfo mi = mallinfo();
    if ((int)mi.uordblks > peak)
        peak = mi.uordblks;
    return mi.uordblks / 1024;
}

int vcpe_mem_peak_kb(void)
{
    vcpe_mem_used_kb();
    return peak / 1024;
}
