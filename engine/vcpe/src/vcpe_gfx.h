/* VCPE graphics core: GU setup, frames, texture bundles and pages (paletted, swizzled, from the
 * data PAK), cached render state, textured quads, fills, strips, clipping. Games draw their own
 * sprites and text on top of these. */
#ifndef VCPE_GFX_H
#define VCPE_GFX_H
#include <stdint.h>

#define VCPE_SCR_W 480
#define VCPE_SCR_H 272

/* the converters write these table layouts */
typedef struct { uint32_t off, size; uint16_t first_page, npages; } VcpeBundle;
typedef struct { uint16_t w, h; uint8_t psm, swz, bundle; uint32_t data, clut; } VcpePage;
typedef struct { float u, v; uint32_t color; float x, y, z; } VcpeVtx;
typedef struct { uint32_t color; float x, y, z; } VcpeCVtx;

int vcpe_gfx_init(void);
void vcpe_gfx_shutdown(void);
void vcpe_gfx_tables(const VcpePage *pages, const VcpeBundle *bundles, int nbundles);
void vcpe_gfx_begin(uint32_t clear);
void vcpe_gfx_end(void);

/* texture bundles: a group of pages loaded from the PAK together */
int vcpe_bundle_load(int b);
void vcpe_bundle_unload(int b);
int vcpe_bundle_loaded(int b);
uint8_t *vcpe_bundle_mem(int b);
void vcpe_bundles_keep(const uint8_t *list, int n);   /* unload the others first, then load these */

/* cached state */
int vcpe_bind_page(int page);   /* 0: its bundle is not loaded */
void vcpe_unbind(void);         /* the game bound a texture itself (fonts, LANG pages) */
void vcpe_blend(int additive);
void vcpe_tex_white(int on);    /* colour = vertex colour, alpha = texture alpha (font materials) */
void vcpe_filter(int nearest);

void vcpe_quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, uint32_t color);
void vcpe_fill(float x, float y, float w, float h, uint32_t color);
void vcpe_strip(const float *xy, const uint32_t *colors, int n, int additive);
void vcpe_clip(int x, int y, int w, int h);
void vcpe_clip_reset(void);

extern int vcpe_stat_binds, vcpe_stat_quads;   /* per-frame counters for the test builds' perf lines */
extern float vcpe_stat_area;

#endif
