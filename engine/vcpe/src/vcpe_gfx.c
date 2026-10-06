/* VCPE graphics core: GU setup, frames, texture bundles and pages, cached render state, primitives. */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <malloc.h>
#include <string.h>
#include "vcpe_pak.h"
#include "vcpe_gfx.h"

#define BUF_W 512
#define FB_SIZE (BUF_W * VCPE_SCR_H * 4)
#define MAX_BUNDLES 64

static unsigned int __attribute__((aligned(64))) dlist[512 * 1024 / 4];
static const VcpePage *pages;
static const VcpeBundle *bundles;
static int nbundles;
static uint8_t *bundle_mem[MAX_BUNDLES];
static int bound_page = -2, bound_mode = -1, bound_white = -1, bound_filter = -1;
int vcpe_stat_binds, vcpe_stat_quads;
float vcpe_stat_area;

int vcpe_gfx_init(void)
{
    void *draw_fb = (void *)0, *disp_fb = (void *)FB_SIZE;
    sceGuInit();
    sceGuStart(GU_DIRECT, dlist);
    sceGuDrawBuffer(GU_PSM_8888, draw_fb, BUF_W);
    sceGuDispBuffer(VCPE_SCR_W, VCPE_SCR_H, disp_fb, BUF_W);
    sceGuOffset(2048 - (VCPE_SCR_W / 2), 2048 - (VCPE_SCR_H / 2));
    sceGuViewport(2048, 2048, VCPE_SCR_W, VCPE_SCR_H);
    sceGuScissor(0, 0, VCPE_SCR_W, VCPE_SCR_H);
    sceGuEnable(GU_SCISSOR_TEST);
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_CULL_FACE);
    sceGuEnable(GU_TEXTURE_2D);
    sceGuEnable(GU_BLEND);
    sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    sceGuTexWrap(GU_CLAMP, GU_CLAMP);
    sceGuShadeModel(GU_SMOOTH);
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuDisplay(GU_TRUE);
    return 0;
}

void vcpe_gfx_shutdown(void)
{
    sceGuTerm();
    vcpe_pak_close();
}

void vcpe_gfx_tables(const VcpePage *p, const VcpeBundle *b, int nb)
{
    pages = p;
    bundles = b;
    nbundles = nb > MAX_BUNDLES ? MAX_BUNDLES : nb;
}

void vcpe_gfx_begin(uint32_t clear)
{
    sceGuStart(GU_DIRECT, dlist);
    sceGuClearColor(clear);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    bound_page = -2;
    bound_mode = -1;
    bound_white = -1;
    bound_filter = -1;
}

void vcpe_gfx_end(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

/* ------------------------------------------------------------------ bundles */
int vcpe_bundle_loaded(int b) { return b >= 0 && b < nbundles && bundle_mem[b] != 0; }
uint8_t *vcpe_bundle_mem(int b) { return b >= 0 && b < nbundles ? bundle_mem[b] : 0; }

int vcpe_bundle_load(int b)
{
    if (b < 0 || b >= nbundles)
        return -1;
    if (bundle_mem[b])
        return 0;
    bundle_mem[b] = vcpe_pak_read_alloc(bundles[b].off, bundles[b].size);
    return bundle_mem[b] ? 0 : -1;
}

void vcpe_bundle_unload(int b)
{
    if (b < 0 || b >= nbundles || !bundle_mem[b])
        return;
    sceGuSync(0, 0);
    free(bundle_mem[b]);
    bundle_mem[b] = 0;
    bound_page = -2;
}

void vcpe_bundles_keep(const uint8_t *list, int n)
{
    /* free first so the new pages fit */
    for (int b = 0; b < nbundles; b++) {
        int need = 0;
        for (int i = 0; i < n; i++)
            if (list[i] == b)
                need = 1;
        if (!need)
            vcpe_bundle_unload(b);
    }
    for (int i = 0; i < n; i++)
        vcpe_bundle_load(list[i]);
}

/* ------------------------------------------------------------------ state */
int vcpe_bind_page(int p)
{
    if (p == bound_page)
        return 1;
    vcpe_stat_binds++;
    const VcpePage *pg = &pages[p];
    uint8_t *base = bundle_mem[pg->bundle];
    if (!base)
        return 0;
    if (pg->psm == 4) {
        sceGuClutMode(GU_PSM_8888, 0, 0x0f, 0);
        sceGuClutLoad(2, base + pg->clut);
        sceGuTexMode(GU_PSM_T4, 0, 0, pg->swz);
    } else {
        sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
        sceGuClutLoad(32, base + pg->clut);
        sceGuTexMode(GU_PSM_T8, 0, 0, pg->swz);
    }
    sceGuTexImage(0, pg->w, pg->h, pg->w, base + pg->data);
    sceGuTexFlush();
    bound_page = p;
    return 1;
}

void vcpe_unbind(void) { bound_page = -2; }

void vcpe_blend(int additive)
{
    if (additive == bound_mode)
        return;
    if (additive)
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
    else
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    bound_mode = additive;
}

void vcpe_tex_white(int on)
{
    if (on == bound_white)
        return;
    sceGuTexFunc(on ? GU_TFX_ADD : GU_TFX_MODULATE, GU_TCC_RGBA);
    bound_white = on;
}

void vcpe_filter(int nearest)
{
    if (nearest == bound_filter)
        return;
    sceGuTexFilter(nearest ? GU_NEAREST : GU_LINEAR, nearest ? GU_NEAREST : GU_LINEAR);
    bound_filter = nearest;
}

/* ------------------------------------------------------------------ primitives */
void vcpe_quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, uint32_t color)
{
    if (x1 < x0) { float t = x0; x0 = x1; x1 = t; t = u0; u0 = u1; u1 = t; }
    if (y1 < y0) { float t = y0; y0 = y1; y1 = t; t = v0; v0 = v1; v1 = t; }
    if (x1 < 0 || y1 < 0 || x0 > VCPE_SCR_W || y0 > VCPE_SCR_H)
        return;
    vcpe_stat_quads++;
    vcpe_stat_area += (x1 - x0) * (y1 - y0);
    VcpeVtx *v = sceGuGetMemory(2 * sizeof(VcpeVtx));
    v[0].u = u0; v[0].v = v0; v[0].color = color; v[0].x = x0; v[0].y = y0; v[0].z = 0;
    v[1].u = u1; v[1].v = v1; v[1].color = color; v[1].x = x1; v[1].y = y1; v[1].z = 0;
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
}

void vcpe_fill(float x, float y, float w, float h, uint32_t color)
{
    if ((color >> 24) == 0)
        return;
    vcpe_blend(0);
    sceGuDisable(GU_TEXTURE_2D);
    VcpeCVtx *v = sceGuGetMemory(2 * sizeof(VcpeCVtx));
    v[0].color = v[1].color = color;
    v[0].x = x; v[0].y = y; v[0].z = 0;
    v[1].x = x + w; v[1].y = y + h; v[1].z = 0;
    sceGuDrawArray(GU_SPRITES, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
    sceGuEnable(GU_TEXTURE_2D);
}

void vcpe_strip(const float *xy, const uint32_t *colors, int n, int additive)
{
    if (n < 3)
        return;
    vcpe_blend(additive);
    sceGuDisable(GU_TEXTURE_2D);
    VcpeCVtx *v = sceGuGetMemory(n * sizeof(VcpeCVtx));
    for (int i = 0; i < n; i++) {
        v[i].color = colors[i];
        v[i].x = xy[i * 2];
        v[i].y = xy[i * 2 + 1];
        v[i].z = 0;
    }
    sceGuDrawArray(GU_TRIANGLE_STRIP, GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, n, 0, v);
    sceGuEnable(GU_TEXTURE_2D);
}

void vcpe_clip(int x, int y, int w, int h)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    sceGuScissor(x, y, x + w, y + h);
}

void vcpe_clip_reset(void) { sceGuScissor(0, 0, VCPE_SCR_W, VCPE_SCR_H); }
