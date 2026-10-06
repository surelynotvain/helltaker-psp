/* VCPE data PAK: one big read-only file, read by offset; survives sleep/resume and slow memory sticks. */
#ifndef VCPE_PAK_H
#define VCPE_PAK_H
#include <stdint.h>

int vcpe_pak_open(const char *path);                         /* <0 on failure */
int vcpe_pak_read(uint32_t off, void *dst, uint32_t size);   /* 0 ok, -1 failed */
void *vcpe_pak_read_alloc(uint32_t off, uint32_t size);      /* 64-byte aligned, cache written back; 0 failed */
void vcpe_pak_close(void);

#endif
