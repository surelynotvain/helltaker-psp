#ifndef ANIM_H
#define ANIM_H
#include <stdint.h>
#include "gamedata.h"

/* Minimal Unity Animator: one layer, trigger/bool params, exit-time transitions,
 * sprite timelines and root float curves (position, scale, colour). */
typedef struct {
    int16_t ctrl;
    int16_t state;
    float t;          /* seconds in state, already multiplied by state speed */
    float prev_t;
    uint8_t params[NUM_PARAMS > 0 ? NUM_PARAMS : 1];
    uint8_t enabled;
    uint8_t finished; /* non-looping clip reached its end */
} Animator;

typedef struct {
    int spr;                 /* SPR_NONE if the clip has no sprite track */
    uint8_t spr_null;        /* a child sprite track keyed to null (SpriteRenderer.sprite = null) */
    float dx, dy;            /* local position curves (world units), valid if has_posx/has_posy */
    float sx, sy;            /* local scale curves, valid if has_scale */
    float a, r, g, b;        /* colour curves, valid if has_* */
    float rot;               /* euler z degrees, valid if has_rot */
    float lw;                /* LineRenderer width multiplier, valid if has_lw */
    float rw, rh, ry;        /* RectTransform sizeDelta / anchoredPosition.y (canvas units) */
    uint8_t has_rw, has_rh, has_ry, enabled, has_enabled;
    float cw, ch;            /* BoxCollider2D size curves */
    uint8_t has_cw, has_ch;
    uint8_t has_lw, has_active, has_pos, has_scale, has_alpha, has_rgb, has_rot, has_posx, has_posy, has_sx, has_sy, active;
} AnimOut;

void anim_init(Animator *a, int ctrl);
void anim_trigger(Animator *a, int param);
void anim_set_bool(Animator *a, int param, int v);
void anim_play_state(Animator *a, int state);
int anim_find_state(int ctrl, const char *clip_name);
/* advance by dt; calls on_event(user, fn, arg) for crossed clip events */
void anim_update(Animator *a, float dt, void (*on_event)(void *, int, int), void *user);
void anim_eval(const Animator *a, AnimOut *o);
/* curves bound to a child object (CRC32 of its path below the animator) */
void anim_eval_path(const Animator *a, uint32_t path, AnimOut *o);
int anim_clip(const Animator *a);
float anim_norm_time(const Animator *a);

/* stand-alone clip helpers (for effects not driven by a controller) */
int clip_sprite_at(int clip, float t, int loop);
float clip_length(int clip);
int clip_find(const char *name);

#endif
