/* Grid puzzle rules rebuilt from Player/Moving/Asset/Spikes (Helltaker 1.x). */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "game.h"
#include "gfx.h"
#include "audio.h"

Puzzle P;
static int will_changed;   /* lab screen: zero-padded only after the first MinusWill */
static float gx, gy;          /* grid origin offset (world) */
static int lock_x, lock_y, key_x, key_y;
static int hint_moved;
static uint32_t trig_used, trig_on;
static int cur_trig = -1;
static int ritual_idx, ritual_done, portal_entered;

int ritual_complete(void) { return ritual_done; }

/* GoalSprite.OpenMask(0) after an 888 epilogue line */
void puzzle_dialogue_closed(int epilogue)
{
    if (cur_trig < 0)
        return;
    if (epilogue == 1) {
        /* Player.EndingCancel: step back off the police door so it can be reopened */
        P.player.y += 1;
        P.player.py += 1;
    } else {
        trig_used |= 1u << cur_trig;
    }
    cur_trig = -1;
}

static void portal_next(int a, int b) { (void)a; (void)b; scene_request(g_scene + 1); }

static void ritual_step(void)
{
    if (!S->nritual || ritual_done)
        return;
    int x = P.player.x, y = P.player.y;
    if (x == S->ritual[ritual_idx][0] && y == S->ritual[ritual_idx][1]) {
        if (++ritual_idx == S->nritual) {
            /* abyssPortal opens: pentagram, portal, Beelzebub's trigger */
            ritual_done = 1;
            cam_shake(1.0f, 0.1f);
            sfx_play(SFX_abyss_portal_opening_01, 0);
            for (int i = 0; i < S->ntrig; i++)
                trig_on |= 1u << i;
        }
        return;
    }
    /* any other step on the ritual floor drops back to the start */
    int inside = x >= 2 && x <= 4 && y >= 3 && y <= 5;
    ritual_idx = 0;
    if (inside && x == S->ritual[0][0] && y == S->ritual[0][1])
        ritual_idx = 1;
}
static int started;
static int death_on;
static float death_t;

/* skeliScatter: 14 spark particles thrown up, removed after 0.4-0.6 s while shrinking */
#define MAX_BONES 64
static struct { float x, y, vx, vy, life, t; int spr, flip; } bones[MAX_BONES];
static const float bone_pos[14][2] = {{-0.13f, 0.58f}, {0.309f, 0.58f}, {-0.107f, 0.299f}, {-0.145f, 0.125f}, {0.121f, 0.284f},
    {0.127f, 0.106f}, {-0.103f, 0.389f}, {0.289f, 0.383f}, {0.162f, 0.585f}, {-0.005f, 0.551f}, {-0.366f, 0.264f},
    {0.369f, 0.224f}, {0.22f, -0.092f}, {-0.188f, -0.069f}};
static const int bone_spr[14] = {SPR_particle0001, SPR_particle0001, SPR_particle0002, SPR_particle0002, SPR_particle0002,
    SPR_particle0002, SPR_particle0006, SPR_particle0006, SPR_particle0007, SPR_particle0008, SPR_particle0004,
    SPR_particle0004, SPR_particle0003, SPR_particle0003};

static float frnd(float a, float b) { return a + (b - a) * (rand() % 10000) / 10000.0f; }

static void bones_spawn(float x, float y)
{
    int k = 0;
    for (int i = 0; i < MAX_BONES && k < 14; i++) {
        if (bones[i].life > 0)
            continue;
        bones[i].x = x + bone_pos[k][0] * 1.2f;
        bones[i].y = y + bone_pos[k][1] * 1.2f;
        /* AddForce(+-40, 30..90) on a 1 kg body over one 0.02 s physics step */
        bones[i].vx = frnd(-40, 40) * 0.02f;
        bones[i].vy = frnd(30, 90) * 0.02f;
        bones[i].life = frnd(0.4f, 0.6f);
        bones[i].t = 0;
        bones[i].spr = bone_spr[k];
        bones[i].flip = rand() & 1;
        k++;
    }
}

static void bones_update(float dt)
{
    for (int i = 0; i < MAX_BONES; i++) {
        if (bones[i].life <= 0)
            continue;
        bones[i].t += dt;
        bones[i].vy -= 9.81f * dt;
        bones[i].x += bones[i].vx * dt;
        bones[i].y += bones[i].vy * dt;
        if (bones[i].t >= bones[i].life)
            bones[i].life = 0;
    }
}

static const int SND_MOVE[] = {SFX_character_move_01};
static const int SND_STONE_MOVE[] = {SFX_stone_move_01, SFX_stone_move_02, SFX_stone_move_03};
static const int SND_STONE_KICK[] = {SFX_stone_kick_01, SFX_stone_kick_02, SFX_stone_kick_03};
static const int SND_ENEMY_KICK[] = {SFX_enemy_kick_01, SFX_enemy_kick_02, SFX_enemy_kick_03};
static const int SND_ENEMY_DIE[] = {SFX_enemy_die_01, SFX_enemy_die_02, SFX_enemy_die_03};
static const int SND_DOOR_KICK[] = {SFX_door_closed_kick_01, SFX_door_closed_kick_02, SFX_door_closed_kick_03};
static const int SND_BLOOD[] = {SFX_spikes_damage_01, SFX_spikes_damage_02};

static float wxc(int cx) { return cx + gx; }
static float wyc(int cy) { return cy + gy; }

static int is_wall(int x, int y)
{
    for (int i = 0; i < S->nwalls; i++)
        if (S->walls[i][0] == x && S->walls[i][1] == y)
            return 1;
    return 0;
}

static int box_at(int x, int y)
{
    for (int i = 0; i < P.nboxes; i++)
        if (P.boxes[i].alive && P.boxes[i].x == x && P.boxes[i].y == y)
            return i;
    return -1;
}

static int skel_at(int x, int y)
{
    for (int i = 0; i < P.nskel; i++)
        if (P.skel[i].alive && P.skel[i].x == x && P.skel[i].y == y)
            return i;
    return -1;
}

/* lockboxDetector: 2x2 trigger around the lock drops it out of the blocking layer
 * while a key holder stands next to it */
static int lock_blocks(void)
{
    if (!P.lock_alive)
        return 0;
    if (P.key_master && abs(P.player.x - lock_x) <= 1 && abs(P.player.y - lock_y) <= 1)
        return 0;
    return 1;
}

static int is_lock(int x, int y) { return P.lock_alive && x == lock_x && y == lock_y; }

int puzzle_cell_blocked(int x, int y)
{
    return is_wall(x, y) || box_at(x, y) >= 0 || skel_at(x, y) >= 0 || is_lock(x, y) || lab_pillar_at(x, y) >= 0;
}

static void ent_init(Ent *e, int x, int y, int ctrl)
{
    memset(e, 0, sizeof *e);
    e->x = x; e->y = y;
    e->px = wxc(x); e->py = wyc(y);
    e->alive = 1;
    anim_init(&e->anim, ctrl);
}

/* Moving.SmoothMovement: Lerp(pos, end, 0.5 * dt * 60) until within 0.1 */
static void ent_slide(Ent *e, float dt)
{
    if (!e->moving)
        return;
    float tx = wxc(e->x), ty = wyc(e->y);
    float k = 0.5f * dt * 60.0f;
    if (k > 1) k = 1;
    e->px += (tx - e->px) * k;
    e->py += (ty - e->py) * k;
    float dx = tx - e->px, dy = ty - e->py;
    if (dx * dx + dy * dy <= 0.01f) {
        e->px = tx; e->py = ty;
        e->moving = 0;
    }
}

static void ent_begin_move(Ent *e, int nx, int ny, const int *snd, int nsnd)
{
    vfx_spawn_any(CLIP_DUST, 3, e->px, e->py, 4, 0);
    if (nsnd)
        sfx_play_any(snd, nsnd, 1);
    e->x = nx; e->y = ny;
    e->moving = 1;
}

void puzzle_load(void)
{
    memset(&P, 0, sizeof P);
    gx = S->gx / 256.0f;
    gy = S->gy / 256.0f;
    hint_moved = 0;
    trig_used = 0;
    trig_on = 0;
    for (int i = 0; i < S->ntrig; i++)
        if (S->trig[i].active)
            trig_on |= 1u << i;
    cur_trig = -1;
    ritual_idx = 0;
    ritual_done = 0;
    portal_entered = 0;
    started = 0;
    death_on = 0;
    memset(bones, 0, sizeof bones);
    ent_init(&P.player, S->px, S->py, S->player_ctrl);
    P.will = S->will;
    will_changed = 0;
    P.restartable = 1;
    P.in_menu = 1;
    P.un_menuable = 1;
    P.nboxes = S->nboxes < 24 ? S->nboxes : 24;
    for (int i = 0; i < P.nboxes; i++) {
        ent_init(&P.boxes[i], S->boxes[i].x, S->boxes[i].y, -1);
        P.boxes[i].spr = S->boxes[i].spr;
        P.boxes[i].flip = S->boxes[i].flip;
    }
    P.nskel = S->nskel < 16 ? S->nskel : 16;
    for (int i = 0; i < P.nskel; i++) {
        ent_init(&P.skel[i], S->skel[i].x, S->skel[i].y, CTRL_skeli);
        P.skel[i].flip = S->skel[i].flip;
    }
    P.nspikes = S->nspikes < 32 ? S->nspikes : 32;
    for (int i = 0; i < P.nspikes; i++) {
        P.spikes[i].x = S->spikes[i].x;
        P.spikes[i].y = S->spikes[i].y;
        P.spikes[i].up = S->spikes[i].up;
        P.spikes[i].stat = S->spikes[i].stat;
        anim_init(&P.spikes[i].anim, CTRL_spikes1);
        if (!P.spikes[i].up)
            anim_trigger(&P.spikes[i].anim, P_spikeStartDown);
    }
    P.key_alive = S->has_key;
    P.lock_alive = S->has_lock;
    key_x = S->key_x; key_y = S->key_y;
    lock_x = S->lock_x; lock_y = S->lock_y;
    anim_init(&P.key_anim, CTRL_key);
}

/* ------------------------------------------------------------ turn flow */
void game_over_ext(int target);

static void will_minus(void)
{
    if (P.will >= 50 || P.will <= 0)
        return;
    P.will--;
    will_changed = 1;
}

void player_laser_death(void) { game_over_ext(-2); }
void puzzle_bones(float x, float y) { bones_spawn(x, y); }

static void waiter_done(int a, int b)
{
    (void)a; (void)b;
    P.player_turn = 1;
}

static void waiter_mid(int a, int b)
{
    (void)a; (void)b;
    if (P.will > 0)
        will_minus();
    if (P.will_scale < 0.2f) {
        P.will_scale = 0.2f;
    }
    timer_after(0.01f, waiter_done, 0, 0);
}

static void spiked(void)
{
    if (P.will > 0)
        will_minus();
    P.will_scale = 0.5f;
    P.will_red = 1.0f;
    P.player_red = 1.0f;
    cam_shake(0.14f, 0.1f);
    vfx_spawn_any(CLIP_BLOOD, 3, P.player.px, P.player.py, 7, 0);
    sfx_play_any(SND_BLOOD, 2, 1);
}

static void skel_broken(int i)
{
    Ent *e = &P.skel[i];
    if (!e->alive)
        return;
    sfx_play_any(SND_ENEMY_DIE, 3, 0);
    vfx_spawn_any(CLIP_HIT, 2, e->px, e->py, 7, 1);
    bones_spawn(e->px, e->py);
    e->alive = 0;
}

static void spike_hurt(int i, int b)
{
    (void)b;
    int x = P.spikes[i].x, y = P.spikes[i].y;
    int s = skel_at(x, y);
    if (s >= 0) {
        skel_broken(s);
        return;
    }
    if (P.player.x == x && P.player.y == y && !P.dead)
        spiked();
}

static void spikes_toggle(void)
{
    for (int i = 0; i < P.nspikes; i++) {
        if (P.spikes[i].up) {
            if (!P.spikes[i].stat) {
                P.spikes[i].up = 0;
                anim_trigger(&P.spikes[i].anim, P_spikeDescend);
            } else {
                timer_after(0.14f, spike_hurt, i, 0);
            }
        } else {
            P.spikes[i].up = 1;
            anim_trigger(&P.spikes[i].anim, P_spikeAscend);
            timer_after(0.14f, spike_hurt, i, 0);
        }
    }
}

static void waiter(void)
{
    spikes_toggle();
    timer_after(0.14f, waiter_mid, 0, 0);
}

/* ------------------------------------------------------------ death / restart / win */
static int advance_target;
static void game_over(int target);
void game_over_ext(int target) { game_over(target); }

static void advance_now(int a, int b)
{
    (void)b;
    if (a > -1)
        scene_request(a);
    else
        scene_request(g_scene + 2 + a);
}

static void game_over_door(int target, int b)
{
    (void)b;
    door_trigger_close();
    timer_after(1.0f, advance_now, target, 0);
}

static void game_over(int target)
{
#ifdef BOSS_DEBUG
    printf("game_over target=%d frozen=%d will=%d\n", target, P.frozen, P.will);
#endif
    P.un_menuable = 1;
    if (!P.frozen) {
        P.frozen = 1;
        sfx_play(SFX_player_death_01, 0);
        sfx_mute_new(1);
        /* "death" prefab: dark screen, DeathGround strike 3 units up, fading white flash */
        death_t = 0;
        death_on = 1;
        P.hero_hidden = 1;
        P.dead = 1;
        cam_shake(1.0f, 0.1f);
        timer_after(0.9f, game_over_door, target, 0);
    } else {
        game_over_door(target, 0);
    }
}

void player_dialogue_restart(int target)
{
    game_over(target);
}

void puzzle_restart(void)
{
    if (g_scene == 27)
        return;   /* hellm07: no restart object */
    if (!P.un_menuable && !P.in_menu && P.restartable) {
        P.frozen = 1;
        game_over(-2);
    }
}

static void win_door(int a, int b) { (void)a; (void)b; door_trigger_close(); timer_after(1.0f, advance_now, -1, 0); }
static void win_speed(int step, int b)
{
    (void)b;
    if (step < 2) {
        g_time_scale += 0.4f;
        timer_after(0.05f, win_speed, step + 1, 0);
    } else {
        g_time_scale = 1.0f;
        timer_after(1.2f, win_door, 0, 0);
    }
}
static void win_slow2(int a, int b) { (void)a; (void)b; win_speed(0, 0); }
static void win_slow(int a, int b) { (void)a; (void)b; g_time_scale = 0.2f; timer_after(0.1f, win_slow2, 0, 0); }
static void win_shake(int a, int b) { (void)a; (void)b; cam_shake(0.14f, 0.1f); timer_after(0.2f, win_slow, 0, 0); }

void player_victory(void)
{
    P.un_menuable = 1;
    anim_trigger(&P.player.anim, P_playerWin);
    sfx_play(SFX_succub_capture_01, 0);
    timer_after(1.5f, win_shake, 0, 0);
    (void)advance_target;
}

/* ------------------------------------------------------------ triggers */
static void check_triggers(void)
{
    int x = P.player.x, y = P.player.y;
    ritual_step();
    if (ritual_done && S->nritual && x == S->portal_x && y == S->portal_y && !portal_entered) {
        /* portalEnter: blackout, music off, next scene after 2 s */
        portal_entered = 1;
        P.frozen = 1;
        P.un_menuable = 1;
        music_change(-1);
        P.hero_hidden = 1;
        timer_after(2.0f, portal_next, 0, 0);
        return;
    }
    if (P.key_alive && x == key_x && y == key_y) {
        sfx_play(SFX_key_pick_up_01, 0);
        P.key_master = 1;
        vfx_spawn(CLIP_KEYVFX, wxc(key_x), wyc(key_y), 7, 1);
        P.key_alive = 0;
    }
    if (P.lock_alive && x == lock_x && y == lock_y) {
        sfx_play(SFX_door_opening_01, 0);
        P.key_master = 1;
        vfx_spawn(CLIP_KEYVFX, wxc(lock_x), wyc(lock_y), 7, 1);
        P.lock_alive = 0;
    }
    for (int i = 0; i < S->ntrig; i++) {
        const TrigDef *t = &S->trig[i];
        if (t->kind == TRIG_GOAL && t->x == x && t->y == y && t->dlg >= 0 && !P.frozen) {
            P.frozen = 1;
            dlg_start(t->dlg);
        } else if (t->kind == TRIG_OTHER && x >= t->x && x < t->x + (t->w ? t->w : 1) && t->y == y && t->dlg >= 0 &&
                   !P.frozen && (trig_on & (1u << i)) && !(trig_used & (1u << i))) {
            /* epilogue: each girl's pancake talk plays once */
            cur_trig = i;
            P.frozen = 1;
            dlg_start(t->dlg);
        } else if (t->kind == TRIG_SECRET && t->x == x && t->y == y && t->dlg >= 0 && !P.frozen &&
                   !(trig_used & (1u << i))) {
            /* after 888 the original moves the inscription 20 units away */
            trig_used |= 1u << i;
            P.frozen = 1;
            dlg_start(t->dlg);
        }
    }
}

static void starter(int a, int b)
{
    (void)a; (void)b;
    P.in_menu = 0;
    P.un_menuable = 0;
    P.player_turn = 1;
}

static void try_move(int dx, int dy)
{
    Ent *pl = &P.player;
    if (dx < 0) P.facing_left = 1;
    if (dx > 0) P.facing_left = 0;
    int tx = pl->x + dx, ty = pl->y + dy;
    if (S->kind == SK_BOSS && boss_try_kick(tx, ty)) {
        anim_trigger(&pl->anim, P_playerKick);
        waiter();
        return;
    }
    int pi = lab_pillar_at(tx, ty);
    if (pi >= 0) {
        anim_trigger(&pl->anim, P_playerKick);
        lab_kick_pillar(pi, dx, dy);
        waiter();
        return;
    }
    if (is_wall(tx, ty)) {
        /* void Asset (voidSpace): no kick, no cost */
        P.player_turn = 1;
        return;
    }
    if (is_lock(tx, ty) && lock_blocks()) {
        anim_trigger(&pl->anim, P_playerKick);
        sfx_play_any(SND_DOOR_KICK, 3, 1);
        vfx_spawn_any(CLIP_HIT_SMALL, 2, wxc(tx), wyc(ty), 7, 1);
        /* the lock shakes in place; drawn via lock_shake */
        waiter();
        return;
    }
    int b = box_at(tx, ty), s = skel_at(tx, ty);
    if (b >= 0 || s >= 0) {
        Ent *e = b >= 0 ? &P.boxes[b] : &P.skel[s];
        anim_trigger(&pl->anim, P_playerKick);
        sfx_play_any(b >= 0 ? SND_STONE_KICK : SND_ENEMY_KICK, 3, 1);
        int nx = tx + dx, ny = ty + dy;
        int blocked = puzzle_cell_blocked(nx, ny);
        if (!blocked) {
            vfx_spawn_any(CLIP_HIT, 2, e->px, e->py, 7, 1);
            if (b >= 0) {
                ent_begin_move(e, nx, ny, SND_STONE_MOVE, 3);
            } else {
                /* breakable faces the kicker */
                if (dx > 0) e->flip = 1;
                if (dx < 0) e->flip = 0;
                anim_trigger(&e->anim, P_assetKick);
                ent_begin_move(e, nx, ny, 0, 0);
            }
        } else if (s >= 0) {
            skel_broken(s);
        } else {
            vfx_spawn_any(CLIP_HIT_SMALL, 2, e->px, e->py, 7, 1);
            e->bx = e->px; e->by = e->py;
            e->shake = 0.1f;
        }
        waiter();
        return;
    }
    anim_trigger(&pl->anim, P_playerMove);
    ent_begin_move(pl, tx, ty, SND_MOVE, 1);
    check_triggers();
    waiter();
}

void puzzle_advice(void)
{
    if (S->lab)
        return;   /* pauseMenu: advice object inactive on the lab floors */
    if (P.in_menu || P.frozen || P.un_menuable || hint_moved)
        return;
    for (int i = 0; i < S->ntrig; i++) {
        const TrigDef *t = &S->trig[i];
        if (t->kind == TRIG_HINT && t->dlg >= 0) {
            P.frozen = 1;
            dlg_start(t->dlg);
            return;
        }
    }
}

void puzzle_start_goal(void)
{
    for (int i = 0; i < S->ntrig; i++) {
        const TrigDef *t = &S->trig[i];
        if (t->kind == TRIG_GOAL && t->dlg >= 0) {
            P.frozen = 1;
            dlg_start(t->dlg);
            return;
        }
    }
}

void puzzle_skip(void)
{
    P.cheater = 1;
    if (lab_active()) {
        lab_skip();
        return;
    }
    for (int i = 0; i < S->ntrig; i++) {
        const TrigDef *t = &S->trig[i];
        if (t->kind == TRIG_GOAL && t->dlg >= 0) {
            P.frozen = 1;
            dlg_start(t->dlg);
            return;
        }
    }
}

void puzzle_update(void)
{
    float dt = DT * g_time_scale;
    if (ui_pause_open())
        dt = 0;
    if (!started || P.in_menu == 1) {
        if (!started) {
            started = 1;
            timer_after(0.5f, starter, 0, 0);
            /* scenes that open on the trigger tile (menus, interludes) */
            for (int i = 0; i < S->ntrig; i++) {
                const TrigDef *t = &S->trig[i];
                if (t->kind == TRIG_GOAL && t->x == P.player.x && t->y == P.player.y && t->dlg >= 0) {
                    P.frozen = 1;
                    dlg_start(t->dlg);
                }
            }
        }
    }
    ent_slide(&P.player, dt);
    anim_update(&P.player.anim, dt, 0, 0);
    bones_update(dt);
    if (death_on)
        death_t += dt;
    for (int i = 0; i < P.nboxes; i++) {
        Ent *e = &P.boxes[i];
        ent_slide(e, dt);
        if (e->shake > 0) {
            e->shake -= 0.02f * dt * 60.0f;
            if (e->shake <= 0) { e->shake = 0; e->px = e->bx; e->py = e->by; }
        }
    }
    for (int i = 0; i < P.nskel; i++) {
        ent_slide(&P.skel[i], dt);
        anim_update(&P.skel[i].anim, dt, 0, 0);
    }
    for (int i = 0; i < P.nspikes; i++)
        anim_update(&P.spikes[i].anim, dt, 0, 0);
    anim_update(&P.key_anim, dt, 0, 0);
    if (P.will_scale > 0) {
        P.will_scale -= 0.05f * dt * 60.0f;
        if (P.will_scale < 0) P.will_scale = 0;
    }
    if (P.will_red > 0) { P.will_red -= 0.05f * dt * 60.0f; if (P.will_red < 0) P.will_red = 0; }
    if (P.player_red > 0) { P.player_red -= 0.05f * dt * 60.0f; if (P.player_red < 0) P.player_red = 0; }

    if (S->kind != SK_PUZZLE && S->kind != SK_BOSS && S->kind != SK_ABYSS)
        return;
    if (!P.un_menuable && !P.in_menu && !ui_pause_open()) {
        if (in_pressed & BTN_RESTART)
            puzzle_restart();
        else if (in_pressed & BTN_ADVICE)
            puzzle_advice();
    }
    if (!P.player_turn || P.frozen || P.in_menu || ui_pause_open() || P.player.moving)
        return;
    int dx = 0, dy = 0;
    /* one step per press: holding a direction does not keep walking */
    if (in_pressed & BTN_LEFT) dx = -1;
    else if (in_pressed & BTN_RIGHT) dx = 1;
    else if (in_pressed & BTN_UP) dy = 1;
    else if (in_pressed & BTN_DOWN) dy = -1;
    if (!dx && !dy)
        return;
    P.player_turn = 0;
    if (P.will == 0)
        game_over(-2);
    else
        try_move(dx, dy);
}

void puzzle_reset_started(void) { started = 0; }

int puzzle_will_text(char *buf)
{
    if (S->will > 50)
        strcpy(buf, "XX");
    else if (S->lab && will_changed)
        sprintf(buf, P.will == 0 ? "XX" : (P.will < 10 ? "0%d" : "%d"), P.will);
    else if (P.will == 0)
        strcpy(buf, "X");
    else
        sprintf(buf, "%d", P.will);
    return 0;
}

static void draw_ent_anim(Ent *e, float sx, uint32_t col)
{
    AnimOut o;
    anim_eval(&e->anim, &o);
    float x = e->px, y = e->py;
    if (e->shake > 0) {
        x += ((rand() % 200) / 100.0f - 1.0f) * e->shake;
        y += ((rand() % 200) / 100.0f - 1.0f) * e->shake;
    }
    gfx_sprite(o.spr, gfx_wx(x), gfx_wy(y), sx, 1, col);
}

void puzzle_draw_layer(int layer)
{
    if (!S)
        return;
    if (layer == 3) {
        for (int i = 0; i < P.nspikes; i++) {
            AnimOut o;
            anim_eval(&P.spikes[i].anim, &o);
            gfx_sprite(o.spr, gfx_wx(wxc(P.spikes[i].x)), gfx_wy(wyc(P.spikes[i].y)), 1, 1, WHITE);
        }
    } else if (layer == 5) {
        if (P.key_alive) {
            AnimOut o;
            anim_eval(&P.key_anim, &o);
            gfx_sprite(o.spr, gfx_wx(wxc(key_x)), gfx_wy(wyc(key_y)), 1, 1, WHITE);
        }
        if (P.lock_alive)
            gfx_sprite(SPR_backup_lockbox, gfx_wx(wxc(lock_x)), gfx_wy(wyc(lock_y)), 1, 1, WHITE);
        /* draw back-to-front by row so lower objects overlap upper ones */
        for (int row = 40; row >= -40; row--) {
            for (int i = 0; i < P.nboxes; i++) {
                Ent *e = &P.boxes[i];
                if (!e->alive || (int)floorf(e->py - gy + 0.5f) != row)
                    continue;
                float x = e->px, y = e->py;
                if (e->shake > 0) {
                    x += ((rand() % 200) / 100.0f - 1.0f) * e->shake;
                    y += ((rand() % 200) / 100.0f - 1.0f) * e->shake;
                }
                gfx_sprite(e->spr, gfx_wx(x), gfx_wy(y), e->flip ? -1 : 1, 1, WHITE);
            }
            for (int i = 0; i < P.nskel; i++) {
                Ent *e = &P.skel[i];
                if (e->alive && (int)floorf(e->py - gy + 0.5f) == row)
                    draw_ent_anim(e, e->flip ? -1 : 1, WHITE);
            }
        }
    } else if (layer == 9) {
        if (portal_entered)
            gfx_rect(0, 0, SCR_W, SCR_H, RGBA(2, 2, 28, 255));
        if (death_on) {
            gfx_rect(0, 0, SCR_W, SCR_H, RGBA(2, 2, 28, 255));
            static int clip = -2;
            if (clip == -2)
                clip = clip_find("deathGround_anim");
            int spr = clip_sprite_at(clip, death_t * 1.25f, 0);
            gfx_sprite(spr, gfx_wx(P.player.px), gfx_wy(P.player.py + 3.0f), 1, 1, WHITE);
            float fa = 0.392f - death_t * 60.0f * 0.05f;
            if (fa > 0)
                gfx_rect(0, 0, SCR_W, SCR_H, RGBA(255, 255, 255, (int)(fa * 255)));
        }
    } else if (layer == 7) {
        for (int i = 0; i < MAX_BONES; i++) {
            if (bones[i].life <= 0)
                continue;
            float sc = 1.0f - bones[i].t; /* particleFade */
            if (sc <= 0)
                continue;
            gfx_sprite(bones[i].spr, gfx_wx(bones[i].x), gfx_wy(bones[i].y), bones[i].flip ? -sc : sc, sc, WHITE);
        }
    } else if (layer == 6) {
        if (!P.hero_hidden) {
            int gb = 255 - (int)(P.player_red * 255);
            draw_ent_anim(&P.player, P.facing_left ? -1 : 1, RGBA(255, gb, gb, 255));
        }
    }
}

/* autotest helper */
void puzzle_teleport(int x, int y)
{
    P.player.x = x;
    P.player.y = y;
    P.player.px = wxc(x);
    P.player.py = wyc(y);
    P.player.moving = 0;
}
