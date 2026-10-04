/* hellm07: the Arch Mecha Demon, rebuilt from LabScript (AMD parts) / AMDLoop / AMDBody /
 * WaveSpike / BombardParticles / cannonSoundScript.
 *
 * The fight is a timeline: kicking the core starts loopIgniter's 31 s slide along one row of
 * AMDLoop "tics" (one row per phase); each tic it enters fires an attack. Every attack that can
 * kill is a "skull" trigger whose BoxCollider2D size is animated (0 = harmless). */
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "game.h"
#include "gfx.h"
#include "audio.h"

#define PATH_DEATHCOL 2934266396u
#define PATH_HARM 3441975007u
#define PATH_IGNITER 233908985u
#define PATH_TARGET 1181691900u
#define PATH_PROJECTILE 1023099501u
#define PATH_EXPLO 2043656033u
#define PATH_LINE1 247608487u
#define PATH_LINE2 2546689309u
#define PATH_LINEAIM 1390831613u

#define MAX_TICS 160
#define MAX_WAVES 40
#define MAX_BOMBS 32

int g_amd_phase;   /* Manager.AMDphase: survives a death, cleared by the main menu */

static const AmdDef *A;
static int started;            /* GoalSprite.started */
static uint8_t tic_in[MAX_TICS], tic_dead[MAX_TICS];
static uint8_t wave_in[3][MAX_WAVES];
static float ig_x, ig_y;
static int igniter_number, audible_hands, hp_left, hp_bar, cannon_order;
static struct { int on; float t, x, y; int sounded; Animator an; } bombs[MAX_BOMBS];

int amd_active(void) { return A != 0; }

static const int SND_HAND[] = {SFX_boss2_hand_hit_01, SFX_boss2_hand_hit_02, SFX_boss2_hand_hit_03, SFX_boss2_hand_hit_04};
static const int SND_BOMB[] = {SFX_boss2_cannon_explosion_01, SFX_boss2_cannon_explosion_02, SFX_boss2_cannon_explosion_03,
                               SFX_boss2_cannon_explosion_04, SFX_boss2_cannon_explosion_05, SFX_boss2_cannon_explosion_06};
static const int SND_SALVO[] = {SFX_boss2_cannon_shot_01, SFX_boss2_cannon_shot_02, SFX_boss2_cannon_shot_03, SFX_boss2_cannon_shot_04};

static void trig(int decor, int param)
{
    Animator *a = decor_anim(decor);
    if (a)
        anim_trigger(a, param);
}

static void sparks(float x, float y) { puzzle_bones(x, y); }   /* skeliScatter_dlc */

/* ------------------------------------------------------------ AMDBody animation events */
static void body_event(int decor, int fn, int arg)
{
    (void)decor;
    if (fn == EVT_ShakeSpawn) {
        if (igniter_number != 2)
            sfx_play_any(SND_HAND, 4, 0);
        trig(A->ig[igniter_number].decor, P_activate);
        cam_shake(0.14f, 0.1f);
    } else if (fn == EVT_HandSound) {
        if (audible_hands)
            sfx_play(SFX_boss2_hands_hit_01, 0);
        audible_hands = !audible_hands;
    } else if (fn == EVT_UltimateHandSound) {
        sfx_play(SFX_boss2_hands_hit_02, 0);
    } else if (fn == EVT_Laugh) {
        sfx_play(SFX_boss2_dance_start_01, 0);
    } else if (fn == EVT_EraserShake) {
        cam_shake(0.15f, 0.1f);
    } else if (fn == EVT_EraserShakeHARD) {
        cam_shake(0.3f, 0.1f);
    } else if (fn == EVT_WeakSpot) {
        lab_pillar_revive(0);
    } else if (fn == EVT_End) {
        trig(A->loop, P_end);
    } else if (fn == EVT_SparkSplash) {
        sparks(2, 4);
        sparks(4, 5);
        if (g_amd_phase == 1)
            sfx_play(SFX_boss2_batte_start_01, 0);
        else if (arg == 1)
            sfx_play(SFX_boss2_battle_end_01, 0);
    } else if (fn == EVT_EraserGunSounder) {
        sfx_play(g_amd_phase == 3 ? SFX_boss2_beam_strong_01 : SFX_boss2_beam_01, 0);
    } else if (fn == EVT_ScreenSparkSplash) {
        sparks(1, 7);
        sparks(5, 8);
    } else if (fn == EVT_Death) {
        sfx_play(SFX_boss2_battle_end_part2, 0);
        sfx_loop_stop();
        for (int i = 0; i < A->nwaves; i++)
            decor_set_hidden(A->waves[i].decor, 1);   /* spikers.SetActive(false) */
    } else if (fn == EVT_Overdrive) {
        trig(A->pomp[0], P_hyperActive);
        trig(A->pomp[1], P_hyperActive);
        sparks(-4, 1);
        sparks(10, 0);
    }
}

/* cannonSoundScript: only the right cannon is audible */
static void cannon_event(int decor, int fn, int arg)
{
    (void)arg;
    if (decor != A->turret[0])
        return;
    if (fn == EVT_Emerge)
        sfx_play(SFX_boss2_cannon_show_01, 0);
    else if (fn == EVT_Salvo)
        sfx_play(SND_SALVO[cannon_order < 3 ? cannon_order++ : 3], 0);
}

static void bomb_event(void *u, int fn, int arg)
{
    (void)arg;
    if (fn == EVT_Impact) {
        int i = (int)(intptr_t)u;
        for (int k = 0; k < 3; k++)
            vfx_spawn_any(CLIP_DUST, 3, bombs[i].x + (rand() % 10 - 5) * 0.1f, bombs[i].y + (rand() % 10 - 5) * 0.1f, 4, 0);
    }
}

static void bomb_spawn(float x, float y)
{
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].on) {
            bombs[i].on = 1;
            bombs[i].t = 0;
            bombs[i].x = x;
            bombs[i].y = y;
            bombs[i].sounded = 0;
            anim_init(&bombs[i].an, CTRL_bombardInstance1);
            return;
        }
    }
}

static void bomb_sound(int a, int b) { (void)a; (void)b; sfx_play_any(SND_BOMB, 6, 1); }

static void after_amd(int a, int b)
{
    (void)a; (void)b;
    if (P.frozen || P.dead)
        return;
    int track = -1;
    for (int i = 0; i < S->ndlg; i++)
        if (S->dlg[i].abyss_music >= 0)
            track = S->dlg[i].abyss_music;
    puzzle_start_goal();
    if (track >= 0)
        music_change(track);
}

/* ------------------------------------------------------------ AMDLoop.OnTriggerEnter2D */
static void tic_fire(int i)
{
    const AmdTic *t = &A->tics[i];
    if (tic_dead[i])
        return;
    switch (t->type) {
    case 0:
        sfx_loop_start(SFX_boss2_machine_loop_01);   /* Manager.LoopAMD */
        hp_bar = 1;
        hp_left = t->hp;
        trig(A->pomp[0], P_active);
        trig(A->pomp[1], P_active);
        if (started) {
            trig(A->body, P_dmg);
            break;
        }
        trig(A->body, P_start);
        started = 1;
        for (int k = 0; k < t->nshots; k++)
            lab_laser_activate(t->shots[k]);
        if (t->act) {
            trig(A->turret[0], P_fastActive);
            trig(A->turret[1], P_fastActive);
        }
        break;
    case 1:
        for (int k = 0; k < t->nshots; k++) {
            if (t->act)
                lab_laser_activate(t->shots[k]);
            else
                lab_laser_shot(t->shots[k]);
        }
        break;
    case 2:
        trig(A->body, t->attack);
        igniter_number = t->attack == P_right ? 0 : t->attack == P_left ? 1 : 2;
        break;
    case 3:
        timer_after(0.6f, bomb_sound, 0, 0);
        for (int k = 0; k < t->nbombs; k++) {
            int b = t->bombs[k];
            if (b > 16)
                bomb_spawn(b - 17.5f, -0.5f);
            else if (b > 8)
                bomb_spawn(b - 9.5f, 0.5f);
            else
                bomb_spawn(b - 1.5f, 1.5f);
        }
        break;
    case 4:
        if (t->act) {
            trig(A->turret[0], P_active);
            trig(A->turret[1], P_active);
            tic_dead[i] = 1;   /* Object.Destroy(gameObject) */
        } else {
            trig(A->turret[0], P_bombard);
            trig(A->turret[1], P_bombard);
        }
        break;
    case 5:
        hp_bar = 0;
        trig(A->body, P_skip);
        lab_lasers_off();
        trig(A->pomp[0], P_destroy);
        trig(A->pomp[1], P_destroy);
        /* GoalSprite.AfterAMD */
        music_change(-1);
        timer_after(5.0f, after_amd, 0, 0);
        break;
    }
}

static const int P_START[4] = {0, P_start1, P_start2, P_start3};

void amd_load(void)
{
    A = S->lab ? S->lab->amd : 0;
    memset(tic_in, 0, sizeof tic_in);
    memset(tic_dead, 0, sizeof tic_dead);
    memset(wave_in, 0, sizeof wave_in);
    memset(bombs, 0, sizeof bombs);
    started = 0;
    igniter_number = 0;
    audible_hands = 1;
    hp_bar = 0;
    hp_left = 0;
    cannon_order = 0;
    if (!A)
        return;
    decor_set_events(A->body, body_event);
    decor_set_events(A->turret[0], cannon_event);
    decor_set_events(A->turret[1], cannon_event);
    ig_x = A->lox - 4.97f;
    ig_y = A->loy + 0.35f;
    /* LabScript.Awake: a retry resumes the phase it died in */
    if (g_amd_phase != 0) {
        lab_pillar_alive_set(0, 0);
        trig(A->loop, P_START[g_amd_phase]);
    }
    /* CameraShake.Start: silence until the core is first hit */
    if (g_amd_phase == 0)
        music_change(-1);
}

/* LabScript.AMDhit (the core is kicked) */
void amd_core_hit(void)
{
    started = 1;
    if (g_amd_phase == 0)
        music_change(S->music);
    if (g_amd_phase != 3) {
        g_amd_phase++;
        trig(A->loop, P_START[g_amd_phase]);
    } else {
        trig(A->loop, P_kill);
    }
}

/* pause menu SKIP SUFFERING: LabScript.Skip */
void amd_skip(void)
{
    trig(A->loop, P_end);
    lab_pillar_alive_set(0, 0);
}

static int overlap(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh)
{
    return fabsf(ax - bx) < (aw + bw) * 0.5f && fabsf(ay - by) < (ah + bh) * 0.5f;
}

void amd_update(float dt)
{
    if (!A)
        return;
    AnimOut o;
    /* loopIgniter along its row */
    Animator *la = decor_anim(A->loop);
    if (la) {
        anim_eval(la, &o);
        if (o.has_posx)
            ig_x = A->lox + o.dx;
        if (o.has_posy)
            ig_y = A->loy + o.dy;
    }
    for (int i = 0; i < A->ntics && i < MAX_TICS; i++) {
        const AmdTic *t = &A->tics[i];
        int in = fabsf(ig_x - t->x) <= A->lhalf + t->w * 0.5f && fabsf(ig_y - t->y) <= A->lhalf + t->h * 0.5f;
        if (in && !tic_in[i])
            tic_fire(i);
        tic_in[i] = in;
    }
    /* spike igniters sweep down the arena; each ground wave they enter spikes */
    for (int k = 0; k < 3; k++) {
        const AmdIgniter *g = &A->ig[k];
        Animator *ga = decor_anim(g->decor);
        float ly = 0;
        if (ga) {
            anim_eval_path(ga, PATH_IGNITER, &o);
            if (o.has_posy)
                ly = o.dy;
        }
        float cy = g->y + ly + g->oy;
        for (int w = 0; w < A->nwaves && w < MAX_WAVES; w++) {
            const AmdWave *wv = &A->waves[w];
            int in = overlap(g->x, cy, g->w, g->h, wv->x, wv->y, wv->w, wv->w);
            if (in && !wave_in[k][w])
                trig(wv->decor, P_spike);
            wave_in[k][w] = in;
        }
    }
    /* bombardment instances (AnimDestroy: clip length - 0.02 s) */
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].on)
            continue;
        bombs[i].t += dt;
        anim_update(&bombs[i].an, dt, bomb_event, (void *)(intptr_t)i);
        if (bombs[i].t >= 1.08f)
            bombs[i].on = 0;
    }
    /* skull triggers vs the player's 0.5 x 0.5 trigger box */
    if (P.dead || P.hero_hidden)
        return;
    float px = P.player.px, py = P.player.py;
    Animator *ba = decor_anim(A->body);
    if (ba) {
        anim_eval_path(ba, PATH_DEATHCOL, &o);
        /* the scene's 0.0001-wide placeholder never fires; only the eraser-gun clips size it up */
        if (o.has_cw && o.cw > 0.001f && overlap(px, py, 0.5f, 0.5f, A->dcx, A->dcy, o.cw, o.has_ch ? o.ch : 3.0f))
            lab_player_hit();
    }
    for (int w = 0; w < A->nwaves && w < MAX_WAVES; w++) {
        Animator *wa = decor_anim(A->waves[w].decor);
        if (!wa)
            continue;
        anim_eval_path(wa, PATH_HARM, &o);
        if (o.has_cw && o.cw > 0.001f && overlap(px, py, 0.5f, 0.5f, A->waves[w].hx, A->waves[w].hy, o.cw, o.has_ch ? o.ch : o.cw))
            lab_player_hit();
    }
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].on)
            continue;
        /* the blast collider is 1.5 wide for one physics step around 0.72 s */
        if (bombs[i].t >= 0.70f && bombs[i].t - dt < 0.73f && bombs[i].t - dt >= 0 &&
            overlap(px, py, 0.5f, 0.5f, bombs[i].x, bombs[i].y, 1.5f, 1.5f))
            lab_player_hit();
    }
}

/* LineRenderer with an alpha gradient along its length (top -> bottom) and round caps */
static void cap(float x, float y, float w, int a)
{
    int sw = gfx_sprite_w(SPR_whiteCircle);
    if (sw <= 0 || a <= 0)
        return;
    float s = w * PXU / sw;
    gfx_sprite(SPR_whiteCircle, gfx_wx(x), gfx_wy(y), s, s, RGBA(255, 255, 255, a));
}

static void vbeam(float x, float y0, float y1, float w, int a0, int a1)
{
    cap(x, y0, w, a0);
    const int seg = 8;
    float sx = gfx_wx(x) - w * PXU * 0.5f;
    for (int s = 0; s < seg; s++) {
        float ya = y0 + (y1 - y0) * s / seg, yb = y0 + (y1 - y0) * (s + 1) / seg;
        int a = a0 + (a1 - a0) * (2 * s + 1) / (2 * seg);
        float t = gfx_wy(ya), b = gfx_wy(yb);
        gfx_rect(sx, t < b ? t : b, w * PXU, fabsf(b - t), RGBA(255, 255, 255, a));
    }
}

static void eraser_line(const Animator *ba, uint32_t path, float bx, float by, float base_w, float top, float bottom, int a0, int a1)
{
    AnimOut o;
    anim_eval_path(ba, path, &o);
    if (!o.has_active || !o.active)
        return;
    float w = o.has_lw ? o.lw : base_w;
    if (w <= 0.001f)
        return;
    float x = o.has_posx ? o.dx : bx, y = o.has_posy ? o.dy : by;
    vbeam(3.0f + x, top + y, bottom + y, w, a0, a1);
}

void amd_draw_layer(int layer)
{
    if (!A || layer != 7)
        return;
    for (int i = 0; i < MAX_BOMBS; i++) {
        if (!bombs[i].on)
            continue;
        AnimOut o;
        anim_eval_path(&bombs[i].an, PATH_TARGET, &o);
        float s = o.has_sx ? o.sx : 1;
        if (o.spr != SPR_NONE)
            gfx_sprite(o.spr, gfx_wx(bombs[i].x), gfx_wy(bombs[i].y + 0.3f), s, s, RGBA(255, 255, 255, (int)((o.has_alpha ? o.a : 0) * 255)));
        anim_eval_path(&bombs[i].an, PATH_EXPLO, &o);
        if (o.spr != SPR_NONE)
            gfx_sprite(o.spr, gfx_wx(bombs[i].x), gfx_wy(bombs[i].y + 0.4f), 1, 1, WHITE);
        anim_eval_path(&bombs[i].an, PATH_PROJECTILE, &o);
        if (o.spr != SPR_NONE)
            gfx_sprite(o.spr, gfx_wx(bombs[i].x + (o.has_posx ? o.dx : 0)), gfx_wy(bombs[i].y + (o.has_posy ? o.dy : 0)), 1, 1, WHITE);
    }
    /* eraser gun: laserLine2 (order 9), laserLine1 + aim (order 10) */
    Animator *ba = decor_anim(A->body);
    if (!ba)
        return;
    eraser_line(ba, PATH_LINE2, 0, -0.9f, 7.0f, 1.6f, -5.0f, 80, 0);
    eraser_line(ba, PATH_LINEAIM, 0, 2.4f, 5.5f, 2.0f, -5.0f, 200, 60);
    eraser_line(ba, PATH_LINE1, 0, -0.9f, 6.0f, 2.0f, -5.0f, 255, 255);
}

/* bossHPbar: frame, two bracket halves and three segments (hpLeft) */
#define UI_X(cx) (240.0f + (cx) * 0.25f)
#define UI_Y(cy) (136.0f - (cy) * 0.25f)
void amd_draw_ui(void)
{
    if (!A || !hp_bar || P.dead)
        return;
    const float k = 0.8f * 0.25f;
    float cx = UI_X(0), cy = UI_Y(-490);
    gfx_sprite_rect(SPR_button_small, cx - 30 * k - 200 * k, cy - 50 * k, 400 * k, 100 * k, WHITE);
    gfx_sprite_rect(SPR_button_small, cx + 40 * k + 200 * k, cy - 50 * k, -400 * k, 100 * k, WHITE);
    for (int i = 0; i < 3; i++) {
        if (i >= hp_left)
            continue;
        float x = cx + (-90 + 90 * i) * k, y = cy - 2.5f * k;
        float w = 68 * 1.2f * k, h = 48 * 0.6f * k;
        gfx_sprite_rect(SPR_W_chapter2, x - w * 0.5f, y - h * 0.5f, w, h, WHITE);
    }
}
