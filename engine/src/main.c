/* Helltaker PSP - clean reimplementation driven by data converted from the user's own copy. */
#include <pspkernel.h>
#include "vcpe.h"
#include <pspdisplay.h>
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



#define SAVE_MAGIC 0x48545031 /* HTP1 */

static void save_read(void)
{
    char p[300];
    vcpe_path(p, sizeof p, "SETTINGS.BIN");
    memset(&g_save, 0, sizeof g_save);
    int fd = sceIoOpen(p, PSP_O_RDONLY, 0);
    int got = 0;
    if (fd >= 0) {
        got = sceIoRead(fd, &g_save, sizeof g_save);
        sceIoClose(fd);
    }
    /* saves from before Examtaker are 8 bytes (no dlc_progress); anything shorter is truncated */
    if (got < 8 || g_save.magic != SAVE_MAGIC) {
        memset(&g_save, 0, sizeof g_save);
        g_save.magic = SAVE_MAGIC;
        g_save.music_vol = 2;  /* PlayerPrefs defaults */
        g_save.sfx_vol = 3;
        g_save.chapter_reached = 0;
    }
}

void save_write(void)
{
    char p[300];
    vcpe_path(p, sizeof p, "SETTINGS.BIN");
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
int g_test_god;

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

/* Helltaker's own test commands (the engine handles wait / press / hold / rec / quit) */
static int test_command(const char *cmd, const char *arg, int num, int num2, const char *line, unsigned *held)
{
    (void)num2; (void)line; (void)held;
    if (!strcmp(cmd, "scene")) {
        scene_request(atoi(arg));
    } else if (!strcmp(cmd, "bossend")) {
        extern void boss_debug_end(void);
        boss_debug_end();
    } else if (!strcmp(cmd, "dlg")) {   /* dlg D E: open dialogue D at element E */
        extern int g_dlg_force_start;
        g_dlg_force_start = num;
        dlg_start(atoi(arg));
    } else if (!strcmp(cmd, "amd")) {   /* amd N: Manager.AMDphase (set before "scene 27") */
        extern int g_amd_phase;
        g_amd_phase = atoi(arg);
    } else if (!strcmp(cmd, "amdhit")) {   /* kick the AMD core (phase 3: the kill) */
        extern void amd_core_hit(void);
        amd_core_hit();
    } else if (!strcmp(cmd, "god")) {
        g_test_god = 1;   /* autotest only: hazards don't kill */
    } else if (!strcmp(cmd, "pos")) {
        puzzle_teleport(atoi(arg), num);
    } else {
        return VCPE_CMD_UNKNOWN;
    }
    return VCPE_CMD_FRAME;
}

static void script_load(void)
{
    static const VcpeTestHooks hooks = {btn_from, test_command, 0, 0, 1};
    vcpe_test_load(&hooks);
}

static void read_input(void);

/* language choice, only with a LANG.PAK: ENGLISH / the translation, D-pad or stick, X.
 * At start-up, and from the main menu with START (behind the closed door, scene.c) */
void lang_choose(void)
{
    int sel = gfx_lang_active() ? 1 : 0;   /* 0 English, 1 the translation; starts on the saved choice */
    float k[2] = {0, 0};
    gfx_bundle_load(0);
    gfx_lang_set(1);                       /* the translation's name is a LANG.PAK picture */
    for (;;) {
        read_input();
        if (in_pressed & (BTN_UP | BTN_DOWN | BTN_LEFT | BTN_RIGHT)) {
            sel ^= 1;
            sfx_play(SFX_button_menu_highlight_01, 0);
        }
        if (in_pressed & BTN_OK) {
            sfx_play(SFX_button_menu_confirm_01, 0);
            break;
        }
        gfx_begin(RGBA(2, 2, 28, 255));
        static const char *const label[2] = {"ENGLISH", "LANGUAGE NAME"};
        for (int i = 0; i < 2; i++) {
            float y = 112.0f + i * 38.0f;
            k[i] += ((i == sel ? 1.0f : 0.0f) - k[i]) * 0.4f;
            /* the pause menu's brackets, pushed out for wide names */
            float lw = gfx_text_width(FONT_MENU, label[i], 1.0f);
            float off = 25.0f - 6.9f * k[i] + (lw * 0.5f > 58.0f ? lw * 0.5f - 58.0f : 0.0f);
            uint32_t bc = i == sel ? RGBA(230, 77, 81, 255) : RGBA(101, 61, 72, 255);   /* pause menu colours */
            gfx_sprite(SPR_button_small, 240 - off, y + 1, 1.0f, 1.0f, bc);
            gfx_sprite(SPR_button_small, 240 + off, y + 1, -1.0f, 1.0f, bc);
            gfx_text_fit(FONT_MENU, 240, y - 11, label[i], i == sel ? RGBA(255, 255, 255, 255) : RGBA(212, 185, 191, 255), 220);
        }
        gfx_end();
    }
    gfx_lang_set(sel);
    g_save.lang_off = !sel;
    save_write();
}

static void read_input(void)
{
    static unsigned prev;
    unsigned held = 0;
    if (vcpe_autotest) {
        vcpe_test_step(&held);
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
    vcpe_sys_init(argc, argv);
    save_read();
    gfx_init();
    char p[300];
    vcpe_path(p, sizeof p, "HT.PAK");
    if (gfx_pak_open(p) < 0) {
        /* nothing we can draw without the pak: show a red screen */
        while (vcpe_running) {
            gfx_begin(RGBA(120, 0, 0, 255));
            gfx_end();
        }
        return 0;
    }
    vcpe_path(p, sizeof p, "LANG.PAK");
    if (gfx_lang_load(p) == 0)
        gfx_lang_set(!g_save.lang_off);
    vcpe_path(p, sizeof p, "MUSIC.PAK");
    audio_init(p);
    music_set_volume(g_save.music_vol);
    sfx_set_volume(g_save.sfx_vol);
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    script_load();
    if (gfx_lang_name())
        lang_choose();

    game_start(0);
    while (vcpe_running) {
        read_input();
        game_frame();
        game_draw();
        vcpe_test_frame_end();
    }
    audio_shutdown();
    gfx_shutdown();
    sceKernelExitGame();
    return 0;
}
