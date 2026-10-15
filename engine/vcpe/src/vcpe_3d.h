/* VCPE 3D: perspective rendering on the GU. A 16-bit depth buffer in VRAM (only once a game asks for 3D),
 * a camera given like a Godot Camera3D (position, YXZ euler angles in degrees, vertical FOV), and indexed
 * meshes in world space: VcpeVtx vertices (float UV, ABGR colour, float position) with 16-bit indices.
 * Draw the 3D scene between vcpe_3d_begin() and vcpe_3d_end(), then 2D (HUD) as before. */
#ifndef VCPE_3D_H
#define VCPE_3D_H
#include <stdint.h>

void vcpe_3d_begin(const float pos[3], const float rot_deg[3], float fov_deg, float znear, float zfar);
void vcpe_3d_model(const float *m16);   /* row-major 4x4 model matrix for the next draws; 0 = identity */
void vcpe_3d_draw(int page, const void *verts, const uint16_t *idx, int nidx);   /* page < 0: untextured */
void vcpe_3d_fog(float start, float end, uint32_t color);
void vcpe_3d_add(float k);     /* next draws: added on top, k x their colour (a light layer over the same surfaces); vcpe_3d_alpha(1) ends it */
void vcpe_3d_alpha(float a);   /* next draws: whole-object opacity (fixed blend factors); 1 = opaque */                         /* end <= start: off */
void vcpe_3d_end(void);
/* the camera of the last vcpe_3d_begin: can it see a world-space sphere? (skip drawing what it cannot) */
int vcpe_3d_visible(const float center[3], float radius);
int vcpe_3d_project(const float p[3], float *sx, float *sy);   /* world point -> screen pixels of the last camera; 0: behind it */

extern int vcpe_stat_tris;   /* triangles submitted this frame */

#endif
