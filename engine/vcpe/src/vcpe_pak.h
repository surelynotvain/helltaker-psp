/* VCPE data PAK: one big read-only file, read by offset; survives sleep/resume and slow memory sticks. */
#ifndef VCPE_PAK_H
#define VCPE_PAK_H
#include <stdint.h>

int vcpe_pak_open(const char *path);                         /* <0 on failure */
int vcpe_pak_read(uint32_t off, void *dst, uint32_t size);   /* 0 ok, -1 failed */
void *vcpe_pak_read_alloc(uint32_t off, uint32_t size);      /* 64-byte aligned, cache written back; 0 failed */
void vcpe_pak_close(void);
/* set by vcpe_load while a loading screen is up: reads go in 256 KB chunks and report each one */
extern void (*vcpe_pak_progress)(uint32_t bytes);
/* open the PAK next to the EBOOT and check its header {char magic[4]; u32 total size; u32 build stamp} against
 * what this EBOOT was built with (the converter's generated values; magic 0 / size 0 / stamp 0 skip a check).
 * Missing, cut short (incomplete copy) or from another build: vcpe_fatal with a message the player can act on. */
void vcpe_pak_open_checked(const char *file, const char *title, const char *magic, uint32_t size, uint32_t stamp);

#endif
