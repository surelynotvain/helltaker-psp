/* VCPE math: what game code needs every frame, at PSP speed. libm sinf/cosf/atan2f cost ~0.55 us a call on the
 * PSP's FPU and integer division is ~20x slower than a multiply, so per-object code uses these instead:
 *   sin/cos from a 4096-step table with linear interpolation (|error| < 2e-6), degrees or radians
 *   atan2 from a minimax polynomial (|error| < 1.2e-5 rad, Abramowitz & Stegun 4.4.47), no libm
 *   sqrt as the FPU's own sqrt.s instruction
 *   xorshift32 random numbers (repeatable from a seed: autotest runs) with range picks by multiply, not modulo
 * No PSP dependency except sqrt.s (plain sqrtf elsewhere), so host tests use the same code. */
#ifndef VCPE_MATH_H
#define VCPE_MATH_H
#include <stdint.h>

#define VCPE_PI 3.14159265358979f
#define VCPE_SIN_N 4096

extern float vcpe_sin_tab[VCPE_SIN_N + 1];
void vcpe_math_init(void);   /* fills the table; vcpe_sys_init calls it when the game links this module */

/* sine of a turn fraction: t = 1 is a full circle */
static inline float vcpe_sin_turn(float t)
{
    float x = t * VCPE_SIN_N;
    int i = (int)x;
    if (x < i) i--;
    float f = x - i;
    i &= VCPE_SIN_N - 1;
    return vcpe_sin_tab[i] + (vcpe_sin_tab[i + 1] - vcpe_sin_tab[i]) * f;
}
static inline float vcpe_sin_deg(float d) { return vcpe_sin_turn(d * (1.0f / 360.0f)); }
static inline float vcpe_cos_deg(float d) { return vcpe_sin_turn(d * (1.0f / 360.0f) + 0.25f); }
static inline float vcpe_sin(float r) { return vcpe_sin_turn(r * (0.5f / VCPE_PI)); }
static inline float vcpe_cos(float r) { return vcpe_sin_turn(r * (0.5f / VCPE_PI) + 0.25f); }

static inline float vcpe_absf(float x) { return x < 0 ? -x : x; }

/* atan2(y, x) in radians, -pi..pi */
static inline float vcpe_atan2(float y, float x)
{
    float ax = vcpe_absf(x), ay = vcpe_absf(y);
    if (ax == 0 && ay == 0)
        return 0;
    float a = ax > ay ? ay / ax : ax / ay, s = a * a;
    float r = a * (0.9998660f + s * (-0.3302995f + s * (0.1801410f + s * (-0.0851330f + s * 0.0208351f))));
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0) r = 3.14159274f - r;
    if (y < 0) r = -r;
    return r;
}
/* degrees 0..360, counter-clockwise with y down on screen (GameMaker's point_direction) */
static inline float vcpe_direction_deg(float x1, float y1, float x2, float y2)
{
    float d = vcpe_atan2(-(y2 - y1), x2 - x1) * (180.0f / VCPE_PI);
    return d < 0 ? d + 360 : d;
}

static inline float vcpe_sqrt(float x)
{
#if defined(__psp__)
    float r;
    __asm__("sqrt.s %0, %1" : "=f"(r) : "f"(x));
    return r;
#else
    return __builtin_sqrtf(x);
#endif
}
static inline float vcpe_dist(float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1, dy = y2 - y1;
    return vcpe_sqrt(dx * dx + dy * dy);
}

static inline float vcpe_clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline int vcpe_clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static inline float vcpe_lerp(float a, float b, float t) { return a + (b - a) * t; }
/* move v toward target by at most step */
static inline float vcpe_approach(float v, float target, float step)
{
    return v < target ? (v + step > target ? target : v + step) : (v - step < target ? target : v - step);
}
/* a - b wrapped to -180..180 degrees */
static inline float vcpe_angle_diff_deg(float a, float b)
{
    float d = a - b;
    d -= 360.0f * (float)(int)(d * (1.0f / 360.0f));
    if (d > 180) d -= 360;
    else if (d < -180) d += 360;
    return d;
}

/* ---------------------------------------------------------------- random numbers */
typedef struct { uint32_t s; } VcpeRng;
static inline void vcpe_rng_seed(VcpeRng *r, uint32_t seed) { r->s = seed ? seed : 0x9E3779B9u; }
static inline uint32_t vcpe_rng_u32(VcpeRng *r)
{
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return r->s = x;
}
static inline float vcpe_rng_float(VcpeRng *r) { return (vcpe_rng_u32(r) >> 8) * (1.0f / 16777216.0f); }   /* [0, 1) */
static inline int vcpe_rng_int(VcpeRng *r, int n)                                                           /* [0, n) */
{
    return n > 0 ? (int)(((uint64_t)vcpe_rng_u32(r) * (uint32_t)n) >> 32) : 0;
}
static inline float vcpe_rng_range(VcpeRng *r, float lo, float hi) { return lo + (hi - lo) * vcpe_rng_float(r); }

/* the engine's shared generator: seeded from the clock by vcpe_sys_init (when linked); autotest runs call
 * vcpe_seed with a fixed value so every run is the same */
extern VcpeRng vcpe_rng;
static inline void vcpe_seed(uint32_t seed) { vcpe_rng_seed(&vcpe_rng, seed); }
static inline float vcpe_frand(void) { return vcpe_rng_float(&vcpe_rng); }
static inline int vcpe_irand(int n) { return vcpe_rng_int(&vcpe_rng, n); }

#endif
