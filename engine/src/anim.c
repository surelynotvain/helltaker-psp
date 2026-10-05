#include <string.h>
#include <math.h>
#include "anim.h"

/* UnityEditor.Animations.AnimatorConditionMode */
enum { COND_IF = 1, COND_IFNOT = 2, COND_GREATER = 3, COND_LESS = 4, COND_EQUALS = 6, COND_NOTEQUAL = 7 };

int clip_find(const char *name)
{
    for (int i = 0; i < NUM_CLIPS; i++) {
        if (!strcmp(g_clips[i].name, name))
            return i;
    }
    return -1;
}

float clip_length(int clip)
{
    return clip < 0 ? 0 : g_clips[clip].length / 1000.0f;
}

static float wrap_time(int clip, float t, int loop)
{
    float len = clip_length(clip);
    if (len <= 0)
        return 0;
    if (loop) {
        if (!(t >= 0) || t > 1e6f)
            return 0;
        t = fmodf(t, len);
    } else if (t > len) {
        t = len;
    }
    return t;
}

int clip_sprite_at(int clip, float t, int loop)
{
    if (clip < 0)
        return SPR_NONE;
    const ClipDef *c = &g_clips[clip];
    if (!c->n)
        return SPR_NONE;
    int ms = (int)(wrap_time(clip, t, loop) * 1000.0f + 0.5f);
    int spr = g_clip_frames[c->first].spr;
    for (int i = 0; i < c->n; i++) {
        if (g_clip_frames[c->first + i].t <= ms)
            spr = g_clip_frames[c->first + i].spr;
        else
            break;
    }
    return spr;
}

static float curve_at(const ClipCurve *cv, int ms)
{
    const CurveKey *k = &g_curve_keys[cv->first];
    if (cv->n == 1 || ms <= k[0].t)
        return k[0].v / 256.0f;
    for (int i = 1; i < cv->n; i++) {
        if (ms <= k[i].t) {
            int span = k[i].t - k[i - 1].t;
            if (span <= 0)
                return k[i].v / 256.0f;
            float f = (ms - k[i - 1].t) / (float)span;
            return (k[i - 1].v + (k[i].v - k[i - 1].v) * f) / 256.0f;
        }
    }
    return k[cv->n - 1].v / 256.0f;
}

int anim_find_state(int ctrl, const char *clip_name)
{
    const CtrlDef *c = &g_ctrls[ctrl];
    for (int i = 0; i < c->nstates; i++) {
        int clip = g_states[c->first_state + i].clip;
        if (clip >= 0 && !strcmp(g_clips[clip].name, clip_name))
            return i;
    }
    return -1;
}

void anim_play_state(Animator *a, int state)
{
    a->state = state;
    a->t = a->prev_t = 0;
    a->finished = 0;
}

void anim_init(Animator *a, int ctrl)
{
    memset(a, 0, sizeof *a);
    a->ctrl = ctrl;
    a->enabled = 1;
    a->state = ctrl >= 0 ? g_ctrls[ctrl].def : 0;
}

void anim_trigger(Animator *a, int param) { if (param >= 0) a->params[param] = 1; }
void anim_set_bool(Animator *a, int param, int v) { if (param >= 0) a->params[param] = v ? 1 : 0; }

int anim_clip(const Animator *a)
{
    if (a->ctrl < 0)
        return -1;
    return g_states[g_ctrls[a->ctrl].first_state + a->state].clip;
}

float anim_norm_time(const Animator *a)
{
    int clip = anim_clip(a);
    float len = clip_length(clip);
    return len > 0 ? a->t / len : 1.0f;
}

static int conds_ok(Animator *a, const TransDef *t)
{
    for (int i = 0; i < t->ncond; i++) {
        const TransCond *c = &g_conds[t->first_cond + i];
        int v = a->params[c->param];
        if (c->mode == COND_IF && !v) return 0;
        if (c->mode == COND_IFNOT && v) return 0;
    }
    return 1;
}

static void consume(Animator *a, const TransDef *t)
{
    /* triggers reset when used; bools stay. We can't tell them apart from the
     * condition alone, so only reset params that are never IFNOT-tested. */
    for (int i = 0; i < t->ncond; i++) {
        const TransCond *c = &g_conds[t->first_cond + i];
        if (c->mode == COND_IF) {
            const CtrlDef *cd = &g_ctrls[a->ctrl];
            int is_bool = 0;
            for (int s = 0; s < cd->nstates && !is_bool; s++) {
                const StateDef *st = &g_states[cd->first_state + s];
                for (int k = 0; k < st->ntrans; k++) {
                    const TransDef *tt = &g_trans[st->first_trans + k];
                    for (int q = 0; q < tt->ncond; q++)
                        if (g_conds[tt->first_cond + q].param == c->param && g_conds[tt->first_cond + q].mode == COND_IFNOT)
                            is_bool = 1;
                }
            }
            if (!is_bool)
                a->params[c->param] = 0;
        }
    }
}

static void fire_events(Animator *a, int clip, float t0, float t1, void (*cb)(void *, int, int), void *user)
{
    if (!cb || clip < 0)
        return;
    const ClipDef *c = &g_clips[clip];
    for (int i = 0; i < c->nevents; i++) {
        const ClipEvent *e = &g_clip_events[c->first_event + i];
        float et = e->t / 1000.0f;
        if (et >= t0 && et < t1)
            cb(user, e->fn, e->arg);
    }
    (void)a;
}

/* Mecanim fires an exit-time transition when normalized time crosses exitTime; at 0 that
 * first happens at the end of the first loop, so the state plays its clip once. */
static float exit_norm(const TransDef *t)
{
    float e = t->exit / 1000.0f;
    return e <= 0.0f ? 1.0f : e;
}

void anim_update(Animator *a, float dt, void (*on_event)(void *, int, int), void *user)
{
    if (a->ctrl < 0 || !a->enabled)
        return;
    const CtrlDef *cd = &g_ctrls[a->ctrl];
    const StateDef *st = &g_states[cd->first_state + a->state];
    int clip = st->clip;
    float len = clip_length(clip);
    a->prev_t = a->t;
    a->t += dt * (st->speed / 256.0f);
    /* clip events, including wrap-around for looping clips */
    if (clip >= 0 && len > 0) {
        int loop = g_clips[clip].loop;
        float p = a->prev_t, n = a->t;
        if (loop) {
            float base = floorf(p / len) * len;
            p -= base;
            n -= base;
            if (n >= len) {
                fire_events(a, clip, p, len + 0.0001f, on_event, user);
                fire_events(a, clip, 0, n - len, on_event, user);
            } else {
                fire_events(a, clip, p, n, on_event, user);
            }
        } else if (p <= len) {
            fire_events(a, clip, p, n > len ? len + 0.0001f : n, on_event, user);
        }
        if (!loop && a->t >= len)
            a->finished = 1;
    }
    float norm = len > 0 ? a->t / len : 1.0f;
    /* any-state transitions */
    for (int i = 0; i < cd->nany; i++) {
        const TransDef *t = &g_trans[cd->first_any + i];
        if (!t->ncond && !t->has_exit)
            continue;
        if (!t->self && t->to == a->state)
            continue;
        if (t->has_exit && norm < exit_norm(t))
            continue;
        if (!t->ncond && t->to == a->state)
            continue; /* exit-time self loop: the clip already loops */
        if (conds_ok(a, t)) {
            consume(a, t);
            anim_play_state(a, t->to);
            return;
        }
    }
    for (int i = 0; i < st->ntrans; i++) {
        const TransDef *t = &g_trans[st->first_trans + i];
        if (t->has_exit && norm < exit_norm(t))
            continue;
        if (!t->has_exit && !t->ncond)
            continue;
        if (conds_ok(a, t)) {
            consume(a, t);
            anim_play_state(a, t->to);
            return;
        }
    }
}

static void eval_curves(const Animator *a, uint32_t path, AnimOut *o, int use_frames)
{
    memset(o, 0, sizeof *o);
    o->spr = SPR_NONE;
    o->sx = o->sy = 1;
    o->a = o->r = o->g = o->b = 1;
    o->active = 1;
    int clip = anim_clip(a);
    if (clip < 0)
        return;
    const ClipDef *c = &g_clips[clip];
    float t = wrap_time(clip, a->t, c->loop);
    if (use_frames)
        o->spr = clip_sprite_at(clip, t, c->loop);
    int ms = (int)(t * 1000.0f + 0.5f);
    for (int i = 0; i < c->ncurves; i++) {
        const ClipCurve *cv = &g_clip_curves[c->first_curve + i];
        if (cv->path != path)
            continue;
        if (cv->prop == CP_SPRITE) {
            if (path == 0 && use_frames)
                continue;
            const CurveKey *k = &g_curve_keys[cv->first];
            int spr = k[0].v;
            for (int j = 0; j < cv->n && k[j].t <= ms; j++)
                spr = k[j].v;
            o->spr = spr < 0 ? SPR_NONE : spr;
            o->spr_null = spr < 0;
            continue;
        }
        float v;
        if (cv->prop == CP_ACTIVE || cv->prop == CP_ENABLED) {
            /* discrete properties step: the last key at or before t */
            const CurveKey *k = &g_curve_keys[cv->first];
            int j = 0;
            while (j + 1 < cv->n && k[j + 1].t <= ms)
                j++;
            v = k[j].v / 256.0f;
        } else {
            v = curve_at(cv, ms);
        }
        switch (cv->prop) {
        case CP_POSX: o->dx = v; o->has_pos = o->has_posx = 1; break;
        case CP_POSY: o->dy = v; o->has_pos = o->has_posy = 1; break;
        case CP_SCALEX: o->sx = v; o->has_scale = o->has_sx = 1; break;
        case CP_SCALEY: o->sy = v; o->has_scale = o->has_sy = 1; break;
        case CP_ALPHA: o->a = v; o->has_alpha = 1; break;
        case CP_R: o->r = v; o->has_rgb = 1; break;
        case CP_G: o->g = v; o->has_rgb = 1; break;
        case CP_B: o->b = v; o->has_rgb = 1; break;
        case CP_ROTZ: o->rot = v * 4.0f; o->has_rot = 1; break; /* stored *64 */
        case CP_ACTIVE: o->active = v > 0.5f; o->has_active = 1; break;
        case CP_LINEW: o->lw = v; o->has_lw = 1; break;
        case CP_SIZEX: o->rw = v * 64.0f; o->has_rw = 1; break;   /* stored *4 */
        case CP_SIZEY: o->rh = v * 64.0f; o->has_rh = 1; break;
        case CP_ANCHY: o->ry = v * 64.0f; o->has_ry = 1; break;
        case CP_ENABLED: o->enabled = v > 0.5f; o->has_enabled = 1; break;
        case CP_COLW: o->cw = v; o->has_cw = 1; break;
        case CP_COLH: o->ch = v; o->has_ch = 1; break;
        default: break;
        }
    }
}

void anim_eval(const Animator *a, AnimOut *o) { eval_curves(a, 0, o, 1); }
void anim_eval_path(const Animator *a, uint32_t path, AnimOut *o) { eval_curves(a, path, o, path == 0); }
