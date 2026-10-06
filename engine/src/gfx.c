/* GU renderer: paletted (CLUT) swizzled texture pages streamed from HT.PAK. */
#include <pspkernel.h>
#include <pspdisplay.h>
#include <pspgu.h>
#include <pspiofilemgr.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include "gfx.h"
#include "vcpe.h"
#include "gamedata.h"



static uint8_t *font_mem;
static float cam_x, cam_y, shake_x, shake_y;


int gfx_init(void)
{
    vcpe_gfx_tables((const VcpePage *)g_pages, (const VcpeBundle *)g_bundles, NUM_BUNDLES);
    return vcpe_gfx_init();
}

void gfx_shutdown(void) { vcpe_gfx_shutdown(); }

void gfx_begin(uint32_t clear) { vcpe_gfx_begin(clear); }

void gfx_end(void) { vcpe_gfx_end(); }


int gfx_pak_open(const char *path)
{
    int fd = vcpe_pak_open(path);
    if (fd < 0)
        return fd;
    font_mem = vcpe_pak_read_alloc(g_font_off, g_font_size);
    return font_mem ? 0 : -1;
}


int gfx_bundle_loaded(int b) { return vcpe_bundle_loaded(b); }
int gfx_bundle_load(int b) { return vcpe_bundle_load(b); }
void gfx_bundle_unload(int b) { vcpe_bundle_unload(b); }
void gfx_bundles_require(const uint8_t *list, int n) { vcpe_bundles_keep(list, n); }

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
    vcpe_blend(additive);
    vcpe_filter(crisp);
    for (int i = 0; i < d->n; i++) {
        const SprPiece *p = &g_pieces[d->first + i];
        if (!vcpe_bind_page(p->page))
            return;
        float lx0 = d->ox + p->dx, ly0 = d->oy + p->dy;
        vcpe_quad(x + lx0 * sx, y + ly0 * sy, x + (lx0 + p->w) * sx, y + (ly0 + p->h) * sy,
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
    vcpe_blend(0);
    vcpe_filter(0);
    for (int i = 0; i < d->n; i++) {
        const SprPiece *p = &g_pieces[d->first + i];
        if (!vcpe_bind_page(p->page))
            return;
        float lx0 = (d->ox + p->dx) * sx, ly0 = (d->oy + p->dy) * sy;
        float lx1 = lx0 + p->w * sx, ly1 = ly0 + p->h * sy;
        VcpeVtx *v = sceGuGetMemory(4 * sizeof(VcpeVtx));
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

void gfx_silhouette(int on)
{
    vcpe_tex_white(on);
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
    vcpe_blend(0);
    vcpe_filter(0);
    for (int i = 0; i < d->n; i++) {
        const SprPiece *p = &g_pieces[d->first + i];
        if (!vcpe_bind_page(p->page))
            return;
        vcpe_quad(x + (bx + p->dx) * kx, y + (by + p->dy) * ky, x + (bx + p->dx + p->w) * kx, y + (by + p->dy + p->h) * ky,
             p->u, p->v, p->u + p->w, p->v + p->h, color);
    }
}

int gfx_sprite_w(int spr) { return (spr >= 0 && spr < NUM_SPRITES) ? g_sprites[spr].w : 0; }
int gfx_sprite_h(int spr) { return (spr >= 0 && spr < NUM_SPRITES) ? g_sprites[spr].h : 0; }

void gfx_rect(float x, float y, float w, float h, uint32_t color) { vcpe_fill(x, y, w, h, color); }

void gfx_clip(int x, int y, int w, int h) { vcpe_clip(x, y, w, h); }

void gfx_clip_reset(void) { vcpe_clip_reset(); }

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

/* ------------------------------------------------------------ LANG.PAK (translations, tools/mklang.py)
 * pictures of the translated texts under the same keys as g_text_spr (FNV-1a of font + English string) */
typedef struct { char magic[4]; uint32_t version, kit, nent, npages, ent, pages; char name[32]; } LangHdr;
typedef struct { uint32_t hash; uint8_t font, page; uint16_t u, v, w, h; int16_t ox, oy; uint16_t adv; } LangEnt;
typedef struct { uint16_t w, h; uint32_t data, clut; } LangPage;
static uint8_t *lang_mem;
static const LangEnt *lang_ent;
static const LangPage *lang_pages;
static uint32_t lang_n;
static int lang_on;

int gfx_lang_load(const char *path)
{
    SceUID fd = sceIoOpen(path, PSP_O_RDONLY, 0);
    if (fd < 0)
        return -1;
    int n = sceIoLseek32(fd, 0, PSP_SEEK_END);
    sceIoLseek32(fd, 0, PSP_SEEK_SET);
    uint8_t *m = n > (int)sizeof(LangHdr) ? memalign(64, n) : 0;
    int got = 0, r;
    while (m && got < n && (r = sceIoRead(fd, m + got, n - got)) > 0)
        got += r;
    sceIoClose(fd);
    const LangHdr *h = (const LangHdr *)m;
    if (!m || got != n || memcmp(h->magic, "HTLG", 4) || h->version != 1 || h->kit != LANG_KIT ||
        h->ent + h->nent * sizeof(LangEnt) > (uint32_t)n || h->pages + h->npages * sizeof(LangPage) > (uint32_t)n) {
        free(m);
        return -1;
    }
    sceKernelDcacheWritebackRange(m, n);
    lang_mem = m;
    lang_ent = (const LangEnt *)(m + h->ent);
    lang_pages = (const LangPage *)(m + h->pages);
    lang_n = h->nent;
    lang_on = 1;
    return 0;
}

/* the translation's name, or 0 without a usable LANG.PAK */
const char *gfx_lang_name(void) { return lang_mem ? ((const LangHdr *)lang_mem)->name : 0; }
void gfx_lang_set(int on) { lang_on = on && lang_mem; }
int gfx_lang_active(void) { return lang_on; }

static const LangEnt *lang_find(int font, uint32_t h)
{
    int lo = 0, hi = (int)lang_n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        const LangEnt *e = &lang_ent[mid];
        if (e->hash == h && e->font == font)
            return e;
        if (e->hash < h || (e->hash == h && e->font < font))
            lo = mid + 1;
        else
            hi = mid - 1;
    }
    return 0;
}

static void lang_draw(const LangEnt *e, float x, float y, float sc, uint32_t color)
{
    const LangPage *pg = &lang_pages[e->page];
    vcpe_blend(0);
    vcpe_filter(sc == 1.0f);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuClutLoad(32, lang_mem + pg->clut);
    sceGuTexMode(GU_PSM_T8, 0, 0, 1);
    sceGuTexImage(0, pg->w, pg->h, pg->w, lang_mem + pg->data);
    sceGuTexFlush();
    vcpe_unbind();
    vcpe_quad(x + e->ox * sc, y + e->oy * sc, x + (e->ox + e->w) * sc, y + (e->oy + e->h) * sc,
         e->u, e->v, e->u + e->w, e->v + e->h, color);
}

/* strings known at convert time are pre-rendered with real kerning (g_text_spr, or the LANG.PAK picture) */
static const TextSpr *text_find_lang(int font, const char *s, const LangEnt **le)
{
    uint32_t h = 2166136261u ^ (uint32_t)font;
    for (const unsigned char *p = (const unsigned char *)s; *p; p++)
        h = (h ^ *p) * 16777619u;
    *le = lang_on ? lang_find(font, h) : 0;
    if (*le)
        return 0;
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
    const LangEnt *le = 0;
    const TextSpr *ts = s ? text_find_lang(font, s, &le) : 0;
    if (le)
        return le->adv / 16.0f * scale;
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
    const LangEnt *le;
    const TextSpr *ts = text_find_lang(font, s, &le);
    if (le) {
        if (scale == 1.0f) {
            x = floorf(x + 0.5f);
            y = floorf(y + 0.5f);
        }
        lang_draw(le, x, y, scale, color);
        return w;
    }
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
    vcpe_blend(0);
    vcpe_filter(scale == 1.0f);
    sceGuClutMode(GU_PSM_8888, 0, 0xff, 0);
    sceGuClutLoad(32, font_mem + g_font_clut);
    sceGuTexMode(GU_PSM_T8, 0, 0, 1);
    int cur = -1;
    vcpe_unbind();
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
            vcpe_quad(gx, gy, gx + g->w * scale, gy + g->h * scale, g->u, g->v, g->u + g->w, g->v + g->h, color);
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
