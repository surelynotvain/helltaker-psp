/* VCPE post effects: per-channel colour curves over the 3D picture (posterize, gamma, tint), done with the
 * GE's palette lookup since the PSP has no pixel shaders. Between vcpe_post_begin and vcpe_post_end the
 * frame is drawn into an off-screen VRAM buffer; vcpe_post_end reads it back as a 32-bit indexed texture
 * once per channel (the CLUT picks that channel's byte and maps it through the curve) and adds the three
 * results into the frame buffer. 2D drawn afterwards (UI, text) is not affected.
 * VRAM: the off-screen buffer follows the depth buffer of vcpe_3d (both frame buffers + depth + this = 1.86 MB). */
#ifndef VCPE_POST_H
#define VCPE_POST_H
#include <stdint.h>

void vcpe_post_curves(const uint8_t r[256], const uint8_t g[256], const uint8_t b[256]);
void vcpe_post_posterize(int levels);    /* curves: floor(v * levels) / levels on every channel */
void vcpe_post_grain(int amount);        /* film grain added before the curves (0..255, 0 = off); it dithers
                                          * posterized bands; the curves should expect values raised by amount / 2 */
void vcpe_post_dither(int amount);       /* like grain but a fixed 4x4 ordered (Bayer) pattern: still, PS1-like */
void vcpe_post_begin(void);              /* inside a frame, before the 3D: draw off-screen (cleared to black) */
void vcpe_post_end(void);                /* apply the curves into the frame buffer; drawing goes on there */
#endif
