/* HUD statues, pause menu (pauseMenu.cs), chapter select (widgetScript.cs), door transition. */
#include <string.h>
#include <stdio.h>
#include "game.h"
#include "gfx.h"
#include "audio.h"

#define UI_X(cx) (240.0f + (cx) * 0.25f)
#define UI_Y(cy) (136.0f - (cy) * 0.25f)
#define GREY_TEXT RGBA(212, 185, 191, 255)
#define GREY_BRACKET RGBA(101, 61, 72, 255)
#define RED_BRACKET RGBA(230, 77, 81, 255)

extern int g_menu_returning;

/* ------------------------------------------------------------ door */
static Animator door;
static int door_visible = 1;
static int dlc_door;   /* Manager.dlcTransition: Examtaker's door (transtionHM) from hellm00 until the main menu */
#define PATH_HMDOOR 2439613268u
static float door_showup;

static void door_event(void *u, int fn, int arg)
{
    (void)u; (void)arg;
    if (fn == EVT_Opening) {
        if (door_visible)
            sfx_play(dlc_door ? SFX_screen_changer_2_part2 : SFX_screen_changer_part2_01, 0);
    } else if (fn == EVT_Closing) {
        sfx_mute_new(0); /* Manager.doorsClosing lets this through deadSound */
        sfx_play(dlc_door ? SFX_screen_changer_2_part1 : SFX_screen_changer_part1_01, 0);
    } else if (fn == EVT_DoorSwap) {
        if (g_scene == 20)
            dlc_door = 1;      /* transitionScript.dlcFirstLevel (hellm00) */
        else if (S->kind == SK_MENU)
            dlc_door = 0;
    }
}

void door_reset(int visible)
{
    anim_init(&door, CTRL_Image);
    door_visible = visible;
    door_showup = visible ? -1 : 2.0f;
}

void door_trigger_close(void)
{
    door_visible = 1;
    anim_trigger(&door, P_gameOver);
}

int door_closed(void)
{
    int clip = anim_clip(&door);
    return clip >= 0 && !strcmp(g_clips[clip].name, "transition_inProgress");
}

void door_update(void)
{
    if (door_showup > 0) {
        door_showup -= DT;
        if (door_showup <= 0)
            door_visible = 1;
    }
    anim_update(&door, DT, door_event, 0);
}

void door_draw(void)
{
    if (!door_visible)
        return;
    int clip = anim_clip(&door);
    if (clip < 0 || !strcmp(g_clips[clip].name, "transition_null"))
        return;
    AnimOut o;
    if (dlc_door) {
        anim_eval_path(&door, PATH_HMDOOR, &o);
        if (o.has_enabled && !o.enabled)
            return;
        if (o.spr == SPR_NONE) {
            if (!strcmp(g_clips[clip].name, "transition_inProgress"))
                gfx_rect(0, 0, SCR_W, SCR_H, RGBA(2, 2, 28, 255));
            return;
        }
        float w = (o.has_rw ? o.rw : 1924) * 0.25f, h = (o.has_rh ? o.rh : 1084) * 0.25f;
        float cy = 136 - (o.has_ry ? o.ry : 0) * 0.25f;
        gfx_sprite_rect(o.spr, 240 - w * 0.5f, cy - h * 0.5f, w, h, WHITE);
        return;
    }
    anim_eval(&door, &o);
    if (o.spr == SPR_NONE) {
        if (!strcmp(g_clips[clip].name, "transition_inProgress"))
            gfx_rect(0, 0, SCR_W, SCR_H, RGBA(2, 2, 28, 255));
        return;
    }
    gfx_sprite(o.spr, 240, 136, 1, 1, WHITE);
}

/* ------------------------------------------------------------ pause menu */
static int pause_open, cur_idx = 3, max_idx = 4;
static int vol[2];
static float bracket_k[5];


int ui_pause_open(void) { return pause_open; }

/* PSP build: volume lives on the console, so only RESUME / SKIP PUZZLE / MAIN MENU */
static const int order[3] = {3, 4, 2};   /* top to bottom on screen */
static const float row_y[3] = {95, 132, 169};   /* PSP pixels */

static void pause_toggle(void)
{
    if (!pause_open) {
        max_idx = (P.frozen || g_scene == 8 || P.cheater || (S->kind != SK_PUZZLE && S->kind != SK_BOSS)) ? 3 : 4;
        cur_idx = 3;
        pause_open = 1;
        P.in_menu = 1;
        music_pause(0);
    } else {
        sfx_play(SFX_button_menu_confirm_01, 0);
        pause_open = 0;
        P.in_menu = 0;
    }
}

static void pause_update(void)
{
    if (!P.un_menuable && (in_pressed & (BTN_START | BTN_BACK)) && !widget_active()) {
        pause_toggle();
        return;
    }
    if (!pause_open)
        return;
    if ((in_pressed & BTN_OK) && cur_idx > 1) {
        int idx = cur_idx;
        sfx_play(SFX_button_menu_confirm_01, 0);
        pause_open = 0;
        if (idx == 2) {
            P.in_menu = 1;
            P.un_menuable = 1;
            P.frozen = 1;
            g_menu_returning = 1;
            player_dialogue_restart(0);
        } else {
            P.in_menu = 0;
            if (idx == 4) {
                if (S->kind == SK_BOSS)
                    boss_god_mode();
                else
                    puzzle_skip();
            }
        }
        return;
    }
    if (in_pressed & (BTN_DOWN | BTN_UP)) {
        int rows[3], n = 0, cur = 0;
        for (int r = 0; r < 3; r++)
            if (order[r] <= max_idx)
                rows[n++] = order[r];
        for (int r = 0; r < n; r++)
            if (rows[r] == cur_idx)
                cur = r;
        cur = (in_pressed & BTN_DOWN) ? (cur + 1) % n : (cur + n - 1) % n;
        cur_idx = rows[cur];
        sfx_play(SFX_button_menu_highlight_01, 0);
    }
}

static void pause_draw(void)
{
    gfx_rect(0, 0, SCR_W, SCR_H, RGBA(2, 2, 28, 230));
    /* ritual frame: two ritBorder halves + pentagram, as in pauseMenu */
    gfx_sprite(SPR_ritBorder, 240 - 55, 132, 1.12f, 1.12f, WHITE);
    gfx_sprite(SPR_ritBorder, 240 + 55, 132, -1.12f, 1.12f, WHITE);
    gfx_sprite(SPR_ritStar, 240, 228, -1.1f, 1.1f, WHITE);
    gfx_text_fit(FONT_TITLE, 240, 18, g_text_m[4], RGBA(230, 77, 82, 255), 300);
    for (int r = 0; r < 3; r++) {
        int idx = order[r];
        if (idx > max_idx)
            continue;
        float y = row_y[r];
        int sel = idx == cur_idx;
        bracket_k[idx] += ((sel ? 1.0f : 0.0f) - bracket_k[idx]) * 0.4f;
        const char *label = idx == 4 && g_scene == 27 ? g_text_hm_m[2] :
                            idx == 4 && S->kind != SK_PUZZLE ? g_text_hm_m[1] : g_text_m[5 + idx];
        /* half_button_R (left, as drawn) at -100 canvas units, half_button_L mirrored at +100: the bracket
         * ends sit outside the label, lines fading inward; ButtonFeedback pulls both 20 px in when selected */
        float off = 25.0f - 6.9f * bracket_k[idx];
        uint32_t bc = sel ? RED_BRACKET : GREY_BRACKET;
        gfx_sprite(SPR_button_small, 240 - off, y + 1, 1.0f, 1.0f, bc);
        gfx_sprite(SPR_button_small, 240 + off, y + 1, -1.0f, 1.0f, bc);
        gfx_text_fit(FONT_MENU, 240, y - 11, label, sel ? WHITE : GREY_TEXT, 220);
    }
    /* code01-03: the ritual pieces found in chapters IV-VI (PlayerPrefs Alfa/Beta/Gamma) */
    static const int done[3] = {SPR_ritCode0001, SPR_ritCode0002, SPR_ritCode0003};
    for (int i = 0; i < 3; i++)
        gfx_sprite_rect((g_save.rituals & (1 << i)) ? done[i] : SPR_ritCode0004, 240 + (i - 1) * 50 - 25, 196, 50, 12.5f, WHITE);
}

/* ------------------------------------------------------------ chapter select */
static int widget_on, widget_idx, widget_block;
static float widget_t, box_k[11];

static int w2_on, w2_dlc, w2_idx, w2_block;
static float w2_t;
int widget_active(void) { return widget_on || w2_on; }

/* widgetScript_ch2: chapter II's 1-10 rating and Examtaker's floor picker (I .. dlcProgress) */
void widget2_open(int dlc)
{
    w2_on = 1;
    w2_dlc = dlc;
    w2_idx = 0;
    w2_block = 1;
    w2_t = 0;
}

static void widget2_update(void)
{
    if (!w2_on)
        return;
    if (w2_block) {
        if (!(in_held & BTN_OK))
            w2_block = 0;
    } else if (in_pressed & BTN_OK) {
        sfx_play(SFX_button_dialogue_confirm_01, 0);
        w2_on = 0;
        if (w2_dlc) {
            if (w2_idx != 0)
                player_dialogue_restart(w2_idx + 21);
            else
                dlg_back_from_widget(3);
        } else {
            dlg_back_from_widget(w2_idx == 9 ? 6 : 5);
        }
        return;
    }
    int max = w2_dlc ? g_save.dlc_progress : 9;
    if (in_pressed & BTN_LEFT) {
        sfx_play(SFX_button_dialogue_highlight_01, 0);
        w2_idx = w2_idx > 0 ? w2_idx - 1 : max;
    } else if (in_pressed & BTN_RIGHT) {
        sfx_play(SFX_button_dialogue_highlight_01, 0);
        w2_idx = w2_idx < max ? w2_idx + 1 : 0;
    }
}

static void widget2_draw(void)
{
    static const char *roman[7] = {"I", "II", "III", "IV", "V", "VI", "VII"};
    w2_t += DT;
    float rise = w2_t < 0.25f ? (1.0f - w2_t / 0.25f) * 80.0f : 0.0f;
    float y = UI_Y(-420) + rise;
    gfx_sprite_rect(SPR_W_selection, UI_X(-150) - 10, y - 7.5f, 20, 15, WHITE);
    gfx_sprite_rect(SPR_W_selection, UI_X(150) + 10, y - 7.5f, -20, 15, WHITE);
    char buf[12];
    const char *t = buf;
    if (w2_dlc)
        t = roman[w2_idx < 7 ? w2_idx : 6];
    else
        snprintf(buf, sizeof buf, "%d", w2_idx + 1);
    float lh = gfx_font_line(FONT_SCORE);
    gfx_text(FONT_SCORE, UI_X(0), UI_Y(-410) + rise - lh * 0.5f, t, WHITE, 1, 1.0f);
}

void widget_open(void)
{
    widget_t = 0;
    for (int i = 0; i < 11; i++)
        box_k[i] = i == 0;
    widget_on = 1;
    widget_idx = 0;
    widget_block = 1;
}

/* chapter select entry -> first scene of that chapter (widgetScript) */
static int widget_scene(int idx)
{
    if (idx == 0) return 1;
    if (idx < 9) return idx + 1;
    if (idx == 9) return 15;
    return 20; /* EX: Examtaker (hellm00) */
}

static int widget_unlocked(int idx)
{
    return idx == 0 || idx == 10 || widget_scene(idx) <= g_save.chapter_reached;
}

static void widget_update(void)
{
    if (!widget_on)
        return;
    if (widget_block) {
        widget_block = 0;
        return;
    }
    if (in_pressed & BTN_BACK) {
        widget_on = 0;
        dlg_back_from_widget(1);
        return;
    }
    if ((in_pressed & BTN_OK) && !widget_unlocked(widget_idx)) {
        sfx_play(SFX_door_closed_kick_01, 0);   /* locked */
        return;
    }
    if (in_pressed & BTN_OK) {
        sfx_play(SFX_button_chapter_confirm_01, 0);
        if (widget_idx == 0) {
            widget_on = 0;
            dlg_back_from_widget(2);
        } else if (widget_idx < 9) {
            widget_on = 0;
            player_dialogue_restart(widget_idx + 1);
        } else if (widget_idx == 9) {
            widget_on = 0;
            player_dialogue_restart(widget_idx + 6);
        }
        else {
            widget_on = 0;
            player_dialogue_restart(widget_idx + 10);   /* Examtaker intro, hellm00 */
        }
        return;
    }
    if (in_pressed & BTN_LEFT) {
        sfx_play(SFX_button_chapter_highlight_01, 0);
        widget_idx = widget_idx > 0 ? widget_idx - 1 : 10;
    } else if (in_pressed & BTN_RIGHT) {
        sfx_play(SFX_button_chapter_highlight_01, 0);
        widget_idx = widget_idx < 10 ? widget_idx + 1 : 0;
    }
}


static void widget_draw(void)
{
    static const char *roman[11] = {"I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "EX"};
    /* widget_emerge: slides up from -320 canvas units over 0.25 s */
    widget_t += DT;
    float rise = widget_t < 0.25f ? (1.0f - widget_t / 0.25f) * 80.0f : 0.0f;
    /* rails: W_chapterSelect 1452x60 at y -270 and mirrored at -380 */
    gfx_sprite_rect(SPR_W_chapterSelect, UI_X(-726), UI_Y(-270) - 7.5f + rise, 363, 15, WHITE);
    gfx_sprite_rect(SPR_W_chapterSelect, UI_X(-726), UI_Y(-380) + 7.5f + rise, 363, -15, WHITE);
    for (int i = 0; i < 11; i++) {
        float cx = i < 10 ? -540 + 120 * i : 678;
        float x = UI_X(cx), y = UI_Y(-325) + rise;
        int sel = i == widget_idx;
        /* chapter_active / chapter_inActive: 0.1667 s fades */
        float target = sel ? 1.0f : 0.0f, step = DT / 0.1667f;
        if (box_k[i] < target) box_k[i] = box_k[i] + step > target ? target : box_k[i] + step;
        if (box_k[i] > target) box_k[i] = box_k[i] - step < target ? target : box_k[i] - step;
        float k = box_k[i];
        int locked = !widget_unlocked(i);
        int box_a = (int)((0.4f + 0.6f * k) * 255 * (locked ? 0.45f : 1.0f)), in_a = (int)(0.25f * k * 255);
        uint32_t tint = i == 10 ? RGBA(230, 77, 82, box_a) : RGBA(255, 255, 255, box_a);
        uint32_t fill = i == 10 ? RGBA(230, 77, 82, in_a) : RGBA(255, 255, 255, in_a);
        gfx_sprite_rect(SPR_W_chapter1, x - 14, y - 10, 28, 20, tint);
        gfx_sprite_rect(SPR_W_chapter2, x - 8.5f, y - 6, 17, 12, fill);
        uint32_t tc = i == 10 ? RGBA(230, 77, 82, box_a) : RGBA(255, 255, 255, box_a);
        gfx_text_fit(FONT_SMALL, x + 0.25f, y - 7.5f, roman[i], tc, 26);
    }
    const char *title = widget_idx < 10 ? g_text_m2[widget_idx] : g_text_hm_m[0];
    uint32_t col = widget_idx < 10 ? WHITE : RGBA(230, 77, 77, 255);
    if (!widget_unlocked(widget_idx))
        col = (col & 0x00FFFFFF) | 0x60000000;
    gfx_text_fit(FONT_TEXT, 240, UI_Y(-425) - 8 + rise, title, col, 460);
}

/* ------------------------------------------------------------ HUD */
void ui_reset(void)
{
    pause_open = 0;
    widget_on = 0;
    w2_on = 0;
    vol[0] = g_save.music_vol;
    vol[1] = g_save.sfx_vol;
    static int menu_seen;   /* Manager.first */
    if (S->kind == SK_MENU) {
        if (menu_seen)
            g_menu_returning = 1;
        menu_seen = 1;
    }
    door_reset(!(S->kind == SK_MENU && !g_menu_returning));
}

void ui_update(void)
{
    widget_update();
    widget2_update();
    if (S->kind == SK_PUZZLE || S->kind == SK_BOSS || pause_open)
        pause_update();
}

static const char *chapter_label(void)
{
    for (int i = 0; i < S->ndlg; i++)
        if (S->dlg[i].chapter && S->dlg[i].chapter[0] && strcmp(S->dlg[i].chapter, "x"))
            return S->dlg[i].chapter;
    return "";
}

static void hud_draw(void)
{
    if (S->lab) {
        /* Examtaker: step counter on the monitor; only RESTART, centred (advice hidden; none on hellm07) */
        amd_draw_ui();
        if (g_scene != 27 && !P.dead && !P.frozen && !P.in_menu && !dlg_active())
            gfx_text_fit(FONT_SMALL, UI_X(0), UI_Y(-490) - 6, g_text_m[2], WHITE, 112);
        return;
    }
    if (S->kind != SK_PUZZLE || P.dead)
        return;
    float slide = dlg_ui_slide() * 190.0f;
    gfx_sprite(SPR_mainUIexport_fUI0001, UI_X(-716), UI_Y(-195) + slide, 1, 1, WHITE);
    gfx_sprite(SPR_mainUIexport_fUI0001, UI_X(716), UI_Y(-195) + slide, -1, 1, WHITE);
    char buf[8];
    puzzle_will_text(buf);
    float sc = 1.0f + P.will_scale;
    int gb = 255 - (int)(P.will_red * 204);
    float lh = gfx_font_line(FONT_BIG);
    gfx_text(FONT_BIG, UI_X(-755), UI_Y(-305) - lh * 0.5f * sc + slide, buf, RGBA(255, gb, gb, 255), 1, sc);
    gfx_text(FONT_BIG, UI_X(755), UI_Y(-305) - lh * 0.5f + slide, chapter_label(), WHITE, 1, 1.0f);
    if (!P.frozen && !P.in_menu && !dlg_active()) {
        char b2[96];
        snprintf(b2, sizeof b2, "%s", g_text_m[2]);
        gfx_text_fit(FONT_SMALL, UI_X(220), UI_Y(-490) - 6, g_text_m[2], WHITE, 112);
        gfx_text_fit(FONT_SMALL, UI_X(-220), UI_Y(-490) - 6, g_text_m[3], WHITE, 112);
    }
}

static void credits_draw(void)
{
    /* Credits.cs: three blocks of menuTxt lines over the cutscene_clean darkness */
    static const int lines[3][3] = {{23, 24, -1}, {25, 26, -1}, {27, 28, 29}};
    /* credits3 (Examtaker): the same blocks from dlcTxt 3..9 */
    static const int dlc_lines[3][3] = {{3, 4, -1}, {5, 6, -1}, {7, 8, 9}};
    int dlc = g_scene == 29;
    /* original layout: 1080p lines at y 170/230, 310/370, 450/510/570 -> PSP */
    static const float ys[3][3] = {{34, 50, 0}, {69, 85, 0}, {104, 120, 136}};
    for (int b = 0; b < 3; b++)
        for (int k = 0; k < 3 && lines[b][k] >= 0; k++)
            gfx_text_fit(FONT_NAME, 240, ys[b][k] - 8, dlc ? g_text_hm_m[dlc_lines[b][k]] : g_text_m[lines[b][k]],
                         k == 0 ? RGBA(230, 77, 82, 255) : WHITE, 460);
    /* the port's own credit at the bottom */
    gfx_text_fit(FONT_NAME, 240, 252, "PSP port by SurelyNotVain", RGBA(230, 77, 82, 255), 460);
}

void ui_draw(void)
{
    if (S->kind == SK_CREDITS)
        credits_draw();
    hud_draw();
    if (widget_on)
        widget_draw();
    if (w2_on)
        widget2_draw();
}

/* the pause menu covers everything except the door transition (dialogue and boss UI included) */
void ui_draw_pause(void)
{
    if (pause_open)
        pause_draw();
}
