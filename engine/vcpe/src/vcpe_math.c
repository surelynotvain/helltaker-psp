/* VCPE math tables (see vcpe_math.h). */
#include <math.h>
#include "vcpe_math.h"

float vcpe_sin_tab[VCPE_SIN_N + 1];
VcpeRng vcpe_rng = {0x12345678u};

/* vcpe_sys_init: tables and a clock seed (weak link there, so games without this module still build) */
void vcpe_math_boot(uint32_t seed)
{
    vcpe_math_init();
    vcpe_seed(seed);
}

void vcpe_math_init(void)
{
    if (vcpe_sin_tab[VCPE_SIN_N / 4] != 0)
        return;
    for (int i = 0; i <= VCPE_SIN_N; i++)
        vcpe_sin_tab[i] = sinf(i * (2 * 3.14159265358979f / VCPE_SIN_N));
    vcpe_sin_tab[0] = vcpe_sin_tab[VCPE_SIN_N / 2] = vcpe_sin_tab[VCPE_SIN_N] = 0;
    vcpe_sin_tab[VCPE_SIN_N / 4] = 1;
    vcpe_sin_tab[3 * VCPE_SIN_N / 4] = -1;
}
