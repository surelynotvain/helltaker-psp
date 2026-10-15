/* VCPE TrueType text: the PSP rasterizes TrueType/OpenType (glyf) fonts itself at runtime, so text is laid out the
 * way a desktop does it instead of being baked into pictures on the PC:
 *   no hinting (letters keep the font's real shapes and spacing at small sizes)
 *   pen positions in quarter pixels (each glyph is rasterized at the sub-pixel offset it lands on)
 *   kerning from the font's GPOS/kern tables
 *   any string at any pixel size, including translations typed in later
 * Glyphs are cached as white-with-alpha T8 texels on two 512x512 pages and drawn as tinted quads through vcpe_gfx
 * (batched when batching is on). A page is recycled only when the frame being built has not used it.
 *
 *   VcpeTtf *f = vcpe_ttf_open(font_file_bytes, size);      // the bytes stay owned by the game, keep them
 *   inside vcpe_gfx_begin/end: vcpe_ttf_draw(f, 13, x, baseline_y, "Hello", color);
 *
 * Built on stb_truetype (public domain, shipped with the pspdev toolchain). No GSUB: ligatures are not formed. */
#ifndef VCPE_TTF_H
#define VCPE_TTF_H
#include <stdint.h>

typedef struct VcpeTtf VcpeTtf;

/* data: a .ttf/.otf (glyf outlines) in memory, kept alive by the caller. 0 if it is not a usable font */
VcpeTtf *vcpe_ttf_open(const void *data, uint32_t size);
/* pixel metrics for an em size of px pixels (as FreeType's FT_Set_Pixel_Sizes): ascent rounded up the way FreeType
 * reports it, descent (negative), line height */
float vcpe_ttf_ascent(const VcpeTtf *f, float px);
float vcpe_ttf_descent(const VcpeTtf *f, float px);
float vcpe_ttf_line(const VcpeTtf *f, float px);
int vcpe_ttf_has(const VcpeTtf *f, uint32_t codepoint);
/* advance width of a UTF-8 string with kerning ('\n' not handled: one line) */
float vcpe_ttf_width(VcpeTtf *f, float px, const char *s);
/* draw one line; (x, y) = pen start on the baseline. Returns the advance width. Missing characters draw as '?' */
float vcpe_ttf_draw(VcpeTtf *f, float px, float x, float y, const char *s, uint32_t color);
/* forget every cached glyph (both pages are cleared at the next use) */
void vcpe_ttf_cache_reset(void);

extern int vcpe_stat_ttf_raster;   /* glyphs rasterized since the last vcpe_ttf_stat_reset (cache misses) */
void vcpe_ttf_stat_reset(void);

#endif
