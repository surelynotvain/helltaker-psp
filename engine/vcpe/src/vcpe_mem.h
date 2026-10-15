/* VCPE memory for the 32 MB PSP-1000: tell the models apart and use the 4 MB "volatile" partition.
 *
 * A game built with PSP_LARGE_MEMORY gets about 50 MB of heap on a PSP-2000/3000/Street/Go but only about 20 MB on a
 * PSP-1000. Every model also has a 4 MB partition at 0x08400000 that the system lends to the running game
 * (sceKernelVolatileMemLock; commercial games use it too). Whether its contents survive sleep is not verified, so keep
 * only data there that can be read again from the PAK: texture bundles placed with vcpe_bundle_load_at(..., 1) are
 * read again after every resume.
 *
 * Fragmentation is the other PSP-1000 trap: after freeing an 8 MB scene bundle, a 9 MB one may not fit anywhere.
 * vcpe_bundle_slab() (vcpe_gfx.h) gives a range of bundles one fixed block that is reused, so it can't happen. */
#ifndef VCPE_MEM_H
#define VCPE_MEM_H
#include <stdint.h>

/* 1 on a 32 MB console (PSP-1000). Measured once with vcpe_heap_kb(), so call it early, before big allocations */
int vcpe_mem_32mb(void);
/* the 4 MB volatile partition (cached address, 64-byte aligned), locked for the game on the first call and kept.
 * 0 when the system refuses it (another program holds it). *size gets its size in bytes */
uint8_t *vcpe_volatile_mem(uint32_t *size);
/* heap in use and the most it ever was, in KB (mallinfo) */
int vcpe_mem_used_kb(void);
int vcpe_mem_peak_kb(void);

#endif
