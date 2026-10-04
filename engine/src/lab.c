/* Examtaker lab floors (hellm01..07), rebuilt from LabScript / LaserBeam / Asset (pillar) /
 * Player.LaserDeath. Lasers are 2D raycasts against the "Laser Raycast" (beam end) and
 * "Laser Death" (kill) collider layers; crates carry both, the player only the second. */
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "game.h"
#include "gfx.h"
#include "audio.h"

#define MAX_LASERS 12
#define MAX_PILLARS 4

/* child paths of laser_dlc (Unity stores them as CRC32) */
#define PATH_LINE 383182111u
#define PATH_BLU 1052489143u
#define PATH_AIM 1390831613u
#define PATH_ORB 859586525u
#define PATH_ORBB 2994822503u

static const LabDef *L;
static struct {
    Animator anim, orb_a, orb_b;
    int mode;            /* LaserBeam.mode: 0 continuous kill, 1 idle/off, 2 timed shots, 3 boss-driven */
    int orb_on, orbb_on;
    float orbb_t;        /* orbB appears 0.13 s after orb */
    float next;          /* Active(): 0.88 s per laserLoop step */
    int step, looping;
    float ex, ey;        /* last beam end (LineRenderer keeps its old points when nothing is hit) */
    int has_end;
} las[MAX_LASERS];
static struct { int x, y, alive; } pil[MAX_PILLARS];
static int pillars_left, protection;

int lab_active(void) { return L != 0; }

static const int SND_BLINK[] = {SFX_boss_chain_blink_01, SFX_boss_chain_blink_02, SFX_boss_chain_blink_03, SFX_boss_chain_blink_04};
static const int SND_PILLAR_KICK[] = {SFX_enemy_kick_01, SFX_enemy_kick_02, SFX_enemy_kick_03};

static void laser_puzzle(int i)
{
    /* Puzzle(): mode 0, "puzzle", orb now, orbB after 0.13 s */
    las[i].mode = 0;
    anim_trigger(&las[i].anim, P_puzzle);
    las[i].orb_on = 1;
    las[i].orbb_t = 0.13f;
}

static void laser_active(int i)
{
    /* Active(): "activate", orbs, then a shot on every set laserLoop step, 0.88 s apart */
    anim_trigger(&las[i].anim, P_activate);
    las[i].orb_on = 1;
    las[i].orbb_t = 0.13f;
    las[i].looping = 1;
    las[i].next = 0.88f;
    las[i].step = 0;
}

void lab_load(void)
{
    L = S->lab;
    memset(las, 0, sizeof las);
    memset(pil, 0, sizeof pil);
    protection = 0;
    if (!L)
        return;
    for (int i = 0; i < L->nlasers && i < MAX_LASERS; i++) {
        anim_init(&las[i].anim, CTRL_laser_dlc);
        anim_init(&las[i].orb_a, CTRL_orb);
        anim_init(&las[i].orb_b, CTRL_orb);
        las[i].mode = L->lasers[i].mode;
        if (las[i].mode == 1)
            laser_puzzle(i);
        else if (las[i].mode == 2)
            laser_active(i);
    }
    for (int i = 0; i < L->npillars && i < MAX_PILLARS; i++) {
        pil[i].x = L->pillars[i].x;
        pil[i].y = L->pillars[i].y;
        pil[i].alive = 1;
    }
    pillars_left = L->pillar_count;
}

int lab_pillar_at(int x, int y)
{
    if (!L)
        return -1;
    for (int i = 0; i < L->npillars && i < MAX_PILLARS; i++)
        if (pil[i].alive && pil[i].x == x && pil[i].y == y)
            return i;
    return -1;
}

/* ------------------------------------------------------------ raycast */
static int aabb_hit(float ox, float oy, int dx, int dy, float cx, float cy, float w, float h, float *t)
{
    float x0 = cx - w * 0.5f, x1 = cx + w * 0.5f, y0 = cy - h * 0.5f, y1 = cy + h * 0.5f;
    float d;
    if (dx) {
        if (oy < y0 || oy > y1)
            return 0;
        if (dx > 0) { if (x1 < ox) return 0; d = x0 - ox; }
        else { if (x0 > ox) return 0; d = ox - x1; }
    } else {
        if (ox < x0 || ox > x1)
            return 0;
        if (dy > 0) { if (y1 < oy) return 0; d = y0 - oy; }
        else { if (y0 > oy) return 0; d = oy - y1; }
    }
    *t = d < 0 ? 0 : d;   /* Physics2D queries start inside colliders */
    return 1;
}

/* nearest collider on the given layer mask; returns 2 if it was the player's laserDeath box */
static int raycast(int i, int mask, float *dist)
{
    const LabLaser *l = &L->lasers[i];
    float best = 1e9f, t;
    int what = 0;
    for (int k = 0; k < L->nshields; k++) {
        const LabShield *s = &L->shields[k];
        if ((s->flag & mask) && aabb_hit(l->ox, l->oy, l->dx, l->dy, s->x, s->y, s->w, s->h, &t) && t < best)
            best = t, what = 1;
    }
    for (int b = 0; b < P.nboxes; b++) {
        const LabShield *s = &L->box_shield[b];
        const Ent *e = &P.boxes[b];
        if (e->alive && (s->flag & mask) &&
            aabb_hit(l->ox, l->oy, l->dx, l->dy, e->px + s->x, e->py + s->y, s->w, s->h, &t) && t < best)
            best = t, what = 1;
    }
    if ((mask & 2) && !P.hero_hidden &&
        aabb_hit(l->ox, l->oy, l->dx, l->dy, P.player.px + L->psh[0], P.player.py + L->psh[1], L->psh[2], L->psh[3], &t) && t < best)
        best = t, what = 2;
    *dist = best;
    return what;
}

static void protection_off(int a, int b) { (void)a; (void)b; protection = 0; }

/* LaserBeam.hitRay */
static void hit_ray(int i)
{
    float d;
    if (las[i].mode != 0 && L->lasers[i].audible)
        sfx_play_any(SND_BLINK, 4, 0);
    if (raycast(i, 2, &d) == 2)
        lab_player_hit();
    (void)protection_off;
}

/* Player.LaserDeath (also the AMD's "skull" hazards) */
extern int g_test_god;
void lab_player_hit(void)
{
    if (protection || P.dead || g_test_god)
        return;
    protection = 1;
    player_laser_death();
}

/* LaserBeam.Activate (AMD): boss mode, "activate" + orbs, no own shot loop */
void lab_laser_activate(int i)
{
    if (!L || i < 0 || i >= L->nlasers || i >= MAX_LASERS)
        return;
    las[i].mode = 1;
    laser_active(i);
}

void lab_laser_shot(int i)
{
    if (!L || i < 0 || i >= L->nlasers || i >= MAX_LASERS)
        return;
    anim_trigger(&las[i].anim, P_shot);
}

void lab_lasers_off(void)
{
    for (int k = 0; L && k < L->nlasers && k < MAX_LASERS; k++) {
        las[k].mode = 1;
        anim_trigger(&las[k].anim, P_deactivate);
    }
}

int lab_pillar_alive(int i)
{
    return L && i >= 0 && i < L->npillars && i < MAX_PILLARS && pil[i].alive;
}

void lab_pillar_alive_set(int i, int alive)
{
    if (L && i >= 0 && i < L->npillars && i < MAX_PILLARS)
        pil[i].alive = alive;
}

/* AMDBody.WeakSpot: the core comes back */
void lab_pillar_revive(int i)
{
    if (L && i >= 0 && i < L->npillars && i < MAX_PILLARS)
        pil[i].alive = 1;
}

static void laser_event(void *u, int fn, int arg)
{
    (void)arg;
    if (fn == EVT_hitRay)
        hit_ray((int)(intptr_t)u);
}

/* ------------------------------------------------------------ pillars */
static void holo_sound(int a, int b) { (void)a; (void)b; sfx_play(SFX_hologram_show_01, 0); }
static void after_pillars(int a, int b)
{
    (void)a; (void)b;
    if (!P.dead)
        puzzle_start_goal();   /* goalSprite moves onto the player */
}

/* LabScript.PillarBrk */
static void pillar_broken(int i)
{
    const LabPillar *lp = &L->pillars[i];
    pil[i].alive = 0;
    float wx = pil[i].x + S->gx / 256.0f, wy = pil[i].y + S->gy / 256.0f;
    vfx_spawn_any(CLIP_HIT, 2, wx, wy, 7, 1);
    if (!lp->arch)
        puzzle_bones(wx, wy);
    cam_shake(0.14f, 0.1f);
    if (lp->arch) {
        amd_core_hit();   /* LabScript.AMDhit */
        return;
    }
    Animator *pa = decor_anim(lp->decor);
    if (pa)
        anim_trigger(pa, P_break);
    sfx_play(SFX_gen_glass_break_01, 0);
    if (--pillars_left != 0)
        return;
    lab_lasers_off();
    for (int k = 0; k < L->ncog; k++) {
        Animator *a = decor_anim(L->cog[k]);
        if (a)
            anim_trigger(a, P_off);
    }
    for (int k = 0; k < L->napar; k++) {
        Animator *a = decor_anim(L->apar[k]);
        if (a)
            a->enabled = 0;
    }
    Animator *h = decor_anim(L->holo);
    if (h)
        anim_trigger(h, P_start);
    timer_after(1.1f, holo_sound, 0, 0);
    /* GoalSprite.AfterPillars(2) */
    if (!P.frozen) {
        P.frozen = 1;
        timer_after(2.0f, after_pillars, 0, 0);
    }
}

/* Player kicks a pillar. The core sits inside its generator housing (a void wall on the
 * blocking layer), so Moving.Move's linecast always hits the housing: every kick breaks it. */
void lab_kick_pillar(int i, int dx, int dy)
{
    (void)dx; (void)dy;
    sfx_play_any(SND_PILLAR_KICK, 3, 1);
    pillar_broken(i);
}

/* pause menu SKIP: break every pillar still standing */
void lab_skip(void)
{
    if (amd_active()) {
        amd_skip();   /* LabScript.Skip */
        return;
    }
    for (int i = 0; i < L->npillars && i < MAX_PILLARS; i++)
        if (pil[i].alive)
            pillar_broken(i);
}

/* ------------------------------------------------------------ update / draw */
void lab_update(float dt)
{
    if (!L)
        return;
    for (int i = 0; i < L->nlasers && i < MAX_LASERS; i++) {
        anim_update(&las[i].anim, dt, laser_event, (void *)(intptr_t)i);
        if (las[i].orb_on)
            anim_update(&las[i].orb_a, dt, 0, 0);
        if (las[i].orb_on && !las[i].orbb_on) {
            las[i].orbb_t -= dt;
            if (las[i].orbb_t <= 0)
                las[i].orbb_on = 1;
        }
        if (las[i].orbb_on)
            anim_update(&las[i].orb_b, dt, 0, 0);
        const LabLaser *l = &L->lasers[i];
        if (las[i].looping && l->nloop) {
            las[i].next -= dt;
            if (las[i].next <= 0) {
                las[i].next += 0.88f;
                if (l->loop[las[i].step] && las[i].mode != 1)
                    anim_trigger(&las[i].anim, P_shot);
                las[i].step = (las[i].step + 1) % l->nloop;
            }
        }
        /* Update(): beam end at the first Laser Raycast collider; continuous kill in mode 0 */
        float d;
        if (raycast(i, 1, &d)) {
            las[i].ex = l->ox + l->dx * d;
            las[i].ey = l->oy + l->dy * d;
            las[i].has_end = 1;
        }
        if (las[i].mode == 0)
            hit_ray(i);
    }
}

static void beam(int i, uint32_t path, uint32_t col)
{
    AnimOut o;
    anim_eval_path(&las[i].anim, path, &o);
    if (!o.has_lw || o.lw <= 0.001f || !las[i].has_end)
        return;
    const LabLaser *l = &L->lasers[i];
    float x0 = gfx_wx(l->ox), y0 = gfx_wy(l->oy), x1 = gfx_wx(las[i].ex), y1 = gfx_wy(las[i].ey);
    float w = o.lw * PXU;
    if (l->dx)
        gfx_rect(x0 < x1 ? x0 : x1, y0 - w * 0.5f, fabsf(x1 - x0), w, col);
    else
        gfx_rect(x0 - w * 0.5f, y0 < y1 ? y0 : y1, w, fabsf(y1 - y0), col);
}

static void orb(int i, const Animator *oa, uint32_t path, float base, uint32_t col)
{
    AnimOut p, o;
    anim_eval_path(&las[i].anim, path, &p);
    anim_eval(oa, &o);
    float s = (p.has_sx ? p.sx : base) * (o.has_sx ? o.sx : 1);
    if (s <= 0.001f)
        return;
    const LabLaser *l = &L->lasers[i];
    gfx_sprite(SPR_whiteCircle, gfx_wx(l->ox), gfx_wy(l->oy), s, s, col);
}

void lab_draw_layer(int layer)
{
    if (!L)
        return;
    for (int i = 0; i < L->nlasers && i < MAX_LASERS; i++) {
        const LabLaser *l = &L->lasers[i];
        if (layer == 3) {
            AnimOut o;
            anim_eval(&las[i].anim, &o);
            gfx_sprite(o.spr != SPR_NONE ? o.spr : SPR_labLaser0001, gfx_wx(l->x), gfx_wy(l->y), 1, 1, WHITE);
        } else if (layer == 7) {
            /* sorting order -3: blue glow, aim line, outer orb; -2: white core, inner orb */
            beam(i, PATH_BLU, RGBA(204, 204, 255, 255));
            beam(i, PATH_AIM, RGBA(204, 204, 255, 100));
            if (las[i].orbb_on)
                orb(i, &las[i].orb_b, PATH_ORBB, 1.1f, RGBA(204, 204, 255, 255));
        }
    }
    if (layer == 7) {
        for (int i = 0; i < L->nlasers && i < MAX_LASERS; i++) {
            beam(i, PATH_LINE, WHITE);
            if (las[i].orb_on)
                orb(i, &las[i].orb_a, PATH_ORB, 1.0f, WHITE);
        }
    }
}

/* monitor step counter (Player.willText on the lab screen, Segment7 font) */
void lab_draw_hud(void)
{
    if (!L || P.dead)
        return;
    char buf[8];
    puzzle_will_text(buf);
    float lh = gfx_font_line(FONT_SEG);
    gfx_text(FONT_SEG, gfx_wx(L->step_x), gfx_wy(L->step_y) - lh * 0.5f, buf, WHITE, 1, 1.0f);
}
