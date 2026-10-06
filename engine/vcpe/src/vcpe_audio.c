/* VCPE audio: software mixer. IMA-ADPCM SFX voices (22.05 kHz mono, resident) + streamed
 * IMA-ADPCM music (44.1 kHz stereo) from the music PAK, output at 44.1 kHz stereo.
 * Games put their own sound policy (which voice plays what, volume curves) on top. */
#include <pspkernel.h>
#include <pspaudio.h>
#include <pspiofilemgr.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include "vcpe_sys.h"
#include "vcpe_pak.h"
#include "vcpe_audio.h"

#define OUT_FRAMES 1024
#define ADPCM_BLOCK 1024
#define SFX_BLOCK_BYTES (4 + ADPCM_BLOCK / 2)
#define MUS_BLOCK_BYTES (2 * (4 + ADPCM_BLOCK / 2))
#define RING_BLOCKS 48

static const int step_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
    50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230,
    253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963,
    1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327,
    3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442,
    11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
static const int index_table[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};

typedef struct { int pred, index; } Adpcm;

static inline int adpcm_step(Adpcm *s, int code)
{
    int step = step_table[s->index];
    int delta = step >> 3;
    if (code & 4) delta += step;
    if (code & 2) delta += step >> 1;
    if (code & 1) delta += step >> 2;
    if (code & 8) s->pred -= delta; else s->pred += delta;
    if (s->pred > 32767) s->pred = 32767;
    else if (s->pred < -32768) s->pred = -32768;
    s->index += index_table[code];
    if (s->index < 0) s->index = 0;
    else if (s->index > 88) s->index = 88;
    return s->pred;
}

/* ---------------------------------------------------------------- SFX voices */
typedef struct {
    volatile int active;
    int loop;
    const uint8_t *data;
    uint32_t samples;
    uint32_t pos;        /* source sample index of `cur` */
    uint32_t frac;       /* 16.16 */
    uint32_t step;
    int cur, next;
    Adpcm st;
    volatile int paused;
    int gain;            /* 0..256 */
    uint16_t gen;
} Voice;

static uint8_t *sfx_mem;
static const VcpeSound *sfx_tab, *music_tab;
static int nsfx, nvoices;
static Voice voices[VCPE_MAX_VOICES];
static volatile int sfx_vol = 256, mus_vol = 256, mus_paused = 0;
static volatile int mus_num = 1, mus_den = 1;
static volatile uint32_t mixed_ms;

static int sfx_sample(Voice *v, uint32_t i)
{
    /* decode sample i sequentially (i only ever increases by one) */
    uint32_t blk = i / ADPCM_BLOCK, k = i % ADPCM_BLOCK;
    const uint8_t *b = v->data + blk * SFX_BLOCK_BYTES;
    if (k == 0) {
        v->st.pred = (int16_t)(b[0] | (b[1] << 8));
        v->st.index = b[2];
    }
    int code = (b[4 + (k >> 1)] >> ((k & 1) * 4)) & 15;
    return adpcm_step(&v->st, code);
}

int vcpe_voice_start(int vi, int id, float pitch, int loop)
{
    if (id < 0 || id >= nsfx || !sfx_mem || vi < 0 || vi >= nvoices)
        return -1;
    Voice *v = &voices[vi];
    v->active = 0;
    v->loop = loop;
    v->paused = 0;
    v->gain = 256;
    v->data = sfx_mem + sfx_tab[id].off;
    v->samples = sfx_tab[id].samples;
    v->pos = 0;
    v->frac = 0;
    v->step = (uint32_t)(32768.0f * pitch);   /* 22050 -> 44100 */
    v->cur = sfx_sample(v, 0);
    v->next = v->samples > 1 ? sfx_sample(v, 1) : 0;
    v->gen++;
    v->active = 1;
    return (v->gen << 8) | vi;
}

void vcpe_voice_stop(int vi) { if (vi >= 0 && vi < nvoices) { voices[vi].active = 0; voices[vi].loop = 0; } }
int vcpe_voice_active(int vi) { return vi >= 0 && vi < nvoices && voices[vi].active; }
void vcpe_voice_pause(int vi, int on) { if (vi >= 0 && vi < nvoices) voices[vi].paused = on; }
void vcpe_voice_pitch(int vi, float pitch) { if (vi >= 0 && vi < nvoices) voices[vi].step = (uint32_t)(32768.0f * pitch); }
void vcpe_voice_gain(int vi, int gain) { if (vi >= 0 && vi < nvoices) voices[vi].gain = gain; }
int vcpe_voice_count(void) { return nvoices; }

int vcpe_handle_voice(int h)
{
    if (h < 0) return -1;
    int vi = h & 0xFF;
    return vi < nvoices && voices[vi].gen == ((h >> 8) & 0xFFFF) ? vi : -1;
}

void vcpe_voices_stop_all(void)
{
    for (int i = 0; i < nvoices; i++)
        voices[i].active = 0;
}

/* ---------------------------------------------------------------- music */
static int mus_fd = -1;
static char mus_path[256];
static uint8_t ring[RING_BLOCKS][MUS_BLOCK_BYTES];
static volatile int ring_rd, ring_wr;           /* block counters */
static volatile int want_track = -1, cur_track = -1;
static volatile int gen, gen_ack;
static volatile int fade = 256, fading_out = 0;
static int pending_track = -2;
static int mus_blk_pos;                         /* frame inside current block */
static Adpcm mus_st[2];
static int io_sema = -1;
static volatile int running;

static int io_thread(SceSize args, void *argp)
{
    int my_gen = -1, track = -1, fd_gen = vcpe_resume_gen;
    uint32_t off = 0, size = 0, pos = 0;
    (void)args; (void)argp;
    while (running) {
        if (my_gen != gen) {
            my_gen = gen;
            track = want_track;
            ring_rd = ring_wr = 0;
            pos = 0;
            if (track >= 0 && mus_fd >= 0) {
                off = music_tab[track].off;
                size = music_tab[track].size / MUS_BLOCK_BYTES * MUS_BLOCK_BYTES;
            } else {
                size = 0;
            }
            gen_ack = my_gen;
        }
        int filled = 0;
        while (running && size && ring_wr - ring_rd < RING_BLOCKS && my_gen == gen) {
            int n = 8;
            int space = RING_BLOCKS - (ring_wr - ring_rd);
            int contig = RING_BLOCKS - (ring_wr % RING_BLOCKS);
            if (n > space) n = space;
            if (n > contig) n = contig;
            uint32_t left = (size - pos) / MUS_BLOCK_BYTES;
            if ((uint32_t)n > left) n = left;
            /* after a sleep the old handle is stale: reopen, and never queue a block that was not read
             * (the ring would replay old audio) */
            if (fd_gen != vcpe_resume_gen || mus_fd < 0) {
                if (mus_fd >= 0)
                    sceIoClose(mus_fd);
                fd_gen = vcpe_resume_gen;
                mus_fd = sceIoOpen(mus_path, PSP_O_RDONLY, 0);
            }
            int want = n * MUS_BLOCK_BYTES, r = -1;
            if (mus_fd >= 0) {
                sceIoLseek32(mus_fd, off + pos, PSP_SEEK_SET);
                r = sceIoRead(mus_fd, ring[ring_wr % RING_BLOCKS], want);
            }
            if (my_gen != gen)
                break;
            if (r != want) {
                if (mus_fd >= 0)
                    sceIoClose(mus_fd);
                mus_fd = -1;
                sceKernelDelayThread(100000);
                continue;
            }
            ring_wr += n;
            pos += n * MUS_BLOCK_BYTES;
            if (pos >= size)
                pos = 0; /* tracks loop */
            filled = 1;
        }
        if (!filled)
            sceKernelWaitSemaCB(io_sema, 1, 0);
    }
    return 0;
}

static void music_frame(int *l, int *r)
{
    if (gen_ack != gen || ring_rd >= ring_wr || mus_paused) {
        *l = *r = 0;
        return;
    }
    const uint8_t *b = ring[ring_rd % RING_BLOCKS];
    int k = mus_blk_pos;
    if (k == 0) {
        for (int c = 0; c < 2; c++) {
            mus_st[c].pred = (int16_t)(b[c * 4] | (b[c * 4 + 1] << 8));
            mus_st[c].index = b[c * 4 + 2];
        }
    }
    const uint8_t *d = b + 8;
    int sh = (k & 1) * 4;
    *l = adpcm_step(&mus_st[0], (d[k >> 1] >> sh) & 15);
    *r = adpcm_step(&mus_st[1], (d[ADPCM_BLOCK / 2 + (k >> 1)] >> sh) & 15);
    if (++mus_blk_pos == ADPCM_BLOCK) {
        mus_blk_pos = 0;
        ring_rd++;
        sceKernelSignalSema(io_sema, 1);
    }
}

static void start_track(int t)
{
    cur_track = t;
    want_track = t;
    mus_blk_pos = 0;
    gen++;
    sceKernelSignalSema(io_sema, 1);
}

void vcpe_music_play(int track)
{
    if (track == cur_track && !fading_out)
        return;
    if (cur_track < 0) {
        fade = 256;
        start_track(track);
        return;
    }
    pending_track = track;
    fading_out = 1;
}

int vcpe_music_current(void) { return fading_out ? pending_track : cur_track; }

void vcpe_music_stop_now(void)
{
    fading_out = 0;
    start_track(-1);
}

void vcpe_sfx_gain(int gain256) { sfx_vol = gain256; }
void vcpe_music_gain(int gain256) { mus_vol = gain256; }
void vcpe_music_scale(int num, int den) { mus_den = den > 0 ? den : 1; mus_num = num; }
void vcpe_music_pause(int on) { mus_paused = on; }
uint32_t vcpe_audio_ms(void) { return mixed_ms; }

/* ---------------------------------------------------------------- output */
static short __attribute__((aligned(64))) outbuf[2][OUT_FRAMES * 2];
static int audio_ch = -1;

static inline short clip16(int s)
{
    return s > 32767 ? 32767 : (s < -32768 ? -32768 : s);
}

static int mix_thread(SceSize args, void *argp)
{
    int which = 0;
    (void)args; (void)argp;
    while (running) {
        short *out = outbuf[which];
        which ^= 1;
        /* song change: volume -= 0.04 every 0.01 s */
        if (fading_out) {
            fade -= 24;
            if (fade <= 0) {
                fade = 256;
                fading_out = 0;
                start_track(pending_track);
            }
        }
        int mg = (((mus_vol * fade) >> 8) * mus_num) / mus_den;
        int sg = sfx_vol;
        mixed_ms += (OUT_FRAMES * 1000) / 44100;
        for (int i = 0; i < OUT_FRAMES; i++) {
            int l, r;
            music_frame(&l, &r);
            l = (l * mg) >> 8;
            r = (r * mg) >> 8;
            int s = 0;
            for (int vi = 0; vi < nvoices; vi++) {
                Voice *v = &voices[vi];
                if (!v->active || v->paused)
                    continue;
                s += ((v->cur + (((v->next - v->cur) * (int)(v->frac >> 4)) >> 12)) * v->gain) >> 8;
                v->frac += v->step;
                while (v->frac >= 65536) {
                    v->frac -= 65536;
                    v->pos++;
                    v->cur = v->next;
                    if (v->pos + 1 < v->samples)
                        v->next = sfx_sample(v, v->pos + 1);
                    else if (v->pos >= v->samples) {
                        if (v->loop) {
                            v->pos = 0;
                            v->cur = sfx_sample(v, 0);
                            v->next = sfx_sample(v, 1);
                            continue;
                        }
                        v->active = 0;
                        break;
                    } else
                        v->next = 0;
                }
            }
            s = (s * sg) >> 8;
            out[i * 2] = clip16(l + s);
            out[i * 2 + 1] = clip16(r + s);
        }
        sceAudioOutputPannedBlocking(audio_ch, PSP_AUDIO_VOLUME_MAX, PSP_AUDIO_VOLUME_MAX, out);
    }
    return 0;
}

int vcpe_audio_init(const char *music_pak, int voices_n, uint32_t sfx_off, uint32_t sfx_size,
                    const VcpeSound *sfx, int sfx_n, const VcpeSound *music)
{
    nvoices = voices_n > VCPE_MAX_VOICES ? VCPE_MAX_VOICES : voices_n;
    sfx_tab = sfx;
    nsfx = sfx_n;
    music_tab = music;
    sfx_mem = vcpe_pak_read_alloc(sfx_off, sfx_size);
    snprintf(mus_path, sizeof mus_path, "%s", music_pak);
    mus_fd = sceIoOpen(music_pak, PSP_O_RDONLY, 0);
    audio_ch = sceAudioChReserve(PSP_AUDIO_NEXT_CHANNEL, OUT_FRAMES, PSP_AUDIO_FORMAT_STEREO);
    if (audio_ch < 0)
        return audio_ch;
    running = 1;
    io_sema = sceKernelCreateSema("mus_io", 0, 0, 64, 0);
    int t = sceKernelCreateThread("mus_io", io_thread, 0x14, 0x4000, PSP_THREAD_ATTR_USER, 0);
    if (t >= 0)
        sceKernelStartThread(t, 0, 0);
    t = sceKernelCreateThread("mixer", mix_thread, 0x12, 0x8000, PSP_THREAD_ATTR_USER, 0);
    if (t >= 0)
        sceKernelStartThread(t, 0, 0);
    return 0;
}

void vcpe_audio_shutdown(void)
{
    running = 0;
    if (io_sema >= 0)
        sceKernelSignalSema(io_sema, 1);
    sceKernelDelayThread(50000);
    if (audio_ch >= 0)
        sceAudioChRelease(audio_ch);
    if (mus_fd >= 0)
        sceIoClose(mus_fd);
}
