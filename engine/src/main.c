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
#define SAVE_FILE "SETTINGS.BIN"
#define SAVE_VERSION 1

static void save_defaults(void)
{
    memset(&g_save, 0, sizeof g_save);
    g_save.magic = SAVE_MAGIC;
    g_save.music_vol = 2;  /* PlayerPrefs defaults */
    g_save.sfx_vol = 3;
    g_save.chapter_reached = 0;
}

/* VCPE checked two-slot save (SETTINGS.BIN + SETTINGS.BIN.BAK); a save from 2.0 or older is the plain struct in
 * SETTINGS.BIN, read once as a fallback so players keep their progress (the next save_write replaces it) */
static void save_read(void)
{
    if (vcpe_save_read(SAVE_FILE, &g_save, sizeof g_save, SAVE_VERSION) == 0 && g_save.magic == SAVE_MAGIC)
        return;
    char p[300];
    vcpe_path(p, sizeof p, SAVE_FILE);
    memset(&g_save, 0, sizeof g_save);
    int fd = sceIoOpen(p, PSP_O_RDONLY, 0);
    int got = 0;
    if (fd >= 0) {
        got = sceIoRead(fd, &g_save, sizeof g_save);
        sceIoClose(fd);
    }
    /* saves from before Examtaker are 8 bytes (no dlc_progress); anything shorter is truncated */
    if (got < 8 || g_save.magic != SAVE_MAGIC)
        save_defaults();
}

void save_write(void)
{
    vcpe_save_write(SAVE_FILE, &g_save, sizeof g_save, SAVE_VERSION);
}

/* ------------------------------------------------------------ autotest
 * If AUTOTEST.TXT sits next to the EBOOT, inputs come from it instead of the pad:
 *   scene N      jump to scene N
 *   wait N       N frames with no input
 *   press B      one-frame press (up/down/left/right/ok/back/start/r/l)
 *   hold B N     hold for N frames
 *   quit         exit (the emulator saves its screenshot)                         */
int g_test_god;

/* the PSP buttons behind each game button; the stick works as the d-pad (vcpe_pad, threshold 50/205) */
static const VcpePadBind binds[] = {
    {PSP_CTRL_UP, BTN_UP}, {PSP_CTRL_DOWN, BTN_DOWN}, {PSP_CTRL_LEFT, BTN_LEFT}, {PSP_CTRL_RIGHT, BTN_RIGHT},
    {PSP_CTRL_CROSS, BTN_OK}, {PSP_CTRL_CIRCLE, BTN_BACK}, {PSP_CTRL_START, BTN_START},
    {PSP_CTRL_RTRIGGER | PSP_CTRL_SQUARE, BTN_RESTART}, {PSP_CTRL_LTRIGGER | PSP_CTRL_TRIANGLE, BTN_ADVICE}};

/* Helltaker's own test commands (the engine handles wait / press / hold / rec / quit) */
extern int g_load_snap;   /* scene.c */
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
    } else if (!strcmp(cmd, "loadshow")) {
        vcpe_loading_show_after(atoi(arg));   /* emulator loads are fast: 0 shows the loading screen every time */
        return VCPE_CMD_NEXT;
    } else if (!strcmp(cmd, "loadsnap")) {
        g_load_snap = 1;
        return VCPE_CMD_NEXT;
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
    static const VcpeTestHooks hooks = {vcpe_pad_button, test_command, 0, 0, 1};
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
            float off = 25.0f + 6.9f * k[i] + (lw * 0.5f > 58.0f ? lw * 0.5f - 58.0f : 0.0f);
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

/* anti-mash: a button's press only counts if 30 ms have passed since its last accepted press (also swallows
 * contact bounce); holding never repeats - every action needs a new press */
static void read_input(void)
{
    in_held = vcpe_pad_update();
    in_pressed = vcpe_pad_pressed();
}

int main(int argc, char *argv[])
{
    vcpe_sys_init(argc, argv);
    int small = vcpe_mem_32mb();   /* measure the heap before anything is allocated */
    if (vcpe_autotest)
        vcpe_seed(1);   /* repeatable test runs */
    save_read();
    gfx_init();
    vcpe_gfx_pipeline(1);
    vcpe_gfx_batch(1);
    /* boot behind the VCPE loading screen: fonts, sound effects (the scene's pages load in game_start) */
    vcpe_loading_begin(g_ttf_size[0] + g_ttf_size[1] + g_ttf_size[2] + g_sfx_size);
    gfx_pak_open();
    vcpe_require_file("MUSIC.PAK", "Helltaker", "Copy MUSIC.PAK next to EBOOT.PBP (all files of the same build).");
    char p[300];
    vcpe_path(p, sizeof p, "MUSIC.PAK");
    SceIoStat st;
    if (sceIoGetstat(p, &st) < 0 || st.st_size != MUSIC_PAK_SIZE)
        vcpe_fatal("Helltaker", "MUSIC.PAK does not match this EBOOT.PBP (an incomplete copy, or a file from another "
                                "version).\n\nCopy EBOOT.PBP and all the PAK files again, all from the same build.");
    vcpe_path(p, sizeof p, "LANG.PAK");
    if (gfx_lang_load(p) == 0)
        gfx_lang_set(!g_save.lang_off);
    vcpe_path(p, sizeof p, "MUSIC.PAK");
    audio_init(p);
    vcpe_loading_end();
    music_set_volume(g_save.music_vol);
    sfx_set_volume(g_save.sfx_vol);
    vcpe_pad_init(binds, sizeof binds / sizeof binds[0], VCPE_STICK_DPAD);
    vcpe_pad_debounce(30);
    script_load();
    if (vcpe_autotest)
        printf("[ht] memory %s, heap free %d KB\n", small ? "32 MB (PSP-1000)" : "64 MB", vcpe_heap_kb());
    if (gfx_lang_name())
        lang_choose();

    game_start(0);
    while (vcpe_running) {
        read_input();
        /* hold SELECT: FPS, update and draw times (the numbers to report from real hardware) */
        g_perf = (vcpe_pad_psp() & PSP_CTRL_SELECT) != 0;
        if (g_perf)
            vcpe_perf_update_begin();
        game_frame();
        if (g_perf)
            vcpe_perf_update_end();
        game_draw();
        vcpe_test_frame_end();
    }
    audio_shutdown();
    gfx_shutdown();
    sceKernelExitGame();
    return 0;
}
