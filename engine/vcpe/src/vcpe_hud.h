/* VCPE HUD built-ins that need no game data:
 *   text in the PSP SDK's 8x8 font (debug lines, test builds, the perf overlay, early prototypes)
 *   the standard perf overlay players read their numbers from: FPS, update ms, draw ms (CPU build + GE wait),
 *     draw calls, quads, free memory
 *   PSP button icons drawn from shapes (cross, circle, square, triangle, L, R, START, SELECT, d-pad arrows) for
 *     prompts like "(X) JUMP", in the PlayStation colours, at any size
 * Everything draws through vcpe_gfx (batched when batching is on). */
#ifndef VCPE_HUD_H
#define VCPE_HUD_H
#include <stdint.h>

void vcpe_hud_text(float x, float y, const char *s, uint32_t color, float scale);   /* '\n' starts a new line */
void vcpe_hud_printf(float x, float y, uint32_t color, const char *fmt, ...) __attribute__((format(printf, 4, 5)));
float vcpe_hud_text_width(const char *s, float scale);                               /* widest line */
/* text with a 1-pixel dark shadow, readable on any background */
void vcpe_hud_text_shadow(float x, float y, const char *s, uint32_t color, float scale);

/* measure the game's update step: begin/end around it (several steps a frame add up) */
void vcpe_perf_update_begin(void);
void vcpe_perf_update_end(void);
extern int vcpe_stat_update_us;   /* smoothed update time per shown frame */
/* the overlay: one line at the bottom of the screen (hold SELECT to show it, by convention) */
void vcpe_hud_perf(void);

enum { VCPE_BTN_CROSS, VCPE_BTN_CIRCLE, VCPE_BTN_SQUARE, VCPE_BTN_TRIANGLE, VCPE_BTN_L, VCPE_BTN_R, VCPE_BTN_START,
       VCPE_BTN_SELECT, VCPE_BTN_UP, VCPE_BTN_DOWN, VCPE_BTN_LEFT, VCPE_BTN_RIGHT };
/* a button icon centred on (cx, cy), r = radius of the round icons (L/R/START/SELECT are badges as tall as 2r);
 * alpha 0..255; returns the icon's width */
float vcpe_hud_button(int btn, float cx, float cy, float r, int alpha);
float vcpe_hud_button_width(int btn, float r);   /* for laying icons out before drawing them */
/* icon then label, left-aligned at (x, y top), label in the 8x8 font; returns the total width */
float vcpe_hud_prompt(float x, float y, int btn, const char *label, uint32_t color);

#endif
