/* VCPE mouse: a PC mouse simulated on the PSP, for ports of games played with one (drag and drop inventories,
 * point-and-click, card and board games, strategy, menus made for a pointer). The game reads a pointer position
 * and mouse buttons the way the original read the real mouse; this module turns the PSP controls into them.
 *
 *   analog stick   moves the pointer, with a deadzone, a response curve and a boost when pushed to the rim
 *   d-pad          warps the pointer to the nearest point of interest in that direction (held: repeats), the
 *                  way console versions of mouse games jump between buttons and slots
 *   buttons        left / right / middle mouse buttons, a "slow" button for precise aiming, wheel up / down
 *
 * The defaults copy a shipped controller cursor (600 px/s on a 1920-wide screen = 150 px/s on the PSP's 480,
 * x1.5 at the rim, deadzone 0.2, warps within 50 degrees scored by distance and angle, first repeat after 20 frames,
 * then every 10).
 *
 * Use (needs vcpe_pad; init the pad with VCPE_STICK_NONE so the stick is the pointer, not the d-pad):
 *   vcpe_mouse_init(NULL);                      // or a VcpeMouseConf with your own buttons and speeds
 *   vcpe_mouse_poi_source(add_my_targets);      // optional: called when a warp needs the points of interest
 *   loop: vcpe_pad_update(); vcpe_mouse_update();
 *         per update step: vcpe_mouse_pressed() / released() / clicked() / drag_started() / dropped() ...
 *         draw: your own cursor at vcpe_mouse_x/y, or vcpe_mouse_draw(VCPE_CURSOR_ARROW, 1)
 *   test hooks: command -> vcpe_mouse_command first, busy -> vcpe_mouse_busy (see "Autotest" below)
 *
 * Press, release, click and drop edges are latched until read, like vcpe_pad_pressed: a tap is never lost when a
 * shown frame has no update step, and each edge is consumed by the first read. Positions are screen pixels
 * (floats; the pointer moves in sub-pixel steps). */
#ifndef VCPE_MOUSE_H
#define VCPE_MOUSE_H
#include <stdint.h>

enum { VCPE_MB_LEFT = 1, VCPE_MB_RIGHT = 2, VCPE_MB_MIDDLE = 4 };

typedef struct {
    uint32_t left, right, middle;    /* PSP buttons (PSP_CTRL_*) acting as each mouse button; any of them */
    uint32_t slow;                   /* held: the pointer moves at slow_factor speed */
    uint32_t wheel_up, wheel_down;   /* each press is a wheel step; held, they repeat */
    float speed;                     /* px per second at full tilt (before the rim boost) */
    float rim_boost;                 /* speed factor when the stick is at the rim (> 0.99 of the way out) */
    float slow_factor;
    float curve;                     /* response exponent: 1 linear, 2 finer near the centre */
    float deadzone;                  /* 0..1 of the stick's travel */
    int dpad_warp;                   /* 1: the d-pad warps between points of interest; 0: the game uses the d-pad */
    float warp_angle;                /* degrees either side of the pressed direction a target may lie */
    float warp_min_dist;             /* targets closer than this (px) are where the pointer already is */
    int warp_first, warp_rapid;      /* frames before the first repeat and between later repeats while held */
    int drag_px;                     /* left held and moved more than this: a drag (not a click) */
    int double_frames;               /* two clicks closer than this (and within drag_px): a double click */
    int hide_idle_frames;            /* > 0: vcpe_mouse_visible() turns 0 after this many frames without use */
} VcpeMouseConf;

/* the defaults: cross left, circle right, triangle middle, R slow, L / R wheel off, d-pad warps */
void vcpe_mouse_defaults(VcpeMouseConf *c);
void vcpe_mouse_init(const VcpeMouseConf *c);       /* NULL: defaults; the pointer starts at the screen centre */
void vcpe_mouse_bounds(float x, float y, float w, float h);   /* where the pointer may go (default: the screen) */
void vcpe_mouse_update(void);                       /* once per shown frame, after vcpe_pad_update */

extern float vcpe_mouse_x, vcpe_mouse_y;            /* the pointer (read-only; move it with vcpe_mouse_set) */
void vcpe_mouse_set(float x, float y);              /* warp the pointer (the game's own Input.warp_mouse) */
int vcpe_mouse_moved(void);                         /* the pointer moved during the last update */
int vcpe_mouse_idle(void);                          /* frames since the pointer last moved or a button changed */
int vcpe_mouse_visible(void);                       /* 0 after hide_idle_frames of no use (if set), else 1 */

uint32_t vcpe_mouse_held(void);                     /* VCPE_MB_* now */
uint32_t vcpe_mouse_pressed(void);                  /* went down since the last read */
uint32_t vcpe_mouse_released(void);                 /* went up since the last read */
int vcpe_mouse_wheel(void);                         /* wheel steps since the last read: + up, - down */

/* left button gestures */
int vcpe_mouse_clicked(void);          /* left went down and up without dragging: 1 (2 for a double click) */
int vcpe_mouse_drag_started(void);     /* left held and moved past drag_px since the last read */
int vcpe_mouse_dragging(void);         /* a drag is in progress */
int vcpe_mouse_dropped(void);          /* a drag ended (left released) since the last read */
void vcpe_mouse_press_pos(float *x, float *y);   /* where the left button went down (the drag origin) */
void vcpe_mouse_cancel_drag(void);     /* forget the current drag (the game refused it); no drop edge follows */

int vcpe_mouse_in(float x, float y, float w, float h);   /* the pointer is inside the rectangle */

/* points of interest for the d-pad warp. The source callback is called only when a warp is asked for, so a game
 * lists its slots and buttons once per warp, not every frame. The list may also be filled by the game directly
 * (vcpe_mouse_poi_clear + vcpe_mouse_poi each frame) when there is no source. */
void vcpe_mouse_poi_source(void (*collect)(void));
void vcpe_mouse_poi_clear(void);
void vcpe_mouse_poi(float x, float y);
void vcpe_mouse_poi_rect(float x, float y, float w, float h);   /* its centre */
int vcpe_mouse_warp(float dx, float dy);   /* warp now towards (dx, dy); 1 if a target was found */
int vcpe_mouse_warped(void);               /* the pointer warped since the last read (for a trail effect) */
void vcpe_mouse_warp_from(float *x, float *y);   /* where the last warp started */

/* built-in pointers drawn from shapes, hot spot at the pointer: for games without cursor art and for tools */
enum { VCPE_CURSOR_ARROW, VCPE_CURSOR_HAND, VCPE_CURSOR_GRAB, VCPE_CURSOR_CROSS };
void vcpe_mouse_draw(int shape, float scale);

/* Autotest. Scripts drive the pointer with these commands (the stick reads 0 in autotest):
 *   mouse X Y               put the pointer at X Y
 *   glide X Y N             move it there over N frames
 *   click X Y | rclick X Y  put it there, press and release the left / right button (2 frames)
 *   dclick X Y              double click
 *   mdown [left|right] / mup [left|right]   hold or release a button until mup
 *   drag X1 Y1 X2 Y2 [N]    press at 1, move to 2 over N frames (default 12), release
 *   warp up|down|left|right one d-pad warp
 *   wheel N                 N wheel steps (negative: down)
 *   mpos                    log the pointer (needs vcpe_log)
 * vcpe_mouse_command returns VCPE_CMD_UNKNOWN for anything else; vcpe_mouse_busy keeps multi-frame actions going. */
int vcpe_mouse_command(const char *cmd, const char *arg, int num, int num2, const char *line, unsigned *held);
int vcpe_mouse_busy(unsigned *held);

#endif
