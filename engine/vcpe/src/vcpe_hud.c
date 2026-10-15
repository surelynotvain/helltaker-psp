/* VCPE HUD built-ins (see vcpe_hud.h). */
#include <pspkernel.h>
#include <pspgu.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vcpe_sys.h"
#include "vcpe_gfx.h"
#include "vcpe_math.h"
#include "vcpe_hud.h"

extern unsigned char msx[];   /* libpspdebug's 8x8 font, 8 bytes per character, bit 7 = left pixel */

static uint8_t __attribute__((aligned(16))) font_tex[128 * 128 / 2];   /* 16x16 cells of 8x8, T4 */
static uint32_t __attribute__((aligned(16))) font_clut[16] = {0x00FFFFFF, 0xFFFFFFFF};
static int font_ready;

static void font_init(void)
{
    for (int c = 0; c < 256; c++) {
        int cx = (c & 15) * 8, cy = (c >> 4) * 8;
        for (int y = 0; y < 8; y++) {
            uint8_t row = msx[c * 8 + y];
            for (int x = 0; x < 8; x++) {
                int px = cx + x, py = cy + y, on = (row >> (7 - x)) & 1;
                uint8_t *b = &font_tex[(py * 128 + px) >> 1];
                if (px & 1) *b = (uint8_t)((*b & 0x0F) | (on << 4));
                else *b = (uint8_t)((*b & 0xF0) | on);
            }
        }
    }
    sceKernelDcacheWritebackRange(font_tex, sizeof font_tex);
    sceKernelDcacheWritebackRange(font_clut, sizeof font_clut);
    font_ready = 1;
}

static void font_bind(void)
{
    if (!font_ready)
        font_init();
    vcpe_gfx_flush();
    vcpe_blend(0);
    vcpe_tex_white(0);
    vcpe_filter(1);
    sceGuClutMode(GU_PSM_8888, 0, 0x0f, 0);
    sceGuClutLoad(2, font_clut);
    sceGuTexMode(GU_PSM_T4, 0, 0, 0);
    sceGuTexImage(0, 128, 128, 128, font_tex);
    sceGuTexFlush();
    vcpe_unbind();
    vcpe_bind_epoch++;   /* other modules' cached texture/palette are no longer bound */
}

void vcpe_hud_text(float x, float y, const char *s, uint32_t color, float scale)
{
    if (!s || !*s || (color >> 24) == 0)
        return;
    font_bind();
    float cx = x, d = 8 * scale;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p == '\n') { cx = x; y += d; continue; }
        if (*p != ' ') {
            float u = (*p & 15) * 8, v = (*p >> 4) * 8;
            vcpe_quad(cx, y, cx + d, y + d, u, v, u + 8, v + 8, color);
        }
        cx += d;
    }
    vcpe_gfx_flush();
    vcpe_unbind();
}

void vcpe_hud_text_shadow(float x, float y, const char *s, uint32_t color, float scale)
{
    vcpe_hud_text(x + scale, y + scale, s, color & 0xFF000000u, scale);
    vcpe_hud_text(x, y, s, color, scale);
}

void vcpe_hud_printf(float x, float y, uint32_t color, const char *fmt, ...)
{
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    vcpe_hud_text(x, y, buf, color, 1);
}

float vcpe_hud_text_width(const char *s, float scale)
{
    int best = 0, n = 0;
    for (; s && *s; s++) {
        if (*s == '\n') n = 0;
        else if (++n > best) best = n;
    }
    return best * 8 * scale;
}

/* ------------------------------------------------------------------ perf overlay */
int vcpe_stat_update_us;
static SceInt64 upd_t0;
static int upd_acc, heap_kb, heap_age, last_quads;

void vcpe_perf_update_begin(void) { upd_t0 = sceKernelGetSystemTimeWide(); }
void vcpe_perf_update_end(void) { upd_acc += (int)(sceKernelGetSystemTimeWide() - upd_t0); }

void vcpe_hud_perf(void)
{
    /* smooth the update time over frames (this is called once per shown frame) */
    vcpe_stat_update_us += (upd_acc - vcpe_stat_update_us) / 8;
    upd_acc = 0;
    if (heap_age-- <= 0) {   /* the heap probe allocates: twice a second is plenty */
        heap_kb = vcpe_heap_kb();
        heap_age = 30;
    }
    /* games reset vcpe_stat_quads on their own schedules: show this frame's share */
    int q = vcpe_stat_quads - last_quads;
    if (q < 0) q = vcpe_stat_quads;
    last_quads = vcpe_stat_quads;
    char buf[96];
    snprintf(buf, sizeof buf, "%2d FPS  UPD %d.%d  DRAW %d.%d+%d.%d MS  %d CALLS %d Q  %dK", vcpe_stat_fps,
             vcpe_stat_update_us / 1000, vcpe_stat_update_us / 100 % 10, vcpe_stat_build_us / 1000,
             vcpe_stat_build_us / 100 % 10, vcpe_stat_gpu_wait_us / 1000, vcpe_stat_gpu_wait_us / 100 % 10,
             vcpe_stat_draws, q, heap_kb);
    vcpe_fill(0, VCPE_SCR_H - 11, VCPE_SCR_W, 11, 0xA0000000);
    vcpe_hud_text(3, VCPE_SCR_H - 9, buf, 0xFFFFFFFF, 1);
}

/* ------------------------------------------------------------------ button icons */
static void convex(const float *pts, int n, uint32_t color)
{
    /* convex polygon as a strip: 0, 1, n-1, 2, n-2, ... */
    float xy[64];
    uint32_t c[32];
    int k = 0, lo = 1, hi = n - 1;
    xy[0] = pts[0]; xy[1] = pts[1]; c[k++] = color;
    while (lo <= hi && k < 32) {
        xy[k * 2] = pts[lo * 2]; xy[k * 2 + 1] = pts[lo * 2 + 1]; c[k++] = color; lo++;
        if (lo > hi || k >= 32) break;
        xy[k * 2] = pts[hi * 2]; xy[k * 2 + 1] = pts[hi * 2 + 1]; c[k++] = color; hi--;
    }
    vcpe_strip(xy, c, k, 0);
}

static void disc(float cx, float cy, float r, uint32_t color)
{
    float p[48];
    int n = r < 6 ? 12 : r < 14 ? 18 : 24;
    for (int i = 0; i < n; i++) {
        p[i * 2] = cx + r * vcpe_cos_deg(i * 360.0f / n);
        p[i * 2 + 1] = cy + r * vcpe_sin_deg(i * 360.0f / n);
    }
    convex(p, n, color);
}

static void ring(float cx, float cy, float r, float w, uint32_t color)
{
    float xy[2 * 50];
    uint32_t c[50];
    int n = 24, k = 0;
    for (int i = 0; i <= n; i++) {
        float co = vcpe_cos_deg(i * 360.0f / n), si = vcpe_sin_deg(i * 360.0f / n);
        xy[k * 2] = cx + r * co; xy[k * 2 + 1] = cy + r * si; c[k++] = color;
        xy[k * 2] = cx + (r - w) * co; xy[k * 2 + 1] = cy + (r - w) * si; c[k++] = color;
    }
    vcpe_strip(xy, c, k, 0);
}

static void line(float x0, float y0, float x1, float y1, float w, uint32_t color)
{
    float dx = x1 - x0, dy = y1 - y0, l = vcpe_sqrt(dx * dx + dy * dy);
    if (l <= 0) return;
    float nx = -dy / l * w * 0.5f, ny = dx / l * w * 0.5f;
    float xy[8] = {x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny, x1 - nx, y1 - ny};
    uint32_t c[4] = {color, color, color, color};
    vcpe_strip(xy, c, 4, 0);
}

static uint32_t rgba(int r, int g, int b, int a) { return (uint32_t)r | (uint32_t)g << 8 | (uint32_t)b << 16 | (uint32_t)a << 24; }

static const char *badge_label(int btn)
{
    return btn == VCPE_BTN_L ? "L" : btn == VCPE_BTN_R ? "R" : btn == VCPE_BTN_START ? "START" : "SELECT";
}

static float badge_scale(float r) { return r >= 14 ? 2 : 1; }

float vcpe_hud_button_width(int btn, float r)
{
    if (btn >= VCPE_BTN_L && btn <= VCPE_BTN_SELECT)
        return vcpe_hud_text_width(badge_label(btn), badge_scale(r)) + 2 * r * 0.8f;
    return 2 * r;
}

static float badge(const char *label, float cx, float cy, float r, int alpha)
{
    float s = badge_scale(r), tw = vcpe_hud_text_width(label, s), w = tw + 2 * r * 0.8f, h = 2 * r;
    float x = cx - w / 2, y = cy - r;
    vcpe_fill(x + 1, y, w - 2, h, rgba(40, 40, 46, alpha));
    vcpe_fill(x, y + 1, w, h - 2, rgba(40, 40, 46, alpha));
    vcpe_fill(x + 1, y + 1, w - 2, 1, rgba(120, 120, 130, alpha));
    vcpe_hud_text(cx - tw / 2, cy - 4 * s, label, rgba(235, 235, 240, alpha), s);
    return w;
}

float vcpe_hud_button(int btn, float cx, float cy, float r, int alpha)
{
    if (alpha <= 0) return 0;
    if (alpha > 255) alpha = 255;
    vcpe_math_init();   /* the icons use the sin table (a no-op once it is filled) */
    switch (btn) {
    case VCPE_BTN_L: case VCPE_BTN_R: case VCPE_BTN_START: case VCPE_BTN_SELECT:
        return badge(badge_label(btn), cx, cy, r, alpha);
    }
    disc(cx, cy, r, rgba(32, 32, 38, alpha));
    float w = r * 0.22f + 0.6f, k = r * 0.48f;
    switch (btn) {
    case VCPE_BTN_CROSS: {
        uint32_t c = rgba(124, 178, 232, alpha);
        line(cx - k, cy - k, cx + k, cy + k, w, c);
        line(cx - k, cy + k, cx + k, cy - k, w, c);
        break;
    }
    case VCPE_BTN_CIRCLE: ring(cx, cy, r * 0.6f, w, rgba(255, 102, 102, alpha)); break;
    case VCPE_BTN_SQUARE: {
        uint32_t c = rgba(255, 105, 248, alpha);
        float a = r * 0.48f;
        vcpe_fill(cx - a, cy - a, 2 * a, w, c);
        vcpe_fill(cx - a, cy + a - w, 2 * a, w, c);
        vcpe_fill(cx - a, cy - a, w, 2 * a, c);
        vcpe_fill(cx + a - w, cy - a, w, 2 * a, c);
        break;
    }
    case VCPE_BTN_TRIANGLE: {
        uint32_t c = rgba(64, 226, 160, alpha);
        float t = r * 0.58f, x0 = cx, y0 = cy - t, x1 = cx - t * 0.87f, y1 = cy + t * 0.5f, x2 = cx + t * 0.87f;
        line(x0, y0, x1, y1, w, c);
        line(x1, y1, x2, y1, w, c);
        line(x2, y1, x0, y0, w, c);
        break;
    }
    default: {   /* d-pad arrows */
        static const float dir[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
        int d = btn - VCPE_BTN_UP;
        if (d < 0 || d > 3) break;
        float fx = dir[d][0], fy = dir[d][1], t = r * 0.55f;
        float p[6] = {cx + fx * t, cy + fy * t, cx - fy * t - fx * t * 0.5f, cy + fx * t - fy * t * 0.5f,
                      cx + fy * t - fx * t * 0.5f, cy - fx * t - fy * t * 0.5f};
        convex(p, 3, rgba(235, 235, 240, alpha));
    }
    }
    return 2 * r;
}

float vcpe_hud_prompt(float x, float y, int btn, const char *label, uint32_t color)
{
    float r = 6, w = vcpe_hud_button_width(btn, r);
    vcpe_hud_button(btn, x + w / 2, y + r, r, (int)(color >> 24));
    vcpe_hud_text_shadow(x + w + 4, y + r - 4, label, color, 1);
    return w + 4 + vcpe_hud_text_width(label, 1);
}
