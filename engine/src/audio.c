/* Helltaker's sound policy (Manager.cs) on the VCPE mixer: 10 voices, the last one reserved for
 * the looping effect (Manager.sinLoopSource); RandomizeSfx pitch; deadSound; mute while ducked. */
#include <stdint.h>
#include "audio.h"
#include "gamedata.h"
#include "vcpe.h"

#define VOICES 10
static int next_voice;
static volatile int dead_sound;
static unsigned rng = 0x2545F491;

static unsigned rnd(void)
{
    rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
    return rng;
}

int audio_init(const char *music_pak)
{
    return vcpe_audio_init(music_pak, VOICES, g_sfx_off, g_sfx_size, (const VcpeSound *)g_sfx, NUM_SFX,
                           (const VcpeSound *)g_music);
}

void audio_shutdown(void) { vcpe_audio_shutdown(); }

void sfx_play(int id, int random_pitch)
{
    if (id < 0 || id >= NUM_SFX || dead_sound)
        return;
    int vi = next_voice;
    next_voice = (next_voice + 1) % (VOICES - 1);
    float pitch = random_pitch ? 0.95f + (rnd() % 1000) * 0.0001f : 1.0f;
    vcpe_voice_start(vi, id, pitch, 0);
}

/* the last voice is reserved for one looping effect (Manager.sinLoopSource) */
void sfx_loop_start(int id) { vcpe_voice_start(VOICES - 1, id, 1.0f, 1); }
void sfx_loop_stop(void) { vcpe_voice_stop(VOICES - 1); }

void sfx_play_any(const int *ids, int n, int random_pitch)
{
    if (n > 0)
        sfx_play(ids[rnd() % n], random_pitch);
}

void sfx_stop_all(void) { vcpe_voices_stop_all(); }
void sfx_mute_new(int on) { dead_sound = on; }

void music_change(int track) { vcpe_music_play(track); }
int music_current(void) { return vcpe_music_current(); }
void music_stop_now(void) { vcpe_music_stop_now(); }

static int level_gain(int lvl)
{
    /* Manager.VolumeChange: lvl/3 (+0.04 for 1 and 2) */
    float v = lvl / 3.0f + ((lvl == 1 || lvl == 2) ? 0.04f : 0);
    return (int)(v * 256);
}

void music_set_volume(int level) { vcpe_music_gain(level_gain(level)); }
void sfx_set_volume(int level) { vcpe_sfx_gain(level_gain(level)); }
void music_duck(int on) { vcpe_music_scale(1, on ? 10 : 1); }   /* Manager.mute = 0.1 */
void music_pause(int on) { vcpe_music_pause(on); }
