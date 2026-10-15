/* Button edges sampled at render rate, consumed exactly once by fixed updates.
 * Pure C: no PSP dependency; one state per logical controller. */
#ifndef VCPE_INPUT_H
#define VCPE_INPUT_H
#include <stdint.h>

typedef struct {
    uint32_t held, pressed, previous;
} VcpeInput;

static inline void vcpe_input_reset(VcpeInput *state, uint32_t held)
{
    state->held = state->previous = held;
    state->pressed = 0;
}

static inline void vcpe_input_sample(VcpeInput *state, uint32_t held)
{
    state->pressed |= held & ~state->previous;
    state->previous = state->held = held;
}

/* A short tap survives display frames with no simulation update. */
static inline uint32_t vcpe_input_pressed(VcpeInput *state)
{
    uint32_t pressed = state->pressed;
    state->pressed = 0;
    return pressed;
}

static inline uint32_t vcpe_input_union(const uint32_t *inputs, int count)
{
    uint32_t result = 0;
    if (count <= 0) return result;
    for (int i = 0; i < count; i++) result |= inputs[i];
    return result;
}

/* Strict majority per button. An even split leaves that button released. */
static inline uint32_t vcpe_input_majority(const uint32_t *inputs, int count)
{
    uint32_t result = 0;
    if (count <= 0) return result;
    for (uint32_t bit = 1; bit; bit <<= 1) {
        int votes = 0;
        for (int i = 0; i < count; i++) votes += (inputs[i] & bit) != 0;
        if (votes > count / 2) result |= bit;
    }
    return result;
}
#endif
