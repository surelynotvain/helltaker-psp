/* GU renderer: paletted (CLUT) swizzled texture pages streamed from HT.PAK. */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspiofilemgr.h>
#include <malloc.h>
#include <string.h>
#include <math.h>
#include "gfx.h"
#include "gamedata.h"

#define BUF_W 512
#define FB_SIZE (BUF_W * SCR_H * 4)

static unsigned int __attribute__((aligned(64))) dlist[256 * 1024 / 4];
static void *draw_fb, *disp_fb;

static int pak_fd = -1;
static uint8_t *bundle_mem[NUM_BUNDLES];
static uint8_t *font_mem;
static int bound_page = -2;
static int bound_mode = -1;  /* 0 normal alpha, 1 additive */
static float cam_x, cam_y, shake_x, shake_y;

typedef struct {
    float u, v;
    uint32_t color;
    float x, y, z;
} Vtx;

int gfx_init(void)
{
    draw_fb = (void *)0;
    disp_fb = (void *)FB_SIZE;
    sceGuInit();
    sceGuStart(GU_DIRECT, dlist);
    sceGuDrawBuffer(GU_PSM_8888, draw_fb, BUF_W);
    sceGuDispBuffer(SCR_W, SCR_H, disp_fb, BUF_W);
    sceGuOffset(2048 - (SCR_W / 2), 2048 - (SCR_H / 2));
    sceGuViewport(2048, 2048, SCR_W, SCR_H);
    sceGuScissor(0, 0, SCR_W, SCR_H);
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

void gfx_shutdown(void)
{
    sceGuTerm();
    if (pak_fd >= 0)
        sceIoClose(pak_fd);
}

void gfx_begin(uint32_t clear)
{
    sceGuStart(GU_DIRECT, dlist);
    sceGuClearColor(clear);
    sceGuClear(GU_COLOR_BUFFER_BIT);
    bound_page = -2;
    bound_mode = -1;
}

void gfx_end(void)
{
    sceGuFinish();
    sceGuSync(0, 0);
    sceDisplayWaitVblankStart();
    sceGuSwapBuffers();
}

int gfx_pak_open(const char *path)
{
    pak_fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (pak_fd < 0)
        return pak_fd;
    font_mem = pak_read_alloc(g_font_off, g_font_size);
    return font_mem ? 0 : -1;
}

void *pak_read_alloc(uint32_t off, uint32_t size)
{
    uint8_t *mem = memalign(64, size);
    if (!mem)
        return 0;
    sceIoLseek32(pak_fd, off, PSP_SEEK_SET);
    uint32_t got = 0;
    while (got < size) {
        int r = sceIoRead(pak_fd, mem + got, size - got);
        if (r <= 0)
            break;
        got += r;
    }
    if (got != size) {
        free(mem);
        return 0;
    }
    sceKernelDcacheWritebackRange(mem, size);
    return mem;
}

int gfx_bundle_loaded(int b) { return bundle_mem[b] != 0; }

int gfx_bundle_load(int b)
{
    if (bundle_mem[b])
        return 0;
    bundle_mem[b] = pak_read_alloc(g_bundles[b].off, g_bundles[b].size);
    return bundle_mem[b] ? 0 : -1;
}

void gfx_bundle_unload(int b)
{
    if (!bundle_mem[b])
        return;
    sceGuSync(0, 0);
    free(bundle_mem[b]);
    bundle_mem[b] = 0;
    bound_page = -2;
}

void gfx_bundles_require(const uint8_t *list, int n)
{
    /* free first so the new scene's pages fit */
    for (int b = 0; b < NUM_BUNDLES; b++) {
        int need = 0;
        for (int i = 0; i < n; i++)
            if (list[i] == b)
                need = 1;
        if (!need)
            gfx_bundle_unload(b);
    }
    for (int i = 0; i < n; i++)
        gfx_bundle_load(list[i]);
}

static int bind_page(int p)
{
    if (p == bound_page)
        return 1;
    const PageDef *pg = &g_pages[p];
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

static void set_blend(int additive)
{
    if (additive == bound_mode)
        return;
    if (additive)
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
    else
        sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    bound_mode = additive;
}

static void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, uint32_t color)
{
    if (x1 < x0) {
        float t = x0; x0 = x1; x1 = t;
        t = u0; u0 = u1; u1 = t;
    }
    if (y1 < y0) {
        float t = y0; y0 = y1; y1 = t;
        t = v0; v0 = v1; v1 = t;
    }
    if (x1 < 0 || y1 < 0 || x0 > SCR_W || y0 > SCR_H)
        return;
    Vtx *v = sceGuGetMemory(2 * sizeof(Vtx));
    v[0].u = u0; v[0].v = v0; v[0].color = color; v[0].x = x0; v[0].y = y0; v[0].z = 0;
    v[1].u = u1; v[1].v = v1; v[1].color = color; v[1].x = x1; v[1].y = y1; v[1].z = 0;
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
}

static void sprite_draw(int spr, float x, float y, float sx, float sy, uint32_t color, int additive)
{
    if (spr < 0 || spr >= NUM_SPRITES || (color >> 24) == 0)
        return;
    const SprDef *d = &g_sprites[spr];
    /* integer-snap unscaled sprites so the pre-scaled art stays crisp */
    int crisp = (sx == 1.0f || sx == -1.0f) && (sy == 1.0f || sy == -1.0f);
    if (crisp) {
        x = floorf(x + 0.5f);
        y = floorf(y + 0.5f);
    }
    set_blend(additive);
    sceGuTexFilter(crisp ? GU_NEAREST : GU_LINEAR, crisp ? GU_NEAREST : GU_LINEAR);
    for (int i = 0; i < d->n; i++) {
        const SprPiece *p = &g_pieces[d->first + i];
        if (!bind_page(p->page))
            return;
        float lx0 = d->ox + p->dx, ly0 = d->oy + p->dy;
        quad(x + lx0 * sx, y + ly0 * sy, x + (lx0 + p->w) * sx, y + (ly0 + p->h) * sy,
             p->u, p->v, p->u + p->w, p->v + p->h, color);
    }
}

void gfx_sprite(int spr, float x, float y, float sx, float sy, uint32_t color)
{
    sprite_draw(spr, x, y, sx, sy, color, 0);
}

/* rotated by deg (counter-clockwise, Unity euler z) around the pivot */
void gfx_sprite_rot(int spr, float x, float y, float sx, float sy, float deg, uint32_t color)
{
    if (spr < 0 || spr >= NUM_SPRITES || (color >> 24) == 0)
        return;
    const SprDef *d = &g_sprites[spr];
    float r = deg * 0.01745329f, c = cosf(r), sn = sinf(r);
    set_blend(0);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    for (int i = 0; i < d->n; i++) {
        const SprPiece *p = &g_pieces[d->first + i];
        if (!bind_page(p->page))
            return;
        float lx0 = (d->ox + p->dx) * sx, ly0 = (d->oy + p->dy) * sy;
        float lx1 = lx0 + p->w * sx, ly1 = ly0 + p->h * sy;
        Vtx *v = sceGuGetMemory(4 * sizeof(Vtx));
        float u0 = p->u, v0 = p->v, u1 = p->u + p->w, v1 = p->v + p->h;
        float px[4] = {lx0, lx1, lx0, lx1}, py[4] = {ly0, ly0, ly1, ly1};
        float uu[4] = {u0, u1, u0, u1}, vv[4] = {v0, v0, v1, v1};
        for (int k = 0; k < 4; k++) {
            v[k].u = uu[k]; v[k].v = vv[k]; v[k].color = color; v[k].z = 0;
            v[k].x = x + px[k] * c + py[k] * sn;
            v[k].y = y - px[k] * sn + py[k] * c;
        }
        sceGuDrawArray(GU_TRIANGLE_STRIP, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 4, 0, v);
    }
}

void gfx_sprite_rot90(int spr, float x, float y, float sx, float sy, uint32_t color)
{
    gfx_sprite_rot(spr, x, y, sx, sy, 90.0f, color);
}

void gfx_sprite_additive(int spr, float x, float y, float sx, float sy, uint32_t color)
{
    sprite_draw(spr, x, y, sx, sy, color, 1);
}

void gfx_sprite_rect(int spr, float x, float y, float w, float h, uint32_t color)
{
    /* UI Image: the full sprite rect (not just the trimmed pixels) fills (x,y,w,h) */
    if (spr < 0 || spr >= NUM_SPRITES)
        return;
    const SprDef *d = &g_sprites[spr];
    float rw = d->rw / 4.0f, rh = d->rh / 4.0f;
    float kx = w / rw, ky = h / rh;
    float bx = d->ox - d->rx / 4.0f, by = d->oy - d->ry / 4.0f;
    set_blend(0);
    sceGuTexFilter(GU_LINEAR, GU_LINEAR);
    for (int i = 0; i < d->n; i++) {
        const SprPiece *p = &g_pieces[d->first + i];
        if (!bind_page(p->page))
            return;
        quad(x + (bx + p->dx) * kx, y + (by + p->dy) * ky, x + (bx + p->dx + p->w) * kx, y + (by + p->dy + p->h) * ky,
             p->u, p->v, p->u + p->w, p->v + p->h, color);
    }
}

int gfx_sprite_w(int spr) { return (spr >= 0 && spr < NUM_SPRITES) ? g_sprites[spr].w : 0; }
int gfx_sprite_h(int spr) { return (spr >= 0 && spr < NUM_SPRITES) ? g_sprites[spr].h : 0; }

void gfx_rect(float x, float y, float w, float h, uint32_t color)
{
    if ((color >> 24) == 0)
        return;
    set_blend(0);
    sceGuDisable(GU_TEXTURE_2D);
    Vtx *v = sceGuGetMemory(2 * sizeof(Vtx));
    memset(v, 0, 2 * sizeof(Vtx));
    v[0].color = v[1].color = color;
    v[0].x = x; v[0].y = y;
    v[1].x = x + w; v[1].y = y + h;
    sceGuDrawArray(GU_SPRITES, GU_TEXTURE_32BITF | GU_COLOR_8888 | GU_VERTEX_32BITF | GU_TRANSFORM_2D, 2, 0, v);
    sceGuEnable(GU_TEXTURE_2D);
}

void gfx_clip(int x, int y, int w, int h)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    sceGuScissor(x, y, x + w, y + h);
}

void gfx_clip_reset(void) { sceGuScissor(0, 0, SCR_W, SCR_H); }

void gfx_camera(float cx, float cy) { cam_x = cx; cam_y = cy; }
void gfx_camera_shake(float ox, float oy) { shake_x = ox; shake_y = oy; }
float gfx_wx(float wx) { return SCR_W * 0.5f + (wx - cam_x - shake_x) * PXU; }
float gfx_wy(float wy) { return SCR_H * 0.5f - (wy - cam_y - shake_y) * PXU; }

/* ------------------------------------------------------------------ text */
static uint32_t utf8_next(const char **ps)
{
    const unsigned char *s = (const unsigned char *)*ps;
    uint32_t c = *s++;
    if (c >= 0xF0) {
        c = ((c & 7) << 18) | ((s[0] & 63) << 12) | ((s[1] & 63) << 6) | (s[2] & 63);
        s += 3;
    } else if (c >= 0xE0) {
        c = ((c & 15) << 12) | ((s[0] & 63) << 6) | (s[1] & 63);
        s += 2;
    } else if (c >= 0xC0) {
        c = ((c & 31) << 6) | (s[0] & 63);
        s += 1;
    }
    *ps = (const char *)s;
    return c;
}

static const Glyph *glyph(int font, uint32_t cp)
{
    const FontDef *f = &g_fonts[font];
    int lo = 0, hi = f->n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const Glyph *g = &g_glyphs[f->first + mid];
        if (g->cp == cp)
            return g;
        if (g->cp < cp)
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    if (cp != '?')
        return glyph(font, '?');
    return 0;
}

int gfx_font_line(int font) { return g_fonts[font].line; }

/* strings known at convert time are pre-rendered with real kerning (g_text_spr) */
static const TextSpr *text_find(int font, const char *s)
{
    uint32_t h = 2166136261u ^ (uint32_t)font;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        h = (h ^ *p) * 16777619u;
    int lo = 0, hi = NUM_TEXT_SPR - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const TextSpr *t = &g_text_spr[mid];
        if (t->hash == h && t->font == font)
            return gfx_bundle_loaded(g_sprites[t->spr].bundle) ? t : 0;
        if (t->hash < h || (t->hash == h && t->font < font))
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return 0;
}

float gfx_text_width(int font, const char *s, float scale)
{
    const TextSpr *ts = s ? text_find(font, s) : 0;
    if (ts)
        return ts->adv / 16.0f * scale;
    float w = 0;
    while (s && *s) {
        const Glyph *g = glyph(font, utf8_next(&s));
        if (g)
            w += g->adv / 16.0f;
    }
    return w * scale;
}

float gfx_text(int font, float x, float y, const char *s, uint32_t color, int align, float scale)
{
    if (!s || !font_mem)
        return 0;
    float w = gfx_text_width(font, s, scale);
    if (align == 1)
        x -= w * 0.5f;
    else if (align == 2)
        x -= w;
    const TextSpr *ts = text_find(font, s);
    if (ts) {
        if (scale == 1.0f) {
            x = floorf(x + 0.5f);
            y = floorf(y + 0.5f);
        }
        gfx_sprite(ts->spr, x, y, scale, scale, color);
        return w;
    }
    if (scale == 1.0f)
        x = floorf(x + 0.5f);
    y = floorf(y + 0.5f);
    float baseline = y + g_fonts[font].ascent * scale;
    set_blend(0);
    sceGuTexFilter(scale == 1.0f ? GU_NEAREST : GU_LINEAR, scale == 1.0f ? GU_NEAREST : GU_LINEAR);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuClutLoad(32, font_mem + g_font_clut);
    sceGuTexMode(GU_PSM_T8, 0, 0, 1);
    int cur = -1;
    bound_page = -2;
    while (*s) {
        const Glyph *g = glyph(font, utf8_next(&s));
        if (!g)
            continue;
        if (g->w) {
            if (g->page != cur) {
                const FontPage *fp = &g_font_pages[g->page];
                sceGuTexImage(0, fp->w, fp->h, fp->w, font_mem + fp->off);
                sceGuTexFlush();
                cur = g->page;
            }
            float gx = x + g->bx * scale, gy = baseline + g->by * scale;
            if (scale == 1.0f)
                gx = floorf(gx + 0.5f);
            quad(gx, gy, gx + g->w * scale, gy + g->h * scale, g->u, g->v, g->u + g->w, g->v + g->h, color);
        }
        x += g->adv / 16.0f * scale;
    }
    return w;
}

void gfx_text_fit(int font, float cx, float y, const char *s, uint32_t color, float max_w)
{
    float w = gfx_text_width(font, s, 1.0f);
    float sc = (w > max_w && w > 0) ? max_w / w : 1.0f;
    gfx_text(font, cx, y + (1.0f - sc) * g_fonts[font].ascent * 0.5f, s, color, 1, sc);
}
