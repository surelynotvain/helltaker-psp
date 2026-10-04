/* Judgement's sin machine (chapter10_1..4), rebuilt from sinChainSummon / sinChainScript /
 * sinSpikeScript / bossHp / Player.ChainHurt. The machine (and camera) descend along the
 * sin_move curve; its two SIN colliders fire chain summoners and step the spike bands. */
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "game.h"
#include "gfx.h"
#include "audio.h"
#include <stdio.h>

#define MAX_CHAINS 48
#define MAX_SETS 16

static const BossDef *B;
static float t_machine, machine_y, cam_y0;
static uint8_t summ_done[64];
static struct { int on; float next; int idx, idx2; } loop_state[8];
static int nloops;
static struct { float det_y, col_y[2], tr_y[3], col_off[2]; int col_on[2], killer, closer; Animator tr_anim[3]; int det_in[2]; } sets[MAX_SETS];
static struct { int on; int vertical; float coord, center; float t; int real; } chains[MAX_CHAINS];
static struct { float x, y; int brk, broken, shown; float hit_t, shake; Animator anim; } giga[4];
static float bar, bar_max;
static int bar_on, boss_won, hearts_lost, protection;
static float protect_t, blink_t[8];
static int nblink;
static int goal_dlg;
static int landing_state;   /* 0 none, 1 Judgement landing, 2 standing on the machine */

int boss_landing_state(void) { return B ? landing_state : 0; }

static void landing_start(int a, int b) { (void)a; (void)b; landing_state = 1; decor_restart_kind(9); }
static void landing_done(int a, int b) { (void)a; (void)b; landing_state = 2; }

static const int SND_BLINK[] = {SFX_boss_chain_blink_01, SFX_boss_chain_blink_02, SFX_boss_chain_blink_03, SFX_boss_chain_blink_04};
static const int SND_CHAIN_DMG[] = {SFX_boss_chain_damage_01, SFX_boss_chain_damage_02, SFX_boss_chain_damage_03};
static const int SND_GIGA_SHOW[] = {SFX_boss_big_chain_show_01, SFX_boss_big_chain_show_02};
static const int SND_GIGA_KICK[] = {SFX_boss_chain_kick_01L, SFX_boss_chain_kick_02L, SFX_boss_chain_kick_03L};

int boss_active(void) { return B != 0; }
/* autotest: jump the machine (and camera) to the end of its descent */
void boss_debug_end(void) { if (B) t_machine = 1000.0f; }
float boss_cam_dy(void) { return B ? machine_y - B->machine_y : 0; }

static float curve_y(float t)
{
    if (t <= B->keys[0][0])
        return B->keys[0][1];
    for (int i = 1; i < B->nkeys; i++) {
        if (t <= B->keys[i][0]) {
            float a = B->keys[i - 1][0], b = B->keys[i][0];
            float f = b > a ? (t - a) / (b - a) : 1;
            return B->keys[i - 1][1] + (B->keys[i][1] - B->keys[i - 1][1]) * f;
        }
    }
    return B->keys[B->nkeys - 1][1];
}

void boss_load(void)
{
    B = S->boss;
    if (!B)
        return;
    t_machine = 0;
    machine_y = B->machine_y;
    cam_y0 = S->cam_y / 256.0f;
    memset(summ_done, 0, sizeof summ_done);
    memset(loop_state, 0, sizeof loop_state);
    memset(chains, 0, sizeof chains);
    nloops = 0;
    nblink = 0;
    for (int i = 0; i < B->nsets && i < MAX_SETS; i++) {
        const BossSpikeSet *s = &B->sets[i];
        sets[i].det_y = s->det[1];
        for (int k = 0; k < 2; k++) {
            sets[i].col_y[k] = s->col[k][1];
            sets[i].col_on[k] = 1;
            sets[i].col_off[k] = 0;
            sets[i].det_in[k] = 0;
        }
        for (int k = 0; k < 3; k++) {
            sets[i].tr_y[k] = s->tr[k][1];
            anim_init(&sets[i].tr_anim[k], CTRL_sinSpike);
        }
        /* sinSpikeScript.Start: spikes 1 and 2 raised, 0 lowered */
        anim_set_bool(&sets[i].tr_anim[1], P_ascending, 1);
        anim_set_bool(&sets[i].tr_anim[2], P_ascending, 1);
        sets[i].killer = 0;
        sets[i].closer = 0;
    }
    for (int i = 0; i < B->ngiga && i < 4; i++) {
        giga[i].x = B->giga[i].x;
        giga[i].y = B->giga[i].y;
        giga[i].brk = B->giga[i].brk;
        giga[i].broken = 0;
        giga[i].shown = 0;
        giga[i].hit_t = 0;
        giga[i].shake = 0;
        anim_init(&giga[i].anim, CTRL_cage);
    }
    bar = bar_max = B->bar_scale;
    bar_on = 0;
    boss_won = 0;
    hearts_lost = 0;
    protection = 0;
    protect_t = 0;
    goal_dlg = -1;
    landing_state = 0;
    for (int i = 0; i < S->ntrig; i++)
        if (S->trig[i].kind == TRIG_GOAL)
            goal_dlg = S->trig[i].dlg;
}

/* ------------------------------------------------------------ chains */
static void chain_spawn(int vertical, float coord, float center)
{
    for (int i = 0; i < MAX_CHAINS; i++) {
        if (!chains[i].on) {
            chains[i].on = 1;
            chains[i].vertical = vertical;
            chains[i].coord = coord;
            chains[i].center = center;
            chains[i].t = 0;
            chains[i].real = 1;
            return;
        }
    }
}

static void summon(const BossSummoner *s, float cord)
{
    if (cord > 0)
        chain_spawn(1, cord - 1, s->y + 2);           /* column x = cord - 1, centred at y + 2 */
    else
        chain_spawn(0, cord + s->y + 5, s->x + 3);    /* row y = cord + y + 5, centred at x + 3 */
}

static void summon_list(const BossSummoner *s, const int8_t *list, int n)
{
    if (nblink < 8)
        blink_t[nblink++] = 0.5f; /* ChainBlink: sound after 0.5 s */
    if (s->reverse) {
        for (int i = 0; i < s->nall; i++) {
            int skip = 0;
            for (int k = 0; k < n && k < 2; k++)
                if (s->all[i] == list[k])
                    skip = 1;
            if (!skip)
                summon(s, s->all[i]);
        }
    } else {
        for (int i = 0; i < n; i++)
            summon(s, list[i]);
    }
}

static void player_hurt(void);

static void chain_strike(int i)
{
    /* sinChainScript: collider live for 0.02 s at t = 0.6 */
    int px = (int)floorf(P.player.px + 0.5f), py = (int)floorf(P.player.py + 0.5f);
    int hit = chains[i].vertical ? (px == (int)floorf(chains[i].coord + 0.5f))
                                 : (py == (int)floorf(chains[i].coord + 0.5f));
    if (hit)
        player_hurt();
}

/* ------------------------------------------------------------ player damage */
static void protect_off(int a, int b) { (void)a; (void)b; protection = 0; }

static void player_hurt(void)
{
    if (protection || P.cheater || P.dead || boss_won)
        return;
#ifdef BOSS_DEBUG
    printf("chain hit hearts_lost=%d\n", hearts_lost);
#endif
    if (hearts_lost > 2) {
        sfx_loop_stop();
        protection = 1;
        player_dialogue_restart(-2);
        P.dead = 1;
        return;
    }
    /* Player.ChainHurt */
    protection = 1;
    hearts_lost++;
    P.will_red = 1.0f;
    P.player_red = 1.0f;
    cam_shake(0.14f, 0.1f);
    vfx_spawn_any(CLIP_BLOOD, 3, P.player.px, P.player.py, 7, 0);
    sfx_play_any(SND_CHAIN_DMG, 3, 0);
    timer_after(0.4f, protect_off, 0, 0);
}

static void spike_death(void)
{
    sfx_loop_stop();
    if (protection || P.dead || boss_won)
        return;
    protection = 1;
    P.dead = 1;
    player_dialogue_restart(-2);
}

/* ------------------------------------------------------------ SIN colliders */
static int sin_overlap(float y, float oy)
{
    for (int k = 0; k < 2; k++)
        if (fabsf(machine_y + B->sin_off[k] - y) < 1.0f)
            return 1;
    (void)oy;
    return 0;
}

static void spike_change(int i)
{
    /* sinSpikeScript.SpikeChange */
    int killer = sets[i].killer, closer = sets[i].closer;
    sets[i].det_y -= 1;
    sets[i].col_on[killer] = 0;
    sets[i].col_y[killer] -= 2;
    sets[i].col_off[killer] = 0.3f;
    sets[i].tr_y[closer] -= 3;
    int opener = closer;
    closer = (closer + 1) % 3;
    anim_set_bool(&sets[i].tr_anim[opener], P_ascending, 1);
    anim_set_bool(&sets[i].tr_anim[closer], P_ascending, 0);
    sets[i].closer = closer;
    sets[i].killer = killer ^ 1;
}

static void after_chains_dialogue(int a, int b)
{
    (void)a; (void)b;
    /* GoalSprite.BrokenChains: the dialogue trigger jumps onto the player */
    if (!P.frozen && goal_dlg >= 0) {
        P.frozen = 1;
        dlg_start(goal_dlg);
    }
}

static void boss_defeated(void)
{
    boss_won = 1;
    bar_on = 0;
    nloops = 0;
    for (int i = 0; i < 8; i++)
        loop_state[i].on = 0;
    cam_shake(0.2f, 0.1f);
    /* GoalSprite.BrokenChains(1): landing at +1 s, dialogue at +2 s, Judgement settled at +3 s */
    timer_after(1.0f, landing_start, 0, 0);
    timer_after(2.0f, after_chains_dialogue, 0, 0);
    timer_after(3.0f, landing_done, 0, 0);
}

int boss_try_kick(int tx, int ty)
{
    if (!B)
        return 0;
    for (int i = 0; i < B->ngiga && i < 4; i++) {
        if (!giga[i].shown || giga[i].broken)
            continue;
        if ((int)floorf(giga[i].x + 0.5f) == tx && (int)floorf(giga[i].y + 0.5f) == ty) {
            /* Asset.OnKicked, tag gigachain */
            sfx_play_any(SND_GIGA_KICK, 3, 1);
            anim_trigger(&giga[i].anim, P_chain_hit);
            giga[i].brk--;
            giga[i].hit_t = 0.15f;
            bar -= 1.0f * B->bar_mult * B->bar_def;
            if (giga[i].brk < 1) {
                giga[i].broken = 1;
                cam_shake(0.2f, 0.1f);
                anim_trigger(&giga[i].anim, P_chain_break);
                sfx_play(SFX_boss_chain_break_01R, 0);
            } else {
                vfx_spawn_any(CLIP_HIT, 2, giga[i].x, giga[i].y, 7, 1);
                giga[i].shake = 0.1f;
            }
            if (bar <= 0 && !boss_won)
                boss_defeated();
            return 1;
        }
    }
    return 0;
}

void boss_update(float dt)
{
    if (!B || dt <= 0)
        return;
    float prev_y = machine_y;
    t_machine += dt;
    machine_y = curve_y(t_machine);
    gfx_camera(S->cam_x / 256.0f, cam_y0 + (machine_y - B->machine_y));
    (void)prev_y;
#ifdef BOSS_DEBUG
    {
        static int last = -1;
        if ((int)t_machine != last) {
            last = (int)t_machine;
            int nc = 0;
            for (int i = 0; i < MAX_CHAINS; i++) nc += chains[i].on;
            int nd = 0;
            for (int i = 0; i < B->nsumm; i++) nd += summ_done[i];
            printf("t=%d my=%.2f chains=%d summ_done=%d/%d player=%.1f,%.1f bar=%d\n", last, machine_y, nc, nd, B->nsumm, P.player.px, P.player.py, bar_on);
        }
    }
#endif
    /* machine start / stop thumps (sinSideAnim CamShaker events) */
    static const float shakes[4] = {1.5167f, 2.2667f, 22.6833f, 23.4333f};
    for (int k = 0; k < 4; k++)
        if (t_machine - dt < shakes[k] && t_machine >= shakes[k])
            cam_shake(0.2f, 0.1f);
    /* AnimSound.sinMachineOn / Off + Manager.sinLoopSource */
    if (t_machine - dt < 1.5333f && t_machine >= 1.5333f) {
        sfx_play(SFX_boss_machine_start_01, 0);
        sfx_loop_start(SFX_boss_machine_loop_01);
    }
    if (t_machine - dt < 22.7f && t_machine >= 22.7f) {
        sfx_play(SFX_boss_machine_off_01, 0);
        sfx_loop_stop();
    }

    /* summoners entered by a SIN collider */
    for (int i = 0; i < B->nsumm && i < 64; i++) {
        const BossSummoner *s = &B->summ[i];
        if (!s->active || summ_done[i] || !sin_overlap(s->y, 0))
            continue;
        summ_done[i] = 1;
        if (s->looping) {
            if (nloops < 8 && !boss_won) {
                loop_state[nloops].on = 1;
                loop_state[nloops].next = s->delay_init;
                loop_state[nloops].idx = i;
                loop_state[nloops].idx2 = 0;
                nloops++;
            }
            continue;
        }
        summon_list(s, s->sum, s->nsum);
        if (s->final) {
            sfx_play_any(SND_GIGA_SHOW, 2, 0);
            bar_on = 1;
            for (int g = 0; g < B->ngiga && g < 4; g++) {
                if (B->giga[g].has_root) {
                    giga[g].x = B->giga[g].rx;
                    giga[g].y = B->giga[g].ry;
                    giga[g].shown = 1;
                    anim_trigger(&giga[g].anim, P_chain_hit);
                }
            }
        }
    }
    /* looping summoners (phases II-IV) */
    for (int l = 0; l < nloops; l++) {
        if (!loop_state[l].on)
            continue;
        loop_state[l].next -= dt;
        if (loop_state[l].next <= 0) {
            const BossSummoner *s = &B->summ[loop_state[l].idx];
            int a = loop_state[l].idx2 % (s->nchn ? s->nchn : 1);
            if (s->nchn)
                summon_list(s, s->chn[a], s->chn_n[a]);
            loop_state[l].idx2++;
            loop_state[l].next += s->delay;
        }
    }
    for (int k = 0; k < nblink; k++) {
        blink_t[k] -= dt;
        if (blink_t[k] <= 0) {
            sfx_play_any(SND_BLINK, 4, 1);
            blink_t[k] = blink_t[--nblink];
            k--;
        }
    }
    /* chains: strike at 0.6 s, gone at 0.92 s */
    for (int i = 0; i < MAX_CHAINS; i++) {
        if (!chains[i].on)
            continue;
        float t0 = chains[i].t;
        chains[i].t += dt;
        if (t0 < 0.6f && chains[i].t >= 0.6f)
            chain_strike(i);
        if (chains[i].t >= 0.92f)
            chains[i].on = 0;
    }
    /* spike sets */
    int px = (int)floorf(P.player.px + 0.5f), py = (int)floorf(P.player.py + 0.5f);
    for (int i = 0; i < B->nsets && i < MAX_SETS; i++) {
        int in = sin_overlap(sets[i].det_y, 0);
        if (in && !sets[i].det_in[0])
            spike_change(i);
        sets[i].det_in[0] = sin_overlap(sets[i].det_y, 0);
        for (int k = 0; k < 2; k++) {
            if (!sets[i].col_on[k]) {
                sets[i].col_off[k] -= dt;
                if (sets[i].col_off[k] <= 0)
                    sets[i].col_on[k] = 1;
            }
            if (sets[i].col_on[k] && px == (int)floorf(B->sets[i].col[k][0] + 0.5f) &&
                py == (int)floorf(sets[i].col_y[k] + 0.5f)) {
#ifdef BOSS_DEBUG
                printf("spike death set %d col %d at %.1f,%.1f my=%.2f\n", i, k, B->sets[i].col[k][0], sets[i].col_y[k], machine_y);
#endif
                spike_death();
            }
        }
        for (int k = 0; k < 3; k++)
            anim_update(&sets[i].tr_anim[k], dt, 0, 0);
    }
    for (int g = 0; g < B->ngiga && g < 4; g++) {
        anim_update(&giga[g].anim, dt, 0, 0);
        if (giga[g].hit_t > 0) giga[g].hit_t -= dt;
        if (giga[g].shake > 0) giga[g].shake -= 0.02f * dt * 60.0f;
    }
}

/* decor riding a gigachain: offset of that chain from its scene position */
int boss_giga_offset(int g, float *dx, float *dy)
{
    if (!B || g < 0 || g >= B->ngiga)
        return 0;
    if (giga[g].broken && anim_norm_time(&giga[g].anim) >= 1.0f)
        return -1;
    *dx = giga[g].x - B->giga[g].x;
    *dy = giga[g].y - B->giga[g].y;
    if (giga[g].shake > 0) {
        *dx += ((rand() % 200) / 100.0f - 1.0f) * giga[g].shake;
        *dy += ((rand() % 200) / 100.0f - 1.0f) * giga[g].shake;
    }
    return giga[g].shown ? 1 : -1;
}

float boss_giga_flash(int g) { return (B && g >= 0 && g < 4 && giga[g].hit_t > 0) ? giga[g].hit_t / 0.15f : 0; }

void boss_draw_layer(int layer)
{
    if (!B)
        return;
    if (layer == 3) {
        for (int i = 0; i < B->nsets && i < MAX_SETS; i++) {
            for (int k = 0; k < 3; k++) {
                AnimOut o;
                anim_eval(&sets[i].tr_anim[k], &o);
                int spr = o.spr != SPR_NONE ? o.spr : SPR_assets100V20116;
                gfx_sprite(spr, gfx_wx(B->sets[i].tr[k][0]), gfx_wy(sets[i].tr_y[k]), 1, 1,
                           RGBA(255, 255, 255, (int)(255 * (o.has_alpha ? o.a : 1))));
            }
        }
    } else if (layer == 7) {
        /* sinChain_appear on every link: thin faint warning (width 0.8 -> 0.2, alpha 0.2 -> 0),
         * full-width white strike at 0.5167 s, then the links shrink away until 0.6667 s */
        for (int i = 0; i < MAX_CHAINS; i++) {
            if (!chains[i].on)
                continue;
            float t = chains[i].t, w, a;
            if (t < 0.5f) {
                w = 0.8f - 1.2f * t;
                a = 0.2f - 0.4f * t;
            } else if (t < 0.5167f) {
                w = 0.2f; a = 0.0f;
            } else if (t < 0.6667f) {
                w = 1.0f - (t - 0.5167f) / 0.15f;
                a = 1.0f;
            } else {
                continue;
            }
            if (a <= 0.01f || w <= 0.01f)
                continue;
            /* warning chains read grey, the strike is solid white */
            uint32_t col = t < 0.5f ? RGBA(150, 140, 160, (int)((0.25f + a) * 255))
                                    : RGBA(255, 255, 255, (int)(a * 255));
            for (int k = -6; k <= 6; k++) {
                float off = k * 1.5f;
                if (chains[i].vertical)
                    gfx_sprite(SPR_chainlink, gfx_wx(chains[i].coord), gfx_wy(chains[i].center + off), w, 1, col);
                else
                    gfx_sprite_rot(SPR_chainlink, gfx_wx(chains[i].center + off), gfx_wy(chains[i].coord + 0.2f), w, 1, 90, col);
            }
        }
    }
}

void boss_draw_ui(void)
{
    if (!B || dlg_active() || P.dead)
        return;
    if (bar_on && !boss_won) {
        /* chainTitle (y 470) with bar at -68 and the two bracket halves at +-160, -70 */
        const char *title = (B->title < NUM_TEXT_M) ? g_text_m[B->title] : "";
        gfx_text_fit(FONT_TITLE, 240, 7, title, WHITE, 400);
        float full = 22 * 24 * 0.25f;                  /* sizeDelta 22 x scale 24, canvas -> PSP */
        float w = full * (bar > 0 ? bar / bar_max : 0);
        gfx_sprite(SPR_button_small, 240 + 40, 36, -1, 1, WHITE);
        gfx_sprite(SPR_button_small, 240 - 40, 36, 1, 1, WHITE);
        gfx_rect(240 - full * 0.5f, 34.5f, w, 2.5f, WHITE);
    }
}

void boss_god_mode(void)
{
    /* pause menu "GOD MODE": Player.PowerOfLove -> cheater ignores chain hits */
    P.cheater = 1;
}

/* Player.hearts are the four sin pyres; hearts[nextHeart] goes out on each chain hit */
int boss_heart_alive(int idx) { return !B || idx >= hearts_lost; }

const Animator *boss_giga_anim(int g) { return (B && g >= 0 && g < B->ngiga && g < 4) ? &giga[g].anim : 0; }
