/* VCPE 3D scene: a Godot-like node tree (local position / YXZ euler rotation / scale, visibility, render layers,
 * meshes) and AnimationPlayers that play converted Godot animations (value, method and audio tracks).
 * The converter writes the tables below; the game hands them to vcpe_node3d_load() and reacts to method and
 * audio keys through callbacks. */
#ifndef VCPE_NODE3D_H
#define VCPE_NODE3D_H
#include <stdint.h>

typedef struct { int16_t parent; int16_t mesh; int16_t base; uint8_t visible, transparency; uint32_t layers, name;
                 float pos[3], rot[3], scale[3]; } VcpeNodeDef;   /* base: index of a fixed parent matrix (roots), or -1; transparency: GeometryInstance3D.transparency x 255 */
typedef struct { uint16_t page; uint32_t voff, nverts, ioff, nidx; } VcpeBatch;
typedef struct { uint16_t first, n; float bound[4]; } VcpeMeshDef;   /* batches g_mbatches[first .. first+n); bounding sphere (local centre, radius) */
enum { VCPE_TR_POS, VCPE_TR_ROT, VCPE_TR_SCALE, VCPE_TR_VISIBLE, VCPE_TR_FOV, VCPE_TR_METHOD, VCPE_TR_AUDIO, VCPE_TR_TRANSPARENCY };
enum { VCPE_INTERP_NEAREST, VCPE_INTERP_LINEAR, VCPE_INTERP_CUBIC };
typedef struct { uint8_t kind, interp, discrete, pad; int16_t node; uint16_t nkeys; uint32_t first; } VcpeTrackDef;
typedef struct { float t, trans, v[3]; } VcpeKeyDef;              /* method / audio keys: v[0] = method or sound id */
typedef struct { uint32_t name; float length; uint8_t loop; uint16_t first, n; } VcpeClipDef;   /* tracks */
typedef struct { uint32_t name; uint16_t first, n; int16_t autoplay; } VcpePlayerDef;            /* clips */

typedef struct {
    const VcpeNodeDef *nodes; int nnodes;
    const float (*bases)[16];                 /* row-major 4x4, world matrices of the static parents of roots */
    const VcpeMeshDef *meshes; const VcpeBatch *batches; const uint8_t *geo; uint32_t geo_off;   /* geo: PAK bytes from geo_off */
    const VcpePlayerDef *players; int nplayers;
    const VcpeClipDef *clips; const VcpeTrackDef *tracks; const VcpeKeyDef *keys;
    void (*on_method)(int player, int node, int method);   /* method track key reached (node: the track's target, or -1) */
    void (*on_audio)(int player, int sound, float from);   /* audio track key reached (from: start offset, s) */
} VcpeScene3D;

void vcpe_node3d_load(const VcpeScene3D *s);
int vcpe_node3d_find(uint32_t name_hash);            /* node index by its path hash, or -1 */
void vcpe_node3d_set_visible(int node, int on);
int vcpe_node3d_visible(int node);                   /* own flag */
int vcpe_node3d_shown(int node);                     /* it and all its parents visible */
int vcpe_node3d_parent(int node);                    /* current parent, or -1 */
uint32_t vcpe_node3d_layers(int node);
float vcpe_node3d_opacity(int node);                  /* 1 - transparency */
void vcpe_node3d_set_layers(int node, uint32_t layers);
void vcpe_node3d_set_mesh(int node, int mesh);       /* swap the mesh (e.g. a material variant); -1: none */
void vcpe_node3d_set_transparency(int node, float t);   /* GeometryInstance3D.transparency 0..1 */
/* move a node under another parent keeping its world transform (Godot reparent / remove_child + add_child with
 * global_transform restored). The new parent must come before the node in the table (parents are updated first). */
void vcpe_node3d_set_parent(int node, int parent);
float *vcpe_node3d_pos(int node);                    /* writable local position / rotation (radians) / scale */
float *vcpe_node3d_rot(int node);
float *vcpe_node3d_scale(int node);
void vcpe_node3d_world(int node, float out[16]);     /* world matrix (after vcpe_node3d_update) */
void vcpe_node3d_refresh(void);                 /* update changed transforms without advancing animation */
void vcpe_node3d_update(float dt);                   /* advance the players, then the world matrices */
void vcpe_node3d_draw(uint32_t cull_mask);           /* every visible mesh node on those layers (inside vcpe_3d) */

int vcpe_anim_find(uint32_t player_name_hash);
void vcpe_anim_play(int player, uint32_t clip_name_hash);   /* from the start, like AnimationPlayer.play() */
void vcpe_anim_stop(int player);
int vcpe_anim_playing(int player);
float vcpe_anim_time(int player);
uint32_t vcpe_anim_clip(int player);                 /* name hash of the current (or last) clip, 0: none */
float vcpe_anim_length(int player);                 /* length of the current (or last) clip, s */
extern float vcpe_anim_fov;                          /* last value of a camera FOV track (0: none) */

uint32_t vcpe_hash(const char *s);                   /* FNV-1a, the hash the converter uses for names */
#endif
