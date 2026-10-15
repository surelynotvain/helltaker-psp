/* VCPE controller reading (see vcpe_pad.h). */
#include <pspctrl.h>
#include <pspkernel.h>
#include <stdio.h>
#include <string.h>
#include "vcpe_test.h"
#include "vcpe_input.h"
#include "vcpe_pad.h"

static const VcpePadBind *binds;
static int nbinds, stick_mode = VCPE_STICK_DPAD, debounce_us;
static VcpeInput edges;
static uint32_t psp_now, held_now, held_prev_update, stick_bits, repeat_frames[32];
static int sx, sy, sampling;
static SceInt64 last_press[32];

void vcpe_pad_init(const VcpePadBind *b, int n, int mode)
{
    binds = b;
    nbinds = n;
    stick_mode = mode;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
    sampling = 1;
    vcpe_input_reset(&edges, 0);
}

void vcpe_pad_debounce(int ms) { debounce_us = ms * 1000; }

static uint32_t map(uint32_t psp)
{
    if (!binds)
        return psp;
    uint32_t b = 0;
    for (int i = 0; i < nbinds; i++)
        if (psp & binds[i].psp)
            b |= binds[i].bits;
    return b;
}

uint32_t vcpe_pad_update(void)
{
    uint32_t psp = 0;
    if (!sampling)
        vcpe_pad_init(binds, nbinds, stick_mode);
    if (vcpe_autotest) {
        unsigned h = 0;
        vcpe_test_step(&h);
        psp = h;
        sx = sy = 0;
    } else {
        SceCtrlData pad;
        sceCtrlPeekBufferPositive(&pad, 1);
        psp = pad.Buttons;
        sx = pad.Lx - 128;
        sy = pad.Ly - 128;
        if (stick_mode == VCPE_STICK_DPAD) {
            stick_bits = 0;
            if (pad.Lx < 50) stick_bits |= PSP_CTRL_LEFT;
            if (pad.Lx > 205) stick_bits |= PSP_CTRL_RIGHT;
            if (pad.Ly < 50) stick_bits |= PSP_CTRL_UP;
            if (pad.Ly > 205) stick_bits |= PSP_CTRL_DOWN;
        } else if (stick_mode == VCPE_STICK_DPAD_HYST) {
            /* engages far out, lets go near the centre */
            if (sy < -96) stick_bits |= PSP_CTRL_UP; else if (sy > -40) stick_bits &= ~PSP_CTRL_UP;
            if (sy > 96) stick_bits |= PSP_CTRL_DOWN; else if (sy < 40) stick_bits &= ~PSP_CTRL_DOWN;
            if (sx < -96) stick_bits |= PSP_CTRL_LEFT; else if (sx > -40) stick_bits &= ~PSP_CTRL_LEFT;
            if (sx > 96) stick_bits |= PSP_CTRL_RIGHT; else if (sx < 40) stick_bits &= ~PSP_CTRL_RIGHT;
        } else {
            stick_bits = 0;
        }
        psp |= stick_bits;
    }
    psp_now = psp;
    uint32_t held = map(psp);
    uint32_t newly = held & ~edges.previous;
    if (debounce_us && newly) {
        SceInt64 now = sceKernelGetSystemTimeWide();
        for (int i = 0; i < 32; i++)
            if (newly & (1u << i)) {
                if (now - last_press[i] < debounce_us)
                    edges.previous |= 1u << i;   /* too soon: not a new press */
                else
                    last_press[i] = now;
            }
    }
    vcpe_input_sample(&edges, held);
    held_prev_update = held_now;
    held_now = held;
    for (int i = 0; i < 32; i++)
        repeat_frames[i] = (held & (1u << i)) ? repeat_frames[i] + 1 : 0;
    return held;
}

uint32_t vcpe_pad_held(void) { return held_now; }
uint32_t vcpe_pad_pressed(void) { return vcpe_input_pressed(&edges); }
uint32_t vcpe_pad_released(void) { return held_prev_update & ~held_now; }
uint32_t vcpe_pad_psp(void) { return psp_now; }
void vcpe_pad_stick(int *x, int *y) { if (x) *x = sx; if (y) *y = sy; }

uint32_t vcpe_pad_repeat(uint32_t bits, int delay, int rate)
{
    uint32_t r = 0;
    for (int i = 0; i < 32; i++) {
        uint32_t b = 1u << i;
        if (!(bits & b) || !repeat_frames[i])
            continue;
        uint32_t f = repeat_frames[i];
        if (f == 1 || (f > (uint32_t)delay && rate > 0 && (f - delay - 1) % rate == 0))
            r |= b;
    }
    return r;
}

unsigned vcpe_pad_button(const char *name)
{
    static const struct { const char *n; unsigned b; } m[] = {
        {"up", PSP_CTRL_UP}, {"down", PSP_CTRL_DOWN}, {"left", PSP_CTRL_LEFT}, {"right", PSP_CTRL_RIGHT},
        {"cross", PSP_CTRL_CROSS}, {"x", PSP_CTRL_CROSS}, {"ok", PSP_CTRL_CROSS},
        {"circle", PSP_CTRL_CIRCLE}, {"o", PSP_CTRL_CIRCLE}, {"back", PSP_CTRL_CIRCLE},
        {"square", PSP_CTRL_SQUARE}, {"sq", PSP_CTRL_SQUARE}, {"triangle", PSP_CTRL_TRIANGLE}, {"tr", PSP_CTRL_TRIANGLE},
        {"l", PSP_CTRL_LTRIGGER}, {"r", PSP_CTRL_RTRIGGER}, {"start", PSP_CTRL_START}, {"select", PSP_CTRL_SELECT}};
    unsigned b = 0;
    char buf[64];
    snprintf(buf, sizeof buf, "%s", name);
    for (char *t = strtok(buf, "+"); t; t = strtok(0, "+"))
        for (unsigned i = 0; i < sizeof m / sizeof m[0]; i++)
            if (!strcmp(t, m[i].n))
                b |= m[i].b;
    return b;
}
