/* VCPE text: bitmap fonts baked by tools/vcpe_font.py (white glyphs with alpha on a T8 page), drawn as 2D
 * quads tinted by a colour. UTF-8 strings; max_chars limits how many characters show (typewriter text). */
#ifndef VCPE_FONT_H
#define VCPE_FONT_H
#include <stdint.h>

typedef struct { uint32_t cp; uint16_t u, v; uint8_t w, h; int8_t xoff, yoff; uint8_t adv; } VcpeGlyph;
typedef struct { int page; int height; int nglyphs; const VcpeGlyph *glyphs; } VcpeFont;   /* glyphs sorted by cp */

int vcpe_utf8_len(const char *s);                                             /* characters */
float vcpe_font_width(const VcpeFont *f, const char *s, int max_chars, float scale);
void vcpe_font_draw(const VcpeFont *f, float x, float y, const char *s, int max_chars, uint32_t color, float scale);
/* centred lines ('\n' separated) around (cx, cy) */
void vcpe_font_draw_centered(const VcpeFont *f, float cx, float cy, const char *s, int max_chars, uint32_t color, float scale);

/* text in 3D, like a Godot Label3D: glyphs in the XY plane of the row-major world matrix m (facing +Z),
 * vertically centred; size = metres per baked pixel; align 0 left, 1 centre, 2 right. Call inside vcpe_3d. */
void vcpe_font_draw3d(const VcpeFont *f, const float m[16], const char *s, float size, uint32_t color, int align);

#endif
