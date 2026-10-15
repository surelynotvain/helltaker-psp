/* VCPE two-slot checked saves (see vcpe_save.h). Plain C file I/O, so host tests use it too. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vcpe_sys.h"
#include "vcpe_save.h"

typedef struct { char magic[4]; uint32_t version, size, seq, crc; } Hdr;

uint32_t vcpe_crc32(const void *data, uint32_t n, uint32_t crc)
{
    static uint32_t tab[256];
    if (!tab[1])
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++)
                c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            tab[i] = c;
        }
    const uint8_t *p = data;
    crc = ~crc;
    while (n--)
        crc = tab[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

static void slot_path(char *out, int n, const char *name, int slot)
{
    char f[256];
    snprintf(f, sizeof f, slot ? "%s.BAK" : "%s", name);
    vcpe_path(out, n, f);
}

/* 1: the slot holds a good save of this version and size (data in buf), 0: missing, -1: present but unusable */
static int read_slot(const char *name, int slot, void *buf, uint32_t size, uint32_t version, uint32_t *seq)
{
    char p[300];
    slot_path(p, sizeof p, name, slot);
    FILE *f = fopen(p, "rb");
    if (!f)
        return 0;
    Hdr h;
    int ok = fread(&h, 1, sizeof h, f) == sizeof h && !memcmp(h.magic, "VSAV", 4) && h.version == version &&
             h.size == size && fread(buf, 1, size, f) == size && fgetc(f) == EOF && vcpe_crc32(buf, size, 0) == h.crc;
    fclose(f);
    if (ok)
        *seq = h.seq;
    return ok ? 1 : -1;
}

int vcpe_save_read(const char *name, void *data, uint32_t size, uint32_t version)
{
    void *a = malloc(size ? size : 1), *b = malloc(size ? size : 1);
    uint32_t sa = 0, sb = 0;
    int ra = a ? read_slot(name, 0, a, size, version, &sa) : -1;
    int rb = b ? read_slot(name, 1, b, size, version, &sb) : -1;
    int r;
    if (ra == 1 && (rb != 1 || (int32_t)(sa - sb) >= 0)) { memcpy(data, a, size); r = 0; }
    else if (rb == 1) { memcpy(data, b, size); r = 0; }
    else r = (ra == 0 && rb == 0) ? -1 : -2;
    free(a);
    free(b);
    return r;
}

int vcpe_save_write(const char *name, const void *data, uint32_t size, uint32_t version)
{
    /* overwrite the slot that is not the newest good one */
    void *tmp = malloc(size ? size : 1);
    uint32_t sa = 0, sb = 0;
    int ra = tmp ? read_slot(name, 0, tmp, size, version, &sa) : 0;
    int rb = tmp ? read_slot(name, 1, tmp, size, version, &sb) : 0;
    free(tmp);
    int slot;
    uint32_t seq;
    if (ra == 1 && rb == 1) { slot = (int32_t)(sa - sb) >= 0 ? 1 : 0; seq = ((int32_t)(sa - sb) >= 0 ? sa : sb) + 1; }
    else if (ra == 1) { slot = 1; seq = sa + 1; }
    else if (rb == 1) { slot = 0; seq = sb + 1; }
    else { slot = 0; seq = 1; }
    Hdr h = {{'V', 'S', 'A', 'V'}, version, size, seq, vcpe_crc32(data, size, 0)};
    char p[300];
    slot_path(p, sizeof p, name, slot);
    FILE *f = fopen(p, "wb");
    if (!f)
        return -1;
    int ok = fwrite(&h, 1, sizeof h, f) == sizeof h && fwrite(data, 1, size, f) == size;
    ok = fclose(f) == 0 && ok;
    return ok ? 0 : -1;
}

void vcpe_save_erase(const char *name)
{
    char p[300];
    for (int s = 0; s < 2; s++) {
        slot_path(p, sizeof p, name, s);
        remove(p);
    }
}
