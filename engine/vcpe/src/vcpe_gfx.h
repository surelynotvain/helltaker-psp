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
/* psm: 4/8 indexed, 16 RGBA4444, 32 RGBA8888. Direct-colour pages do not use clut.
 * nmips: smaller levels after data (each half the size, 16-byte aligned). It sits in what was the padding byte, so
 * the struct keeps its 16 bytes and older page tables (cast from their own 7-field structs) read nmips 0 */
typedef struct { uint16_t w, h; uint8_t psm, swz, bundle, nmips; uint32_t data, clut; } VcpePage;
typedef struct { float u, v; uint32_t color; float x, y, z; } VcpeVtx;
typedef struct { uint32_t color; float x, y, z; } VcpeCVtx;

int vcpe_gfx_init(void);
void vcpe_gfx_shutdown(void);
void vcpe_gfx_tables(const VcpePage *pages, const VcpeBundle *bundles, int nbundles);
void vcpe_gfx_begin(uint32_t clear);
void vcpe_gfx_end(void);
void vcpe_gfx_vsync(int on);
int vcpe_gfx_in_frame(void);   /* between vcpe_gfx_begin and vcpe_gfx_end */   /* 0: vcpe_gfx_end swaps without waiting for the vblank (benchmarks; tears) */

/* Opt-in speed-ups (off by default, so games built before them draw exactly as they did).
 *
 * vcpe_gfx_batch(1): consecutive vcpe_quad calls (sprites, font glyphs) and consecutive vcpe_fill calls are sent to
 *   the GE as one draw each instead of one per rectangle. The picture is the same. Engine calls flush the batch
 *   themselves; a game that issues raw sceGu* commands between engine draws calls vcpe_gfx_flush() first.
 *
 * vcpe_gfx_pipeline(1): vcpe_gfx_end hands the frame to the GE and returns at once, so the game's next update runs
 *   while the GE draws; the next vcpe_gfx_begin waits for the GE, the vblank and swaps. Rules for the game:
 *   memory the GE reads for a frame (vertex arrays passed by pointer, palettes, textures) must not change or be
 *   freed before the next vcpe_gfx_begin - or call vcpe_gfx_gpu_wait() first. Engine-managed memory (bundles,
 *   vcpe_spr pages, sceGuGetMemory data) is already safe. Autotest runs present every frame before the script reads
 *   input, so recordings and snapshots stay frame-exact. */
void vcpe_gfx_batch(int on);
void vcpe_gfx_pipeline(int on);
void vcpe_gfx_flush(void);      /* send the pending batch now (before raw sceGu* drawing) */
void vcpe_gfx_gpu_wait(void);   /* wait until the GE has finished the frame handed over (pipeline), no swap */
void vcpe_gfx_present(void);    /* pipeline: finish the frame handed over now (GE, vblank, swap); else nothing */
int vcpe_gfx_pending(void);     /* a frame is handed over but not shown yet */

/* texture bundles: a group of pages loaded from the PAK together */
int vcpe_bundle_load(int b);
void vcpe_bundle_unload(int b);
int vcpe_bundle_loaded(int b);
uint8_t *vcpe_bundle_mem(int b);
/* a bundle the game already holds in memory (data linked into the EBOOT): 64-byte aligned, written back from the
 * data cache, never freed by the engine */
void vcpe_bundle_attach(int b, uint8_t *mem);
void vcpe_bundles_keep(const uint8_t *list, int n);   /* unload the others first, then load these */
/* 3.4, memory for 32 MB consoles (vcpe_mem.h):
 * vcpe_bundle_slab(first, last): bundles first..last share one block, the size of the largest, allocated now and
 *   reused: loading one of them unloads the one in the block. Scene bundles loaded this way never fragment the heap.
 * vcpe_bundle_load_at(b, mem, cap, reload): read bundle b into memory the game provides (64-byte aligned, at least
 *   the bundle's size, never freed by the engine). reload 1: read it again after a sleep/resume (the volatile
 *   partition from vcpe_volatile_mem()). -1 when it does not fit or the read fails */
int vcpe_bundle_slab(int first, int last);
int vcpe_bundle_load_at(int b, void *mem, uint32_t cap, int reload);

/* cached state */
int vcpe_bind_page(int page);   /* 0: its bundle is not loaded */
void vcpe_page_size(int page, int *w, int *h);
uint32_t vcpe_gfx_draw_offset(void);     /* VRAM offset of this frame's draw buffer (512-wide, 8888) */
void vcpe_gfx_state_reset(void);         /* forget the cached GE state (after drawing with raw sceGu calls) */
extern int vcpe_bind_epoch;     /* changes whenever vcpe_bind_page binds; modules binding their own textures compare it */
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
extern int vcpe_stat_frame_us;   /* last frame: begin until the GPU finished, without the vblank wait */
extern int vcpe_stat_build_us;   /* last frame: vcpe_gfx_begin until vcpe_gfx_end (CPU building the display list) */
extern int vcpe_stat_gpu_wait_us; /* last frame: time spent waiting for the GE after the list was built */
extern uint32_t vcpe_gfx_frame;  /* counts vcpe_gfx_begin calls */
extern int vcpe_stat_draws;      /* GE draw calls this frame (a batch counts once) */
extern int vcpe_stat_fps;        /* frames shown per second, updated twice a second */

#endif
