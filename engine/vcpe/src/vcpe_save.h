/* VCPE saves: small save files next to the EBOOT that survive a crash or a pulled battery mid-write.
 * Each save goes to whichever of two slots (<name> and <name>.BAK) does not hold the newest good copy; a header
 * carries a version, the size, a sequence number and a CRC-32, and reading picks the newest slot that checks out.
 * So a broken write only ever loses that one save, and an old file of another version or size is never loaded
 * as garbage (it reads as "no save": start with defaults). */
#ifndef VCPE_SAVE_H
#define VCPE_SAVE_H
#include <stdint.h>

/* 0 loaded; -1 no save yet; -2 only broken / other-version / other-size copies (data untouched in both cases) */
int vcpe_save_read(const char *name, void *data, uint32_t size, uint32_t version);
/* 0 written */
int vcpe_save_write(const char *name, const void *data, uint32_t size, uint32_t version);
void vcpe_save_erase(const char *name);
uint32_t vcpe_crc32(const void *data, uint32_t n, uint32_t crc);   /* crc 0 to start */

#endif
