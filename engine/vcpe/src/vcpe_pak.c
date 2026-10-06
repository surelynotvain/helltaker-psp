/* VCPE data PAK: one big read-only file, read by offset; survives sleep/resume and slow memory sticks. */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <malloc.h>
#include <stdio.h>
#include "vcpe_sys.h"
#include "vcpe_pak.h"

static int pak_fd = -1;
static char pak_path[256];
static int pak_gen;

static void pak_reopen(void)
{
    if (pak_fd >= 0)
        sceIoClose(pak_fd);
    pak_gen = vcpe_resume_gen;
    pak_fd = sceIoOpen(pak_path, PSP_O_RDONLY, 0);
}

int vcpe_pak_open(const char *path)
{
    snprintf(pak_path, sizeof pak_path, "%s", path);
    pak_gen = vcpe_resume_gen;
    pak_fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    return pak_fd;
}

void vcpe_pak_close(void)
{
    if (pak_fd >= 0)
        sceIoClose(pak_fd);
    pak_fd = -1;
}

int vcpe_pak_read(uint32_t off, void *dst, uint32_t size)
{
    uint8_t *mem = dst;
    uint32_t got = 0;
    /* a failed read (stale handle after sleep, stick still waking up) reopens the PAK and retries */
    for (int attempt = 0; attempt < 50 && got != size; attempt++) {
        if (attempt) {
            sceKernelDelayThread(100000);
            pak_reopen();
        } else if (pak_gen != vcpe_resume_gen || pak_fd < 0) {
            pak_reopen();
        }
        if (pak_fd < 0)
            continue;
        sceIoLseek32(pak_fd, off, PSP_SEEK_SET);
        got = 0;
        while (got < size) {
            int r = sceIoRead(pak_fd, mem + got, size - got);
            if (r <= 0)
                break;
            got += r;
        }
    }
    return got == size ? 0 : -1;
}

void *vcpe_pak_read_alloc(uint32_t off, uint32_t size)
{
    uint8_t *mem = memalign(64, size ? size : 64);
    if (!mem)
        return 0;
    if (vcpe_pak_read(off, mem, size) < 0) {
        free(mem);
        return 0;
    }
    sceKernelDcacheWritebackRange(mem, size);
    return mem;
}
