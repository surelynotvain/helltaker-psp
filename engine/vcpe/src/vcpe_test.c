/* VCPE test harness: AUTOTEST.TXT input scripts and frame recording (REC.RAW). */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspiofilemgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "vcpe_sys.h"
#include "vcpe_test.h"

int vcpe_autotest;
static VcpeTestHooks hk;
static char *script, *script_pos;
static int script_wait, script_hold_frames;
static unsigned script_hold;
static int rec_every, rec_fd = -1, rec_n;

int vcpe_test_load(const VcpeTestHooks *hooks)
{
    char p[300];
    vcpe_path(p, sizeof p, "AUTOTEST.TXT");
    int fd = sceIoOpen(p, PSP_O_RDONLY, 0);
    if (fd < 0)
        return 0;
    int size = sceIoLseek32(fd, 0, PSP_SEEK_END);
    sceIoLseek32(fd, 0, PSP_SEEK_SET);
    script = malloc(size + 1);
    sceIoRead(fd, script, size);
    script[size] = 0;
    sceIoClose(fd);
    script_pos = script;
    hk = *hooks;
    vcpe_autotest = 1;
    return 1;
}

void vcpe_test_step(unsigned *held)
{
    *held = 0;
    if (!script)
        return;
    if (hk.pre)
        hk.pre();
    if (script_hold_frames > 0) {
        script_hold_frames--;
        *held = script_hold;
        return;
    }
    if (hk.busy && hk.busy(held))
        return;
    if (script_wait > 0) {
        script_wait--;
        return;
    }
    while (*script_pos) {
        char line[64];
        int n = 0;
        while (*script_pos && *script_pos != '\n' && n < 63)
            line[n++] = *script_pos++;
        line[n] = 0;
        if (*script_pos == '\n')
            script_pos++;
        char cmd[16] = {0}, arg[16] = {0};
        int num = 0, num2 = 0;
        int k = sscanf(line, "%15s %15s %d %d", cmd, arg, &num, &num2);
        if (k <= 0 || cmd[0] == '#')
            continue;
        int r = VCPE_CMD_UNKNOWN;
        if (!strcmp(cmd, "wait")) {
            script_wait = atoi(arg) - 1;
            r = VCPE_CMD_FRAME;
        } else if (!strcmp(cmd, "press")) {
            *held = hk.button ? hk.button(arg) : 0;
            r = VCPE_CMD_FRAME;
        } else if (!strcmp(cmd, "hold")) {
            script_hold = hk.button ? hk.button(arg) : 0;
            script_hold_frames = num - 1;
            *held = script_hold;
            r = VCPE_CMD_FRAME;
        } else if (!strcmp(cmd, "quit")) {
            sceKernelExitGame();
            r = VCPE_CMD_FRAME;
        } else if (!strcmp(cmd, "rec")) {
            /* rec N: append every Nth displayed frame (480x272 RGBA) to REC.RAW; rec 0 stops */
            if (rec_fd >= 0) { sceIoClose(rec_fd); rec_fd = -1; }
            rec_every = atoi(arg);
            if (rec_every > 0) {
                char p[300];
                vcpe_path(p, sizeof p, "REC.RAW");
                rec_fd = sceIoOpen(p, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_APPEND, 0777);
                rec_n = 0;
            }
            r = VCPE_CMD_NEXT;
        } else if (hk.command) {
            r = hk.command(cmd, arg, num, num2, line, held);
        }
        if (r == VCPE_CMD_FRAME || (r != VCPE_CMD_UNKNOWN && hk.every_command_takes_a_frame))
            return;
    }
}

void vcpe_test_frame_end(void)
{
    if (rec_fd >= 0 && ++rec_n % rec_every == 0) {
        void *top; int bw, pf;
        sceDisplayGetFrameBuf(&top, &bw, &pf, PSP_DISPLAY_SETBUF_IMMEDIATE);
        const uint8_t *fb = (const uint8_t *)((uintptr_t)top | 0x40000000);
        for (int y = 0; y < 272; y++)
            sceIoWrite(rec_fd, fb + y * bw * 4, 480 * 4);
    }
}
