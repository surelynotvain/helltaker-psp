/* VCPE loading screen (see vcpe_load.h). */
#include <pspkernel.h>
#include <pspgu.h>
#include <stdio.h>
#include <string.h>
#include "vcpe_sys.h"
#include "vcpe_test.h"
#include "vcpe_gfx.h"
#include "vcpe_pak.h"
#include "vcpe_hud.h"
#include "vcpe_load.h"
#include "vcpe_loadimg.h"   /* tools/mkloadimg.py: engineassets/wow.gif */

static int show_after_us = 150000;
#define REDRAW_US 33000

static int active, shown;
static uint32_t total, done;
static SceInt64 t_begin, t_last;
static void (*hook)(float progress, int ms);
static const char *word = "LOADING";

/* the boot log: the last steps reported (vcpe_step_*), newest at the bottom */
#define LOG_LINES 15
#define LOG_CHARS 58
typedef struct { int id, status; char text[LOG_CHARS + 1]; } LogLine;
static LogLine log_lines[LOG_LINES];
static int log_n;   /* lines used */
static int show_log = 1;

void vcpe_loading_show_log(int on) { show_log = on; }

static void log_print(const LogLine *l)
{
    static const char *tag[] = {"[ .... ]", "[  OK  ]", "[FAILED]", "[ INFO ]"};
    if (vcpe_autotest)
        printf("%s %s\n", tag[l->status], l->text);
}

static void step_hook(int id, int status, const char *text)
{
    if (text) {
        if (log_n == LOG_LINES) {
            memmove(log_lines, log_lines + 1, sizeof(LogLine) * (LOG_LINES - 1));
            log_n--;
        }
        LogLine *l = &log_lines[log_n++];
        l->id = id;
        l->status = status;
        snprintf(l->text, sizeof l->text, "%s", text);
        if (status != VCPE_STEP_RUN)
            log_print(l);
    } else {
        for (int i = log_n - 1; i >= 0; i--)
            if (log_lines[i].id == id) {
                log_lines[i].status = status;
                log_print(&log_lines[i]);
                break;
            }
    }
    vcpe_loading_tick();
}

/* what the engine is doing, one line per step ("Loading textures 3: 28 pages, 7196 KB"), top-left like a PC
 * booting. Plain text: the results only go to stdout in autotest runs */
void vcpe_loading_draw_log(int ms)
{
    (void)ms;
    for (int i = 0; i < log_n; i++)
        vcpe_hud_text(8, 8 + i * 9, log_lines[i].text, 0xFFC0C0C0, 1);
}

void vcpe_loading_draw_hook(void (*draw)(float progress, int ms)) { hook = draw; }
void vcpe_loading_text(const char *text) { word = text ? text : "LOADING"; }
int vcpe_loading_active(void) { return active; }
void vcpe_loading_show_after(int ms) { show_after_us = ms * 1000; }
int vcpe_loading_shown(void) { return shown; }

/* the engine's loading animation, size x size at (x, y), at ms into the loop */
void vcpe_loading_anim(float x, float y, float size, int ms)
{
    static int loop_ms;
    if (!loop_ms)
        for (int i = 0; i < VCPE_LOADIMG_FRAMES; i++)
            loop_ms += vcpe_loadimg_ms[i];
    int t = ms % loop_ms, f = 0;
    while (f < VCPE_LOADIMG_FRAMES - 1 && t >= vcpe_loadimg_ms[f])
        t -= vcpe_loadimg_ms[f++];
    vcpe_gfx_flush();
    vcpe_blend(0);
    vcpe_tex_white(0);
    vcpe_filter(1);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuClutLoad(32, vcpe_loadimg_clut);
    sceGuTexMode(GU_PSM_T8, 0, 0, 0);
    sceGuTexImage(0, 128, 128, VCPE_LOADIMG_SIZE, vcpe_loadimg + f * VCPE_LOADIMG_SIZE * VCPE_LOADIMG_SIZE);
    sceGuTexFlush();
    vcpe_unbind();
    vcpe_bind_epoch++;
    vcpe_quad(x, y, x + size, y + size, 0, 0, VCPE_LOADIMG_SIZE, VCPE_LOADIMG_SIZE, 0xFFFFFFFF);
    vcpe_gfx_flush();
    vcpe_unbind();
}

/* the standard screen: the boot log top-left; the animation in the bottom-right corner with the word and the
 * progress bar just above it */
void vcpe_loading_draw_standard(float p, int ms)
{
    const float m = 12, sz = VCPE_LOADIMG_SIZE;
    float ax = VCPE_SCR_W - m - sz, ay = VCPE_SCR_H - m - sz;
    if (show_log)
        vcpe_loading_draw_log(ms);
    vcpe_loading_anim(ax, ay, sz, ms);
    char s[40];
    int dots = (ms / 300) % 4;
    snprintf(s, sizeof s, "%s%.*s", word, dots, "...");
    float y = ay - (p >= 0 ? 16 : 12);
    vcpe_hud_text(ax, y, s, 0xFFC0C0C0, 1);
    if (p >= 0) {
        vcpe_fill(ax, ay - 6, sz, 2, 0xFF303030);
        vcpe_fill(ax, ay - 6, sz * (p > 1 ? 1 : p), 2, 0xFFC0C0C0);
    }
}

static void draw(SceInt64 now)
{
    float p = total ? (float)done / (float)total : -1.0f;
    int ms = (int)((now - t_begin) / 1000);
    vcpe_gfx_begin(0xFF000000);
    if (hook)
        hook(p, ms);
    else
        vcpe_loading_draw_standard(p, ms);
    vcpe_gfx_end();
    vcpe_gfx_present();
    shown++;
    t_last = now;
}

void vcpe_loading_tick(void)
{
    if (!active || vcpe_gfx_in_frame())
        return;
    SceInt64 now = sceKernelGetSystemTimeWide();
    if (now - t_begin >= show_after_us && (!shown || now - t_last >= REDRAW_US))
        draw(now);
}

static void pak_progress(uint32_t bytes)
{
    done += bytes;
    vcpe_loading_tick();
}

void vcpe_loading_add(uint32_t bytes)
{
    done += bytes;
    vcpe_loading_tick();
}

void vcpe_loading_begin(uint32_t total_bytes)
{
    active = 1;
    shown = 0;
    total = total_bytes;
    done = 0;
    t_begin = sceKernelGetSystemTimeWide();
    log_n = 0;
    vcpe_pak_progress = pak_progress;
    vcpe_step_hook = step_hook;
}

void vcpe_loading_end(void)
{
    if (!active)
        return;
    /* a screen that was shown ends on its last state: a full bar, every step's result */
    if (shown) {
        if (total)
            done = total;
        draw(sceKernelGetSystemTimeWide());
    }
    active = 0;
    vcpe_pak_progress = 0;
    vcpe_step_hook = 0;
}
