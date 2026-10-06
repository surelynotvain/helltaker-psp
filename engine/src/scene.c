/* Scene runtime: loading, coroutine timers, decor animators, vfx, camera shake, draw order. */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "game.h"
#include "gfx.h"
#include "audio.h"
#ifdef MEM_DEBUG
#include <pspsysmem.h>
#endif

int g_scene = -1;
const SceneDef *S;
float g_time_scale = 1.0f;
static int pending_scene = -1;

int CLIP_DUST[3], CLIP_HIT[2], CLIP_HIT_SMALL[2], CLIP_BLOOD[3], CLIP_KEYVFX, CLIP_DEATH, CLIP_LOVEPLOSION,
    CLIP_SUCCESS, CLIP_BOOPER, CLIP_BOOPER_CLICK, CLIP_BONE;

/* ------------------------------------------------------------ timers */
#define MAX_TIMERS 48
static struct { float left; TimerFn fn; int a, b; } timers[MAX_TIMERS];

void timer_after(float sec, TimerFn fn, int a, int b)
{
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!timers[i].fn) {
            timers[i].left = sec;
            timers[i].fn = fn;
            timers[i].a = a;
            timers[i].b = b;
            return;
        }
    }
}

void timer_clear(void) { memset(timers, 0, sizeof timers); }

static void timers_update(float dt)
{
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (timers[i].fn) {
            timers[i].left -= dt;
            if (timers[i].left <= 0) {
                TimerFn fn = timers[i].fn;
                timers[i].fn = 0;
                fn(timers[i].a, timers[i].b);
            }
        }
    }
}

/* ------------------------------------------------------------ camera shake */
static float shake_dur, shake_init, shake_amt;
static unsigned srng = 12345;
static float frand(void)
{
    srng = srng * 1103515245u + 12345u;
    return ((srng >> 8) & 0xFFFF) / 32768.0f - 1.0f;
}

void cam_shake(float dur, float amount)
{
    shake_dur = shake_init = dur;
    shake_amt = amount;
}

static void shake_update(float dt)
{
    if (shake_dur > 0) {
        gfx_camera_shake(frand() * shake_amt, frand() * shake_amt);
        shake_dur -= dt;
        shake_amt -= shake_amt * dt / shake_init;
        if (shake_dur <= 0)
            gfx_camera_shake(0, 0);
    }
}

/* ------------------------------------------------------------ vfx */
#define MAX_VFX 40
static struct { int clip; float t, x, y, sx; int layer; int active; } vfx[MAX_VFX];

void vfx_spawn(int clip, float wx, float wy, int layer, float sx)
{
    if (clip < 0)
        return;
    for (int i = 0; i < MAX_VFX; i++) {
        if (!vfx[i].active) {
            vfx[i].clip = clip; vfx[i].t = 0; vfx[i].x = wx; vfx[i].y = wy;
            vfx[i].sx = sx; vfx[i].layer = layer; vfx[i].active = 1;
            return;
        }
    }
}

void vfx_spawn_any(const int *clips, int n, float wx, float wy, int layer, int jitter)
{
    if (n <= 0)
        return;
    int c = clips[(unsigned)rand() % n];
    if (jitter) { /* RandomizeVfx(random: true): +-0.1 */
        wx += frand() * 0.1f;
        wy += frand() * 0.1f;
    }
    vfx_spawn(c, wx, wy, layer, 1);
}

static void vfx_update(float dt)
{
    for (int i = 0; i < MAX_VFX; i++) {
        if (vfx[i].active) {
            vfx[i].t += dt;
            /* AnimDestroy: one play-through regardless of the clip's loop flag */
            if (vfx[i].t >= clip_length(vfx[i].clip))
                vfx[i].active = 0;
        }
    }
}

static void vfx_draw(int layer)
{
    for (int i = 0; i < MAX_VFX; i++) {
        if (vfx[i].active && vfx[i].layer == layer) {
            int spr = clip_sprite_at(vfx[i].clip, vfx[i].t, 0);
            gfx_sprite(spr, gfx_wx(vfx[i].x), gfx_wy(vfx[i].y), vfx[i].sx, 1, WHITE);
        }
    }
}

/* ------------------------------------------------------------ decor */
#define MAX_DECOR 400
static struct { Animator anim; int has; int hidden; float glow; int getting; float get_t; DecorEventFn evt; } decor[MAX_DECOR];

/* animation events of a scene object's Animator (e.g. AMDBody / cannonSoundScript receivers) */
void decor_set_events(int idx, DecorEventFn fn)
{
    if (idx >= 0 && idx < MAX_DECOR && idx < S->ndecor)
        decor[idx].evt = fn;
}

void decor_set_hidden(int idx, int hidden)
{
    if (idx >= 0 && idx < MAX_DECOR && idx < S->ndecor)
        decor[idx].hidden = hidden;
}

static void decor_event(void *u, int fn, int arg)
{
    int i = (int)(intptr_t)u;
    if (decor[i].evt)
        decor[i].evt(i, fn, arg);
}

/* AnimSound on Judgement's landing (GoalSprite.demonLanding): singlePlay -> boss_judgement_land_01 */
static void landing_event(int idx, int fn, int arg)
{
    (void)idx; (void)arg;
    if (fn == EVT_singlePlay)
        sfx_play(SFX_boss_judgement_land_01, 0);
}

static void decor_init(void)
{
    memset(decor, 0, sizeof decor);
    for (int i = 0; i < S->ndecor && i < MAX_DECOR; i++) {
        if (S->decor[i].ctrl >= 0) {
            anim_init(&decor[i].anim, S->decor[i].ctrl);
            decor[i].has = 1;
            if (S->decor[i].kind == 9)
                decor[i].evt = landing_event;
        }
    }
}

Animator *decor_anim(int idx)
{
    if (idx < 0 || idx >= S->ndecor || idx >= MAX_DECOR || !decor[idx].has)
        return 0;
    return &decor[idx].anim;
}

void decor_restart_kind(int kind)
{
    for (int i = 0; i < S->ndecor && i < MAX_DECOR; i++)
        if (S->decor[i].kind == kind && decor[i].has)
            anim_init(&decor[i].anim, S->decor[i].ctrl);
}

void decor_demons_get(void)
{
    /* Demon.GetVfx: glow + star build-up, demonDestroy after 0.5 s, then love-plosion */
    for (int i = 0; i < S->ndecor && i < MAX_DECOR; i++) {
        const DecorDef *d = &S->decor[i];
        if (d->kind == 1 || (d->ctrl >= 0 && !strncmp(g_ctrls[d->ctrl].name, "anim_", 5))) {
            decor[i].getting = 1;
            decor[i].get_t = 0;
        }
    }
}

void decor_hide_love_signs(uint32_t demons)
{
    for (int i = 0; i < S->ndecor && i < MAX_DECOR; i++) {
        const DecorDef *d = &S->decor[i];
        if (d->sign >= 0 && (demons >> d->sign) & 1)
            decor[i].hidden = 1;
    }
}

static void decor_update(float dt)
{
    for (int i = 0; i < S->ndecor && i < MAX_DECOR; i++) {
        if (decor[i].has)
            anim_update(&decor[i].anim, dt, decor[i].evt ? decor_event : 0, (void *)(intptr_t)i);
        if (decor[i].getting) {
            float t0 = decor[i].get_t;
            decor[i].get_t += dt;
            const DecorDef *d = &S->decor[i];
            if (d->kind != 1 && t0 < 0.5f && decor[i].get_t >= 0.5f)
                vfx_spawn(CLIP_LOVEPLOSION, d->x / 256.0f, d->y / 256.0f + 0.2f, 7, 1);
        }
    }
}

static float cam_y0, cam_dy;
static int has_pusher;

static void specials_init(void)
{
    has_pusher = 0;
    cam_y0 = S->cam_y / 256.0f;
    cam_dy = 0;
    for (int i = 0; i < S->nspecial; i++)
        if (S->special[i].kind == SP_PUSHER)
            has_pusher = 1;
}

/* pusherScript keeps the camera rig at the player's height; fireIgniter lights torch pairs */
static void specials_update(void)
{
    if (boss_active())
        cam_dy = boss_cam_dy();
    if (has_pusher) {
        float start_y = S->py + S->gy / 256.0f;
        cam_dy = P.player.py - start_y;
        gfx_camera(S->cam_x / 256.0f, cam_y0 + cam_dy);
    }
    for (int i = 0; i < S->nspecial; i++) {
        const SpecialDef *sp = &S->special[i];
        if (sp->kind == SP_FIRE && P.player.y >= sp->y) {
            for (int k = 0; k < S->ndecor && k < MAX_DECOR; k++)
                if (S->decor[k].group == i && decor[k].has && !decor[k].anim.params[P_lit])
                    anim_set_bool(&decor[k].anim, P_lit, 1);
        }
    }
}

static void decor_draw(int layer)
{
    for (int i = 0; i < S->ndecor && i < MAX_DECOR; i++) {
        const DecorDef *d = &S->decor[i];
        if (d->layer != layer || decor[i].hidden || (d->owner >= 0 && d->owner < MAX_DECOR && decor[d->owner].hidden))
            continue;
        if (d->kind == 6 || (d->kind == 7 && !ritual_complete()) || (d->kind == 8 && !boss_heart_alive(d->group)))
            continue;
        if (d->kind == 9 && boss_landing_state() != 1)
            continue;
        if (d->kind == 12 && !lab_pillar_alive(d->group))
            continue;
        int spr = d->spr;
        float a = d->a / 255.0f, sx = d->sx / 256.0f, sy = d->sy / 256.0f;
        float rot = d->prot + d->rot;   /* world z rotation from the scene */
        int mirrored = (d->psx < 0) != (d->psy < 0);
        float x = d->x / 256.0f, y = d->y / 256.0f;
        /* the Animator that owns this object: itself (root curves) or an ancestor (child-path curves) */
        const Animator *an = 0;
        uint32_t path = 0;
        if (d->kind == 5 && d->owner >= 0 && d->owner != i && boss_giga_anim(d->group)) {
            /* gigachain art follows the boss's chain animator (hit / break) */
            an = boss_giga_anim(d->group);
            path = d->path;
        } else if (d->owner >= 0 && d->owner < MAX_DECOR && d->owner < S->ndecor && d->owner != i && decor[d->owner].has) {
            an = &decor[d->owner].anim;
            path = d->path;
        } else if (decor[i].has) {
            an = &decor[i].anim;
        }
        if (d->kind == 11 && !an)
            continue;
        if (an) {
            AnimOut o;
            anim_eval_path(an, path, &o);
            if (!o.active || o.spr_null || (d->kind == 11 && !o.has_active))
                continue;
            if (o.spr != SPR_NONE)
                spr = o.spr;
            if (o.has_alpha)
                a = o.a;
            if (o.has_posx)
                x = d->px + o.dx * d->psx;
            if (o.has_posy)
                y = d->py + o.dy * d->psy;
            if (o.has_sx)
                sx = o.sx * d->psx;
            if (o.has_sy)
                sy = o.sy * d->psy;
            if (o.has_rot)
                rot = d->prot + o.rot;
            /* one level of inheritance: an animated parent (e.g. sinWheelControl) turns its child */
            if (d->ppath) {
                AnimOut po;
                anim_eval_path(an, d->ppath, &po);
                if (po.has_rot)
                    rot += po.rot;
                /* Examtaker: an animated parent scale (SpriteMask holoMask opening) scales the child */
                if (g_scene >= 20 && po.has_sx)
                    sx *= po.sx;
                if (g_scene >= 20 && po.has_sy)
                    sy *= po.sy;
            }
        }
        /* rotate-then-mirror (Unity child under a flipped parent) == mirror-then-rotate the other way */
        if (mirrored)
            rot = -rot;
        if (d->kind == 10 && boss_landing_state() == 2)
            a = 1.0f;   /* GoalSprite.demonAlpha */
        if (d->kind == 4 || d->kind == 8 || d->kind == 9 || d->kind == 10)
            y += cam_dy;
        float gflash = 0;
        if (d->kind == 5) {
            float gdx, gdy;
            int st = boss_giga_offset(d->group, &gdx, &gdy);
            if (st <= 0)
                continue;
            x += gdx;
            y += gdy;
            gflash = boss_giga_flash(d->group);
        }
        if (decor[i].getting) {
            float t = decor[i].get_t;
            if (d->kind == 1) { /* demonGlow: flash in, stays white */
                a = t < 0.5f ? t / 0.5f : 1.0f;
                if (t > 1.0f)
                    continue;
                gfx_silhouette(d->white);   /* Font Material: a pure white silhouette */
                gfx_sprite_additive(spr, gfx_wx(x), gfx_wy(y), sx, sy, RGBA(255, 255, 255, (int)(a * 255)));
                gfx_silhouette(0);
                continue;
            }
            if (t > 1.0f)
                continue; /* Object.Destroy(demon) */
        }
        if (spr == SPR_NONE || a <= 0)
            continue;
        uint32_t col = RGBA(d->r, d->g, d->b, (int)(a * 255));
        if (gflash > 0)
            col = WHITE;
        if (d->white)
            gfx_silhouette(1);
        if (d->kind == 1)
            gfx_sprite_additive(spr, gfx_wx(x), gfx_wy(y), sx, sy, col);
        else if (rot > 0.01f || rot < -0.01f)
            gfx_sprite_rot(spr, gfx_wx(x), gfx_wy(y), sx, sy, rot, col);
        else
            gfx_sprite(spr, gfx_wx(x), gfx_wy(y), sx, sy, col);
        if (d->white)
            gfx_silhouette(0);
    }
}

/* ------------------------------------------------------------ scene flow */
void scene_request(int index)
{
    pending_scene = index;
}

int scene_kind(void) { return S ? S->kind : SK_UNSUPPORTED; }

static void scene_load(int n)
{
    if (n < 0 || n >= NUM_SCENES || g_scenes[n].kind == SK_UNSUPPORTED)
        n = 0;
    g_scene = n;
    S = &g_scenes[n];
#ifdef BOSS_DEBUG
    printf("load scene %d\n", n);
#endif
    timer_clear();
    memset(vfx, 0, sizeof vfx);
    if (S->kind == SK_MENU)
        g_amd_phase = 0;   /* CameraShake.Awake (mainMenu) */
    g_time_scale = 1.0f;
    gfx_camera_shake(0, 0);
    shake_dur = 0;
    gfx_bundles_require(S->bundles, S->nbundles);
    gfx_camera(S->cam_x / 256.0f, S->cam_y / 256.0f);
    sfx_loop_stop();
    decor_init();
    specials_init();
    /* CameraShake.Start */
    music_duck(0);
    if (music_current() != S->music)
        music_change(S->music);
    sfx_mute_new(0);
    dlg_reset();
    ui_reset();
#ifdef MEM_DEBUG
    {
        /* largest malloc-able block: binary search */
        unsigned lo = 0, hi = 32u << 20;
        while (hi - lo > 16384) {
            unsigned mid = (lo + hi) / 2;
            void *p = malloc(mid);
            if (p) { free(p); lo = mid; } else hi = mid;
        }
        printf("MEM scene %d largest free block %u KB\n", n, lo / 1024);
    }
#endif
    puzzle_load();
    lab_load();
    amd_load();
    boss_load();
    /* pauseMenu.Start: DLCChapter (hellm01 = 0 .. hellm07 = 6) raises dlcProgress */
    if (n >= 21 && n <= 27 && n - 21 > g_save.dlc_progress) {
        g_save.dlc_progress = n - 21;
        save_write();
    }
    if (n > g_save.chapter_reached && n <= 19) {
        g_save.chapter_reached = n;
        save_write();
    }
}

void game_init_clips(void)
{
    static const char *dust[3] = {"dust_anim1", "dust_anim2", "dust_anim3"};
    for (int i = 0; i < 3; i++) CLIP_DUST[i] = clip_find(dust[i]);
    CLIP_HIT[0] = clip_find("impact01");
    CLIP_HIT[1] = clip_find("impact02");
    CLIP_HIT_SMALL[0] = clip_find("hit_small_1");
    CLIP_HIT_SMALL[1] = clip_find("hit_small_2");
    CLIP_BLOOD[0] = clip_find("blood01");
    CLIP_BLOOD[1] = clip_find("blood02");
    CLIP_BLOOD[2] = clip_find("blood03");
    CLIP_KEYVFX = clip_find("key_vfx");
    CLIP_DEATH = clip_find("deathGround_anim");
    CLIP_LOVEPLOSION = clip_find("lovePlosion");
    CLIP_SUCCESS = clip_find("successAnim");
    CLIP_BOOPER = clip_find("booper_idle");
    CLIP_BOOPER_CLICK = clip_find("booper_click");
    CLIP_BONE = clip_find("skullAnim");
}

void game_start(int scene)
{
    game_init_clips();
    scene_load(scene);
}

/* main menu + START (only with a LANG.PAK): the door closes, the language picker runs behind it,
 * then the main menu comes back through the opening door */
static int lang_pick;   /* 1: door closing, 2: picker due at the next scene load */
static void lang_pick_now(int a, int b) { (void)a; (void)b; lang_pick = 2; scene_request(0); }

static void lang_pick_update(void)
{
    if (lang_pick) {
        in_pressed = 0;   /* the menu dialogue stays put while the door closes */
        return;
    }
    if (S->kind == SK_MENU && (in_pressed & BTN_START) && gfx_lang_name() && !widget_active() && pending_scene < 0) {
        lang_pick = 1;
        in_pressed = 0;
        P.frozen = 1;
        door_trigger_close();
        timer_after(1.0f, lang_pick_now, 0, 0);
    }
}

void game_frame(void)
{
    lang_pick_update();
    float dt = DT * g_time_scale;
    if (ui_pause_open())
        dt = 0;
    timers_update(dt);
    shake_update(DT);
    vfx_update(dt);
    decor_update(dt);
    puzzle_update();
    lab_update(dt);
    amd_update(dt);
    boss_update(dt);
    specials_update();
    dlg_update();
    ui_update();
    door_update();
    if (pending_scene >= 0) {
        int n = pending_scene;
        pending_scene = -1;
        if (lang_pick == 2) {
            lang_choose();
            lang_pick = 0;
        }
        scene_load(n);
    }
}

void game_draw(void)
{
#ifdef BOSS_DEBUG
    static int fr;
    if (fr < 3) printf("draw %d begin\n", fr);
#endif
    gfx_begin(BG_COLOR);
    for (int layer = 0; layer <= 8; layer++) {
        decor_draw(layer);
#ifdef BOSS_DEBUG
        if (fr < 3) printf(" layer %d decor ok\n", layer);
#endif
        puzzle_draw_layer(layer);
        lab_draw_layer(layer);
        amd_draw_layer(layer);
        boss_draw_layer(layer);
        vfx_draw(layer);
#ifdef BOSS_DEBUG
        if (fr < 3) printf(" layer %d done\n", layer);
#endif
    }
#ifdef BOSS_DEBUG
    fr++;
#endif
    lab_draw_hud();   /* lab monitor counter: world-space canvas, under the dialogue */
    dlg_draw_blackout();
    puzzle_draw_layer(9);
    decor_draw(9);
    dlg_draw_world();
    vfx_draw(9);
    ui_draw();
    boss_draw_ui();
    dlg_draw_ui();
    ui_draw_pause();
    door_draw();
    gfx_end();
}
