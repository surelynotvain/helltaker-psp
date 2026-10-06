#ifndef GAME_H
#define GAME_H
#include <stdint.h>
#include "gamedata.h"
#include "anim.h"

#define DT (1.0f / 60.0f)

/* ---- input (edge + held), filled by main.c each frame */
enum { BTN_UP = 1, BTN_DOWN = 2, BTN_LEFT = 4, BTN_RIGHT = 8, BTN_OK = 16, BTN_BACK = 32,
       BTN_START = 64, BTN_RESTART = 128, BTN_ADVICE = 256 };
extern unsigned in_held, in_pressed;

/* ---- persistent settings / progress */
typedef struct {
    uint32_t magic;
    uint8_t music_vol, sfx_vol;
    uint8_t chapter_reached;
    uint8_t rituals;   /* Alfa/Beta/Gamma inscription pieces (chapters IV-VI) */
    uint8_t dlc_progress; /* PlayerPrefs dlcProgress: furthest Examtaker floor (pauseMenu.DLCChapter) */
    uint8_t lang_off;     /* 1: English although a LANG.PAK is installed (pause menu > LANGUAGE) */
} Save;
extern Save g_save;
void save_write(void);

/* ---- timers (Unity coroutine stand-ins). Scaled by g_time_scale unless realtime. */
typedef void (*TimerFn)(int a, int b);
void timer_after(float sec, TimerFn fn, int a, int b);
void timer_clear(void);
extern float g_time_scale;

/* ---- vfx: one-shot clip players in world space */
void vfx_spawn(int clip, float wx, float wy, int layer, float sx);
void vfx_spawn_any(const int *clips, int n, float wx, float wy, int layer, int jitter);

/* ---- camera shake (CameraShake.Shakedown) */
void cam_shake(float dur, float amount);

/* ---- scene flow */
extern int g_scene;
extern const SceneDef *S;
void scene_request(int index);      /* load at end of frame */
void lang_choose(void);             /* main.c: ENGLISH / translation picker (LANG.PAK only) */
void scene_restart_with_door(int target); /* Player.Advance after door closes; -2 reload, -1 next */
void door_close_then(int target);
int scene_kind(void);

/* ---- puzzle state (puzzle.c) */
typedef struct {
    int x, y;              /* grid cell */
    float px, py;          /* render position (world) */
    int moving;
    float shake;
    float bx, by;          /* shake base */
    int alive;
    int flip;
    Animator anim;
    uint16_t spr;
} Ent;

typedef struct {
    Ent player;
    int facing_left;
    int will;
    int frozen, in_menu, un_menuable, player_turn, restartable, cheater;
    int key_master, key_alive, lock_alive;
    float will_scale, will_red;     /* UI pop + red flash */
    float player_red;
    Ent boxes[24];
    int nboxes;
    Ent skel[16];
    int nskel;
    struct { int x, y, up, stat; Animator anim; } spikes[32];
    int nspikes;
    Animator key_anim;
    int dead;
    int hero_hidden;
} Puzzle;
extern Puzzle P;

void puzzle_load(void);
void puzzle_update(void);
void puzzle_draw_layer(int layer);
void puzzle_restart(void);
void puzzle_advice(void);
void puzzle_skip(void);
void player_victory(void);
void player_dialogue_restart(int target);
int puzzle_will_text(char *buf);
void puzzle_dialogue_closed(int epilogue);
void puzzle_teleport(int x, int y);
int ritual_complete(void);

/* ---- dialogue (dialogue.c) */
void dlg_reset(void);
void dlg_start(int index);
int dlg_active(void);
void dlg_update(void);
void dlg_draw_blackout(void);
void dlg_draw_world(void);   /* background + portraits (deathLayer) */
void dlg_draw_ui(void);      /* text, buttons, booper */
void dlg_back_from_widget(int part);
int dlg_ui_hidden(void);     /* perishableUI cleared */
float dlg_ui_slide(void);

/* ---- ui (ui.c) */
void ui_reset(void);
void ui_update(void);
void ui_draw(void);
void ui_draw_pause(void);
int ui_pause_open(void);
void widget_open(void);
void widget2_open(int dlc);
int widget_active(void);
void door_reset(int visible);
void door_update(void);
void door_draw(void);
void door_trigger_close(void);
int door_closed(void);

/* ---- Judgement boss (boss.c) */
void boss_load(void);
void boss_update(float dt);
void boss_draw_layer(int layer);
void boss_draw_ui(void);
int boss_try_kick(int tx, int ty);
int boss_active(void);
float boss_cam_dy(void);
int boss_giga_offset(int g, float *dx, float *dy);
float boss_giga_flash(int g);
void boss_god_mode(void);
int boss_heart_alive(int idx);
const Animator *boss_giga_anim(int g);
int boss_landing_state(void);
void decor_restart_kind(int kind);

/* ---- Examtaker lab (lab.c) */
void lab_load(void);
void lab_update(float dt);
void lab_draw_layer(int layer);
void lab_draw_hud(void);
int lab_active(void);
int lab_pillar_at(int x, int y);
void lab_kick_pillar(int i, int dx, int dy);
void lab_skip(void);
void player_laser_death(void);
void puzzle_start_goal(void);
void puzzle_bones(float x, float y);
int puzzle_cell_blocked(int x, int y);

/* ---- decor & demons (scene.c) */
Animator *decor_anim(int idx);
typedef void (*DecorEventFn)(int decor, int fn, int arg);
void decor_set_events(int idx, DecorEventFn fn);
void decor_set_hidden(int idx, int hidden);

/* ---- hellm07 AMD boss (amd.c) */
extern int g_amd_phase;
void amd_load(void);
void amd_update(float dt);
void amd_draw_layer(int layer);
void amd_draw_ui(void);
void amd_core_hit(void);
void amd_skip(void);
int amd_active(void);
void lab_laser_activate(int i);
void lab_laser_shot(int i);
void lab_lasers_off(void);
void lab_pillar_revive(int i);
void lab_pillar_alive_set(int i, int alive);
int lab_pillar_alive(int i);
void lab_player_hit(void);
void decor_demons_get(void);
void decor_hide_love_signs(uint32_t demons);   /* bit = DecorDef.sign */

/* clip ids resolved at startup */
extern int CLIP_DUST[3], CLIP_HIT[2], CLIP_HIT_SMALL[2], CLIP_BLOOD[3], CLIP_KEYVFX, CLIP_DEATH, CLIP_LOVEPLOSION,
    CLIP_SUCCESS, CLIP_BOOPER, CLIP_BOOPER_CLICK, CLIP_BONE;

#endif
