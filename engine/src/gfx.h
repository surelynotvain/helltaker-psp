#ifndef GFX_H
#define GFX_H
#include <stdint.h>

#define SCR_W 480
#define SCR_H 272
/* Unity world units -> PSP pixels (ortho size 5.4 => 10.8 units over 272 lines) */
#define PXU (272.0f / 10.8f)

#define RGBA(r, g, b, a) ((uint32_t)(r) | ((uint32_t)(g) << 8) | ((uint32_t)(b) << 16) | ((uint32_t)(a) << 24))
#define WHITE RGBA(255, 255, 255, 255)
#define BG_COLOR RGBA(2, 2, 28, 255) /* camera clear colour (0.0078, 0.0078, 0.11) */

int gfx_init(void);
void gfx_shutdown(void);
void gfx_begin(uint32_t clear);
void gfx_end(void);

int gfx_pak_open(const char *path);
int gfx_bundle_load(int bundle);
void gfx_bundle_unload(int bundle);
void gfx_bundles_require(const uint8_t *list, int n);
int gfx_bundle_loaded(int bundle);
void *pak_read_alloc(uint32_t off, uint32_t size);
/* bumped by the power callback after a sleep/resume: Memory Stick handles opened before it go stale */
extern volatile int g_resume_gen;

/* Draw a sprite with its pivot at (x, y) in screen pixels. sx/sy are scale (negative = flip). */
void gfx_sprite(int spr, float x, float y, float sx, float sy, uint32_t color);
void gfx_sprite_rot(int spr, float x, float y, float sx, float sy, float deg, uint32_t color);
void gfx_sprite_rot90(int spr, float x, float y, float sx, float sy, uint32_t color);
void gfx_sprite_additive(int spr, float x, float y, float sx, float sy, uint32_t color);
/* Stretch a sprite's trimmed image into a screen rectangle. */
void gfx_sprite_rect(int spr, float x, float y, float w, float h, uint32_t color);
int gfx_sprite_w(int spr);
int gfx_sprite_h(int spr);
void gfx_clip(int x, int y, int w, int h);
void gfx_clip_reset(void);
void gfx_rect(float x, float y, float w, float h, uint32_t color);

/* world -> screen with the active camera */
void gfx_camera(float cx, float cy);
void gfx_camera_shake(float ox, float oy);
float gfx_wx(float wx);
float gfx_wy(float wy);

/* text (UTF-8). align: 0 left, 1 centre, 2 right. Returns width. */
float gfx_text(int font, float x, float y, const char *s, uint32_t color, int align, float scale);
float gfx_text_width(int font, const char *s, float scale);
int gfx_font_line(int font);
/* centred text shrunk to fit max_w */
void gfx_text_fit(int font, float cx, float y, const char *s, uint32_t color, float max_w);

#endif
