/* Helltaker PSP - clean reimplementation driven by data converted from the user's own copy. */
#include <pspkernel.h>
#include <pspctrl.h>
#include <psppower.h>
#include <pspiofilemgr.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "gfx.h"
#include "audio.h"
#include "game.h"

PSP_MODULE_INFO("Helltaker PSP", PSP_MODULE_USER, 2, 0);
PSP_MAIN_THREAD_ATTR(THREAD_ATTR_USER | THREAD_ATTR_VFPU);
PSP_HEAP_SIZE_KB(-1024);

void game_start(int scene);
void game_frame(void);
void game_draw(void);

unsigned in_held, in_pressed;
Save g_save;
static char base_dir[256];
static volatile int running = 1;

static int exit_cb(int a, int b, void *c)
{
    (void)a; (void)b; (void)c;
    running = 0;
    sceKernelExitGame();
    return 0;
}

static int cb_thread(SceSize args, void *argp)
{
    (void)args; (void)argp;
    int cb = sceKernelCreateCallback("exit", exit_cb, 0);
    sceKernelRegisterExitCallback(cb);
    sceKernelSleepThreadCB();
    return 0;
}

static void path_join(char *out, int n, const char *file)
{
    snprintf(out, n, "%s%s", base_dir, file);
}

#define SAVE_MAGIC 0x48545031 /* HTP1 */

static void save_read(void)
{
    char p[300];
    path_join(p, sizeof p, "SETTINGS.BIN");
    memset(&g_save, 0, sizeof g_save);
    int fd = sceIoOpen(p, PSP_O_RDONLY, 0);
    if (fd >= 0) {
        sceIoRead(fd, &g_save, sizeof g_save);
        sceIoClose(fd);
    }
    if (g_save.magic != SAVE_MAGIC) {
        g_save.magic = SAVE_MAGIC;
        g_save.music_vol = 2;  /* PlayerPrefs defaults */
        g_save.sfx_vol = 3;
        g_save.chapter_reached = 0;
    }
}

void save_write(void)
{
    char p[300];
    path_join(p, sizeof p, "SETTINGS.BIN");
    int fd = sceIoOpen(p, PSP_O_WRONLY | PSP_O_CREAT | PSP_O_TRUNC, 0777);
    if (fd >= 0) {
        sceIoWrite(fd, &g_save, sizeof g_save);
        sceIoClose(fd);
    }
}

/* ------------------------------------------------------------ autotest
 * If AUTOTEST.TXT sits next to the EBOOT, inputs come from it instead of the pad:
 *   scene N      jump to scene N
 *   wait N       N frames with no input
 *   press B      one-frame press (up/down/left/right/ok/back/start/r/l)
 *   hold B N     hold for N frames
 *   quit         exit (the emulator saves its screenshot)                         */
static char *script, *script_pos;
int g_test_god;
static int script_wait, script_hold_frames;
static unsigned script_hold;

static unsigned btn_from(const char *s)
{
    if (!strcmp(s, "up")) return BTN_UP;
    if (!strcmp(s, "down")) return BTN_DOWN;
    if (!strcmp(s, "left")) return BTN_LEFT;
    if (!strcmp(s, "right")) return BTN_RIGHT;
    if (!strcmp(s, "ok")) return BTN_OK;
    if (!strcmp(s, "back")) return BTN_BACK;
    if (!strcmp(s, "start")) return BTN_START;
    if (!strcmp(s, "r")) return BTN_RESTART;
    if (!strcmp(s, "l")) return BTN_ADVICE;
    return 0;
}

static void script_load(void)
{
    char p[300];
    path_join(p, sizeof p, "AUTOTEST.TXT");
    int fd = sceIoOpen(p, PSP_O_RDONLY, 0);
    if (fd < 0)
        return;
    int size = sceIoLseek32(fd, 0, PSP_SEEK_END);
    sceIoLseek32(fd, 0, PSP_SEEK_SET);
    script = malloc(size + 1);
    sceIoRead(fd, script, size);
    script[size] = 0;
    sceIoClose(fd);
    script_pos = script;
}

static int script_step(unsigned *held)
{
    *held = 0;
    if (script_hold_frames > 0) {
        script_hold_frames--;
        *held = script_hold;
        return 1;
    }
    if (script_wait > 0) {
        script_wait--;
        return 1;
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
        int num = 0;
        int k = sscanf(line, "%15s %15s %d", cmd, arg, &num);
        if (k <= 0 || cmd[0] == '#')
            continue;
        if (!strcmp(cmd, "quit")) {
            sceKernelExitGame();
            return 1;
        }
        if (!strcmp(cmd, "scene")) {
            scene_request(atoi(arg));
            return 1;
        }
        if (!strcmp(cmd, "bossend")) {
            extern void boss_debug_end(void);
            boss_debug_end();
            return 1;
        }
        if (!strcmp(cmd, "dlg")) {   /* dlg D E: open dialogue D at element E */
            extern int g_dlg_force_start;
            g_dlg_force_start = num;
            dlg_start(atoi(arg));
            return 1;
        }
        if (!strcmp(cmd, "god")) {
            g_test_god = 1;   /* autotest only: hazards don't kill */
            return 1;
        }
        if (!strcmp(cmd, "pos")) {
            puzzle_teleport(atoi(arg), num);
            return 1;
        }
        if (!strcmp(cmd, "wait")) {
            script_wait = atoi(arg) - 1;
            return 1;
        }
        if (!strcmp(cmd, "press")) {
            *held = btn_from(arg);
            return 1;
        }
        if (!strcmp(cmd, "hold")) {
            script_hold = btn_from(arg);
            script_hold_frames = num - 1;
            *held = script_hold;
            return 1;
        }
    }
    return 1;
}

static void read_input(void)
{
    static unsigned prev;
    unsigned held = 0;
    if (script) {
        script_step(&held);
    } else {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);
        unsigned b = pad.Buttons;
        if (b & PSP_CTRL_UP) held |= BTN_UP;
        if (b & PSP_CTRL_DOWN) held |= BTN_DOWN;
        if (b & PSP_CTRL_LEFT) held |= BTN_LEFT;
        if (b & PSP_CTRL_RIGHT) held |= BTN_RIGHT;
        if (pad.Lx < 50) held |= BTN_LEFT;
        if (pad.Lx > 205) held |= BTN_RIGHT;
        if (pad.Ly < 50) held |= BTN_UP;
        if (pad.Ly > 205) held |= BTN_DOWN;
        if (b & PSP_CTRL_CROSS) held |= BTN_OK;
        if (b & PSP_CTRL_CIRCLE) held |= BTN_BACK;
        if (b & PSP_CTRL_START) held |= BTN_START;
        if (b & (PSP_CTRL_RTRIGGER | PSP_CTRL_SQUARE)) held |= BTN_RESTART;
        if (b & (PSP_CTRL_LTRIGGER | PSP_CTRL_TRIANGLE)) held |= BTN_ADVICE;
    }
    /* anti-mash: a button's press only counts if 30 ms have passed since its last accepted press
     * (also swallows contact bounce); holding never repeats - every action needs a new press */
    static SceInt64 last_press[16];
    SceInt64 now = sceKernelGetSystemTimeWide();
    unsigned edges = held & ~prev;
    in_pressed = 0;
    for (int i = 0; i < 16; i++) {
        unsigned bit = 1u << i;
        if ((edges & bit) && now - last_press[i] >= 30000) {
            in_pressed |= bit;
            last_press[i] = now;
        }
    }
    in_held = held;
    prev = held;
}

int main(int argc, char *argv[])
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
    save_read();
    gfx_init();
    char p[300];
    path_join(p, sizeof p, "HT.PAK");
    if (gfx_pak_open(p) < 0) {
        /* nothing we can draw without the pak: show a red screen */
        while (running) {
            gfx_begin(RGBA(120, 0, 0, 255));
            gfx_end();
        }
        return 0;
    }
    path_join(p, sizeof p, "MUSIC.PAK");
    audio_init(p);
    music_set_volume(g_save.music_vol);
    sfx_set_volume(g_save.sfx_vol);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    script_load();

    game_start(0);
    while (running) {
        read_input();
        game_frame();
        game_draw();
    }
    audio_shutdown();
    gfx_shutdown();
    sceKernelExitGame();
    return 0;
}
