/* VCPE pad: the PSP controller turned into the game's own button bits, the way every VCPE game read it by hand:
 * a bind table (PSP buttons -> game bits), the analog stick as the d-pad (plain threshold or with hysteresis, so a
 * drifting stick does not hop a menu selection), autotest scripts instead of the pad when AUTOTEST.TXT is there,
 * press edges that survive frames without an update (vcpe_input), optional anti-mash debounce, menu auto-repeat.
 *
 *   static const VcpePadBind binds[] = {{PSP_CTRL_CROSS, B_JUMP | B_OK}, {PSP_CTRL_CIRCLE, B_BACK}, ...};
 *   vcpe_pad_init(binds, n, VCPE_STICK_DPAD);
 *   VcpeTestHooks hooks = { vcpe_pad_button, my_commands, ... };   // script names map like the real pad
 *   loop: vcpe_pad_update();  then per update step: vcpe_pad_held(), vcpe_pad_pressed()
 *
 * Directions: the d-pad (and stick) are reported as PSP_CTRL_UP/DOWN/LEFT/RIGHT before binding, so bind those. */
#ifndef VCPE_PAD_H
#define VCPE_PAD_H
#include <stdint.h>

typedef struct { uint32_t psp, bits; } VcpePadBind;   /* any of the psp buttons held -> bits held */
enum { VCPE_STICK_NONE, VCPE_STICK_DPAD, VCPE_STICK_DPAD_HYST };

void vcpe_pad_init(const VcpePadBind *binds, int n, int stick_mode);
uint32_t vcpe_pad_update(void);      /* once per shown frame: read, map, latch edges; returns the held bits */
uint32_t vcpe_pad_held(void);
uint32_t vcpe_pad_pressed(void);     /* new presses since the last call (call it in the first update step) */
uint32_t vcpe_pad_released(void);    /* bits held last update and not now */
uint32_t vcpe_pad_psp(void);         /* the PSP buttons (with the stick as d-pad) behind the last update */
void vcpe_pad_stick(int *x, int *y); /* last stick position, -128..127 (0, 0 in autotest) */
void vcpe_pad_debounce(int ms);      /* presses of a button closer than ms apart count once (0: off) */
/* bits that fire this frame for menus: on the press, then after delay frames every rate frames while held */
uint32_t vcpe_pad_repeat(uint32_t bits, int delay, int rate);
/* autotest button names -> PSP buttons: up down left right cross circle square triangle l r start select,
 * aliases x o sq tr ok back, and "a+b" for several at once. Use it as VcpeTestHooks.button. */
unsigned vcpe_pad_button(const char *name);

#endif
