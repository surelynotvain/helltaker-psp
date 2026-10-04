/* Dialogue runner rebuilt from GoalSprite + D_charaScript + dialogue prefabs. */
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <pspkernel.h>
#include "game.h"
#include "gfx.h"
#include "audio.h"

/* PSP layout (canvas 1920x1080 scaled by 0.25 around the screen centre) */
#define UI_X(cx) (240.0f + (cx) * 0.25f)
#define UI_Y(cy) (136.0f - (cy) * 0.25f)
#define NAME_RGB RGBA(230, 77, 82, 255)
#define DEATH_RGB RGBA(206, 20, 16, 255)

int g_menu_returning;   /* MainMenuStart: second visit opens with "xxx" */

static const DlgDef *D;
static const DlgElem *E;
static int active, cur = -1, next_part;
static int waiting_input, waiting_choice;
static int first_dialogue, confirm_sound_needed, widget_exit, song_begins;
static float mask, mask_goal;       /* bgMask y scale / blackout alpha */
static int mask_running;
static int ui_hidden;               /* perishableUI "clear" */
static float ui_slide;              /* 0 shown .. 1 hidden */
static int submit_block;

/* talker: an instantiated dialogue prefab (g_prefabs) */
#define MAX_PARTS 32
static int talker_kind;             /* CHARA_* or 0 */
static int talker_spr = SPR_NONE;   /* newSprite override for the root part */
static float talker_dx, talker_a, talker_t;
static Animator part_anim[MAX_PARTS];
/* text */
static const char *name_text, *text1, *text2;
static int text_bad;
static float text_t = 1;
static int booper_on;
static float booper_t, booper_click_t = -1;
static int success_on;
static float success_t;
/* choices */
static int nbuttons, choice_idx;
static float buttons_t;
static int buttons_shown, buttons_hiding;
static float button_anim[4];
static const char *button_text[4];
static int button_effect[4];

int dlg_active(void) { return active; }
int dlg_ui_hidden(void) { return ui_hidden; }
float dlg_ui_slide(void) { return ui_slide; }

void dlg_reset(void)
{
    active = 0;
    cur = -1;
    D = 0;
    E = 0;
    waiting_input = waiting_choice = 0;
    mask = mask_goal = 0;
    mask_running = 0;
    ui_hidden = 0;
    ui_slide = 0;
    talker_kind = 0;
    talker_spr = SPR_NONE;
    name_text = text1 = text2 = 0;
    booper_on = 0;
    success_on = 0;
    nbuttons = 0;
    buttons_shown = 0;
    submit_block = 1;
}

static void open_mask(float end);
static void show(int a);

static void show_part2(int a, int b);
static void show_timer(int a, int b) { (void)b; show(a); }

int g_dlg_force_start = -1;   /* autotest: open a dialogue at a given element */

void dlg_start(int index)
{
    D = &S->dlg[index];
    E = &S->elems[D->first];
    active = 1;
    P.frozen = 1;
    /* animationTime 5.55 on the first element ducks the music (Manager.mute = 0.1) */
    if (E[0].anim_ms == 5550)
        music_duck(1);
    open_mask(1);
    sfx_play(SFX_dialogue_start_01, 0);
    first_dialogue = 1;
    text_bad = 0;
    int start = 0;
    if (D->chapter && !strcmp(D->chapter, "x"))
        start = g_menu_returning ? 10 : 8;
    else if (D->chapter && !strcmp(D->chapter, "xxx"))
        start = 10;
    else if (D->chapter && !strcmp(D->chapter, "dlcIntro") && g_save.dlc_progress != 0)
        start = 2;
    if (g_dlg_force_start >= 0) {
        start = g_dlg_force_start;
        g_dlg_force_start = -1;
    }
    timer_after(0.2f, show_timer, start, 0);
}

static void open_mask(float end)
{
    mask_goal = end;
    mask_running = 1;
    if (end == 0)
        ui_hidden = 0;   /* UIanimator "bring" */
    else
        ui_hidden = 1;   /* "clear" */
}

static void mask_done(void)
{
    mask_running = 0;
    if (mask_goal == 0) {
        active = 0;
        if (next_part == 777) {
            decor_demons_get();
            player_victory();
        } else if (next_part == 888) {
            /* epilogueNumber 666: an ancient inscription (ritual piece) was read */
            if (D->epilogue == 666 && g_scene >= 4 && g_scene <= 6) {
                g_save.rituals |= 1 << (g_scene - 4);
                save_write();
            }
            puzzle_dialogue_closed(D->epilogue);
            P.frozen = 0;
            P.player_turn = 1;
        }
        talker_kind = 0;
    } else {
        decor_hide_love_signs();
    }
}

/* AnimSound on the cutscene prefab: singlePlay(i) / twoPlay / fourPlay animation events */
static void prefab_sound(void *u, int fn, int arg)
{
    (void)u;
    const int16_t *s = g_prefab_snd[talker_kind];
    if (fn == EVT_singlePlay) {
        int k = arg == 2 ? 1 : arg == 3 ? 2 : 0;
        if (s[k] >= 0)
            sfx_play(s[k], 0);
    } else if (fn == EVT_twoPlay) {
        int k = 7 + (rand() & 1);
        if (s[k] >= 0)
            sfx_play(s[k], 0);
    } else if (fn == EVT_fourPlay) {
        int k = 3 + (rand() & 3);
        if (s[k] >= 0)
            sfx_play(s[k], 1);
    }
}

static void set_talker(int kind, int spr)
{
    const PrefabDef *pf = &g_prefabs[kind];
    talker_kind = kind;
    talker_spr = spr;
    talker_t = 0;
    for (int i = 0; i < pf->n && i < MAX_PARTS; i++)
        anim_init(&part_anim[i], g_prefab_parts[pf->first + i].ctrl);
    if (pf->slide) {
        talker_dx = 2.0f;   /* D_charaScript: spawn, then ease 2 units left while fading in */
        talker_a = 0;
    } else {
        talker_dx = 0;
        talker_a = 1;
    }
}

static void show(int a)
{
    const DlgElem *e = &E[a];
    cur = a;
    if (e->success == 1 || e->success == 3)
        sfx_play(SFX_dialogue_success_01, 0);
    else if (confirm_sound_needed)
        sfx_play(SFX_button_dialogue_confirm_01, 0);
    else if (e->success == 2)
        sfx_play(SFX_bad_end_screen_01, 0);
    else if (!first_dialogue && !widget_exit)
        sfx_play(SFX_dialogue_text_end_01, 0);
    confirm_sound_needed = 0;
    widget_exit = 0;
    first_dialogue = 0;
    if (e->chara) {
        if (e->success == 2) {
            P.restartable = 0;
            cam_shake(0.04f, 0.3f);
            text_bad = 1;
            booper_on = 0;
        }
        text1 = text2 = 0;
        name_text = 0;
        set_talker(e->chara, e->sprite);
        if (e->anim_ms > 100) {
            booper_on = 0;
            timer_after(e->anim_ms / 1000.0f, show_part2, a, 0);
            return;
        } else if (a == 0) {
            timer_after(0.2f, show_part2, a, 0);
            return;
        }
    } else if (e->sprite != SPR_NONE) {
        talker_spr = e->sprite;
    }
    show_part2(a, 0);
}

static void ready_input(int a, int b) { (void)a; (void)b; waiting_input = 1; }
static void ready_choice(int a, int b) { (void)a; (void)b; waiting_choice = 1; }

static void show_part2(int a, int b)
{
    (void)b;
    const DlgElem *e = &E[a];
    if (e->anim_ms == 10)
        music_change(-1);
    else if (e->anim_ms == 20)
        cam_shake(0.14f, 0.3f);
    if (e->ntext > 0) {
        if (e->success != 2) {
            if (!booper_on)
                booper_on = 1, booper_t = 0;
            else
                booper_click_t = 0;
            if (e->name)
                name_text = e->name;
        }
        if (e->success == 1 || e->success == 3) {
            success_on = e->success;
            success_t = 0;
        }
        text1 = e->text[0];
        text2 = e->ntext > 1 ? e->text[1] : 0;
        text_t = 0;
        next_part = e->outcome[0];
        timer_after(0.2f, ready_input, 0, 0);
    } else {
        booper_on = 0;
        if (D->buttons > 2) {
            text1 = text2 = 0;
            if (e->name)
                name_text = e->name;
        }
        nbuttons = e->nchoice < 4 ? e->nchoice : 4;
        choice_idx = 0;
        for (int i = 0; i < nbuttons; i++) {
            button_text[i] = e->choice[i];
            button_effect[i] = e->outcome[i];
            button_anim[i] = 0;
        }
        buttons_t = 0;
        buttons_shown = 1;
        buttons_hiding = 0;
        timer_after(0.4f, ready_choice, 0, 0);
    }
    if (song_begins) {
        if (D->abyss_music >= 0)
            music_change(D->abyss_music);
        song_begins = 0;
    }
}

void dlg_back_from_widget(int part)
{
    widget_exit = 1;
    show(part);
}

static void close_dialogue(void)
{
    text1 = text2 = 0;
    name_text = 0;
    booper_on = 0;
    open_mask(0);
}

void dlg_update(void)
{
    float dt = DT;
    if (!active && !mask_running)
        return;
    if (ui_pause_open())
        return;
    /* bgMask / blackout lerp, 0.15 per 60 Hz frame */
    if (mask_running) {
        mask += (mask_goal - mask) * 0.15f;
        if ((mask - mask_goal) * (mask - mask_goal) <= 0.0001f) {
            mask = mask_goal;
            mask_done();
        }
    }
    ui_slide += ((ui_hidden ? 1.0f : 0.0f) - ui_slide) * 0.15f;
    /* D_charaScript.SmoothShowup */
    if (talker_dx > 0.0001f || talker_a < 1) {
        talker_dx += (0 - talker_dx) * 0.2f;
        talker_a += (1 - talker_a) * 0.1f;
        if (talker_dx * talker_dx <= 0.0002f) {
            talker_dx = 0;
            talker_a = 1;
        }
    }
    talker_t += dt;
    if (talker_kind) {
        const PrefabDef *pf = &g_prefabs[talker_kind];
        for (int i = 0; i < pf->n && i < MAX_PARTS; i++)
            anim_update(&part_anim[i], dt, prefab_sound, 0);
    }
    text_t += dt;
    booper_t += dt;
    if (booper_click_t >= 0) {
        booper_click_t += dt;
        if (booper_click_t > clip_length(CLIP_BOOPER_CLICK))
            booper_click_t = -1;
    }
    success_t += dt;
    buttons_t += dt;
    for (int i = 0; i < nbuttons; i++) {
        float target = (i == choice_idx) ? 1.0f : 0.0f;
        button_anim[i] += (target - button_anim[i]) * 0.3f;
    }
    if (widget_active())
        return;
    if (waiting_input) {
        if (in_pressed & BTN_OK) {
            waiting_input = 0;
            success_on = 0;
            int np = next_part;
            if (np < 100) {
                show(np);
                return;
            }
            if (np > 1000) {
                song_begins = 1;
                show(np - 1000);
                return;
            }
            sfx_play(SFX_dialogue_text_end_01, 0);
            switch (np) {
            case 101: player_dialogue_restart(1); return;
            case 102: player_dialogue_restart(0); return;
            case 103: sceKernelExitGame(); return;
            case 104: player_dialogue_restart(-1); return;
            case 105: player_dialogue_restart(19); return;
            case 666: player_dialogue_restart(-2); return;
            case 555: widget2_open(D->chapter && !strcmp(D->chapter, "dlcIntro")); booper_on = 0; name_text = 0; return;
            default: break;
            }
            close_dialogue();
        }
    } else if (waiting_choice) {
        if (in_pressed & BTN_OK) {
            confirm_sound_needed = 1;
            waiting_choice = 0;
            buttons_hiding = 1;
            buttons_t = 0;
            int eff = button_effect[choice_idx];
            if (eff < 100) {
                buttons_shown = 0;
                show(eff);
            } else if (eff == 101) {
                buttons_shown = 0;
                name_text = 0;
                widget_open();
                sfx_play(SFX_button_dialogue_confirm_01, 0);
                confirm_sound_needed = 0;
            }
        } else if (in_pressed & (BTN_UP | BTN_DOWN)) {
            if (in_pressed & BTN_DOWN)
                choice_idx = choice_idx < D->max_index ? choice_idx + 1 : 0;
            else
                choice_idx = choice_idx > 0 ? choice_idx - 1 : D->max_index;
            if (choice_idx >= nbuttons)
                choice_idx = nbuttons - 1;
            sfx_play(SFX_button_dialogue_highlight_01, 0);
        }
    }
}

/* ------------------------------------------------------------------ drawing */
void dlg_draw_blackout(void)
{
    /* D_blackout (deathLayer, order 0): dark veil over the level */
    if (D && mask > 0.001f)
        gfx_rect(0, 0, SCR_W, SCR_H, RGBA(2, 2, 28, (int)(mask * 255)));
}

static void draw_bg_band(void)
{
    if (mask <= 0.001f || D == 0)
        return;
    float cx = gfx_wx(3.0f), cy = gfx_wy(4.0f);
    if (D->bg != SPR_NONE) {
        int h = gfx_sprite_h(D->bg);
        int band = (int)(h * mask + 0.5f);
        gfx_clip(0, (int)(cy - band * 0.5f), SCR_W, band);
        gfx_sprite(D->bg, cx, cy, 1, 1, WHITE);
        gfx_clip_reset();
    }
}

void dlg_draw_world(void)
{
    if (!D || (mask <= 0.001f && !active))
        return;
    draw_bg_band();
    if (!talker_kind)
        return;
    const PrefabDef *pf = &g_prefabs[talker_kind];
    /* portraits sit inside the bgMask: they open and close with the band */
    int clipped = mask < 0.999f && D->bg != SPR_NONE;
    if (clipped) {
        float cy = gfx_wy(4.0f);
        int band = (int)(gfx_sprite_h(D->bg) * mask + 0.5f);
        int top = (int)(cy - band * 0.5f);
        /* above the band the portrait is only cut once the band starts closing */
        gfx_clip(0, mask_goal > 0 ? 0 : top - 140 * mask, SCR_W, band + (mask_goal > 0 ? top : 140 * mask));
    }
    float rx = pf->x / 256.0f - (pf->slide ? 2.0f - talker_dx : 0), ry = pf->y / 256.0f;
    for (int i = 0; i < pf->n && i < MAX_PARTS; i++) {
        const PrefabPart *pp = &g_prefab_parts[pf->first + i];
        int spr = pp->spr;
        float a = pp->a / 255.0f, sx = pp->sx / 256.0f, sy = pp->sy / 256.0f;
        AnimOut o;
        int animated = 0;
        float px = rx + pp->x / 256.0f, py = ry + pp->y / 256.0f, rot = 0;
        if (pp->ctrl < 0 && pp->owner >= 0 && pp->owner < MAX_PARTS && g_prefab_parts[pf->first + pp->owner].ctrl >= 0 && pp->ppath) {
            /* an animated parent (e.g. the cutscene's "background" group) carries this part with it */
            AnimOut po;
            anim_eval_path(&part_anim[pp->owner], pp->ppath, &po);
            if (po.has_posx)
                px += (po.dx - pp->plx) * pp->pgsx;
            if (po.has_posy)
                py += (po.dy - pp->ply) * pp->pgsy;
        }
        if (pp->ctrl < 0 && pp->owner >= 0 && pp->owner < MAX_PARTS && g_prefab_parts[pf->first + pp->owner].ctrl >= 0) {
            anim_eval_path(&part_anim[pp->owner], pp->path, &o);
            if (!o.active)
                continue;
            if (o.spr != SPR_NONE)
                spr = o.spr;
            if (o.has_alpha)
                a = o.a;
            if (o.has_posx)
                px += pp->px + o.dx * pp->psx - pp->x / 256.0f;
            if (o.has_posy)
                py += pp->py + o.dy * pp->psy - pp->y / 256.0f;
            if (o.has_sx)
                sx = o.sx * pp->psx;
            if (o.has_sy)
                sy = o.sy * pp->psy;
            if (o.has_rot)
                rot = o.rot;
        }
        if (pp->ctrl >= 0) {
            anim_eval(&part_anim[i], &o);
            if (o.spr != SPR_NONE && !(pp->root && talker_spr != SPR_NONE && part_anim[i].finished)) {
                spr = o.spr;
                animated = 1;
            }
            if (o.has_alpha)
                a = o.a;
            if (o.has_scale) {
                sx = o.sx * (sx < 0 ? -1 : 1);
                sy = o.sy;
            }
        }
        if (pp->root) {
            /* GoalSprite assigns newSprite to the new talker's renderer even when it is null:
             * without its own sprite animation the root then shows the element's sprite or nothing */
            if (!animated)
                spr = talker_spr;
            if (pf->slide)
                a = talker_a;
        }
        if (spr == SPR_NONE || a <= 0.003f)
            continue;
        if (rot != 0)
            gfx_sprite_rot(spr, gfx_wx(px), gfx_wy(py), sx, sy, rot, RGBA(pp->r, pp->g, pp->b, (int)(a * 255)));
        else
            gfx_sprite(spr, gfx_wx(px), gfx_wy(py), sx, sy, RGBA(pp->r, pp->g, pp->b, (int)(a * 255)));
    }
    if (clipped)
        gfx_clip_reset();
}

static void draw_button(int i, float y)
{
    float k = button_anim[i];
    float slide = 0;
    if (buttons_t < 0.25f)
        slide = (1.0f - buttons_t / 0.25f) * 72.0f; /* button_start: -280 -> 10 canvas */
    float sc = 0.95f + 0.05f * k;
    int fr = 101 + (int)((230 - 101) * k), fg = 61 + (int)((77 - 61) * k), fb = 72 + (int)((81 - 72) * k);
    float w = 420 * sc, h = 25 * sc;
    gfx_sprite_rect(k > 0.5f ? SPR_button0003 : SPR_button0004, 240 - w / 2, y + slide - h / 2, w, h, RGBA(fr, fg, fb, 255));
    int tr = 212 + (int)((255 - 212) * k), tg = 185 + (int)((255 - 185) * k), tb = 191 + (int)((255 - 191) * k);
    gfx_text_fit(FONT_SMALL, 240, y + slide - 8, button_text[i], RGBA(tr, tg, tb, 255), 404);
}

void dlg_draw_ui(void)
{
    if (!D || (!active && mask <= 0.001f))
        return;
    /* PSP layout: name / two text lines / booper, spaced for the larger PSP fonts */
    if (name_text)
        gfx_text_fit(FONT_NAME, 240, 177, name_text, NAME_RGB, 470);
    if (text1) {
        float a = text_t < 0.15f ? text_t / 0.15f : 1.0f;
        uint32_t col = text_bad ? DEATH_RGB : WHITE;
        col = (col & 0x00FFFFFF) | ((uint32_t)(a * 255) << 24);
        int lh = 16;
        float y = text2 ? 196 : 204;
        gfx_text_fit(FONT_TEXT, 240, y, text1, col, 468);
        if (text2)
            gfx_text_fit(FONT_TEXT, 240, y + lh, text2, col, 468);
    }
    if (booper_on && !buttons_shown && !success_on) {
        int spr = booper_click_t >= 0 ? clip_sprite_at(CLIP_BOOPER_CLICK, booper_click_t, 0)
                                      : clip_sprite_at(CLIP_BOOPER, booper_t, 1);
        gfx_sprite(spr, UI_X(0), 244, 0.8f, 0.8f, WHITE);
    }
    if (success_on == 3) {
        /* successG: skeleDancer statues rise (oUI_start) then idle (oUI_idle), "GLORIOUS SUCCESS" plays */
        static int c_start = -2, c_idle, c_g;
        if (c_start == -2) {
            c_start = clip_find("sucessGAnim");
            c_idle = clip_find("sucessGIdle");
            c_g = c_start;
        }
        int st = success_t < 16 / 12.0f ? SPR_mainUIexport_oUI_start0001 + (int)(success_t * 12)
                                         : SPR_mainUIexport_oUI_idle0001 + ((int)((success_t - 16 / 12.0f) * 12) % 8);
        gfx_sprite(st, 72, 214, 1, 1, WHITE);
        gfx_sprite(st, 408, 214, -1, 1, WHITE);
        int spr = success_t < clip_length(c_g) ? clip_sprite_at(c_g, success_t, 0) : clip_sprite_at(c_idle, success_t, 1);
        if (spr == SPR_NONE)
            spr = clip_sprite_at(c_g, clip_length(c_g), 0);
        gfx_sprite(spr, UI_X(0), 248, 0.72f, 0.72f, WHITE);
    } else if (success_on) {
        gfx_sprite(clip_sprite_at(CLIP_SUCCESS, success_t, 0), UI_X(0), 252, 0.72f, 0.72f, WHITE);
    }
    if (buttons_shown && !widget_active()) {
        float y0 = nbuttons > 2 ? UI_Y(-320) : UI_Y(-400) - 2;
        for (int i = 0; i < nbuttons; i++)
            draw_button(i, y0 + i * 20);
    }
}
