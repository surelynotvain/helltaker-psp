/* VCPE data PAK: one big read-only file, read by offset; survives sleep/resume and slow memory sticks. */
#include <pspkernel.h>
#include <pspiofilemgr.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include "vcpe_sys.h"
#include "vcpe_pak.h"

void (*vcpe_pak_progress)(uint32_t bytes);
static int pak_fd = -1;
static char pak_path[300];
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

void vcpe_pak_open_checked(const char *file, const char *title, const char *magic, uint32_t size, uint32_t stamp)
{
    char p[300], msg[600];
    vcpe_path(p, sizeof p, file);
    if (vcpe_pak_open(p) < 0) {
        snprintf(msg, sizeof msg, "%s is missing.\n\nCopy %s next to EBOOT.PBP (all files of the same build).", file, file);
        vcpe_fatal(title, msg);
    }
    struct { char magic[4]; uint32_t size, stamp; } h;
    int bad = vcpe_pak_read(0, &h, sizeof h) || (magic && memcmp(h.magic, magic, 4));
    if (!bad && size) {
        int end = sceIoLseek32(pak_fd, 0, PSP_SEEK_END);
        bad = h.size != size || end < 0 || (uint32_t)end < size;
    }
    if (!bad && stamp)
        bad = h.stamp != stamp;
    if (bad) {
        snprintf(msg, sizeof msg, "%s does not match this EBOOT.PBP (an incomplete copy, or a file from another "
                 "version).\n\nCopy EBOOT.PBP and all the PAK files again, all from the same build.", file);
        vcpe_fatal(title, msg);
    }
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
            /* with a progress hook (loading screen) in chunks, so it can redraw between them */
            uint32_t n = size - got;
            if (vcpe_pak_progress && n > 256u * 1024)
                n = 256u * 1024;
            int r = sceIoRead(pak_fd, mem + got, n);
            if (r <= 0)
                break;
            got += r;
            if (vcpe_pak_progress)
                vcpe_pak_progress((uint32_t)r);
        }
    }
    return got == size ? 0 : -1;
}

void *vcpe_pak_read_alloc(uint32_t off, uint32_t size)
{
    const char *name = strrchr(pak_path, '/');
    int st = vcpe_step_begin("Reading %s @%08x: %u KB", name ? name + 1 : pak_path, (unsigned)off,
                             (unsigned)((size + 1023) / 1024));
    uint8_t *mem = memalign(64, size ? size : 64);
    if (!mem) {
        vcpe_step_end(st, 0);
        return 0;
    }
    if (vcpe_pak_read(off, mem, size) < 0) {
        free(mem);
        vcpe_step_end(st, 0);
        return 0;
    }
    sceKernelDcacheWritebackRange(mem, size);
    vcpe_step_end(st, 1);
    return mem;
}
