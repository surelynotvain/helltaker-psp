/* VCPE audio: software mixer (ADPCM SFX voices + streamed ADPCM music). Games put their own
 * sound policy (which voice plays what, volume curves) on top of these primitives. */
#ifndef VCPE_AUDIO_H
#define VCPE_AUDIO_H
#include <stdint.h>

#define VCPE_MAX_VOICES 32
/* the converters write this table layout (name, PAK offset, bytes, samples) */
typedef struct { const char *name; uint32_t off, size, samples; } VcpeSound;

int vcpe_audio_init(const char *music_pak, int voices, uint32_t sfx_off, uint32_t sfx_size,
                    const VcpeSound *sfx, int sfx_n, const VcpeSound *music);
void vcpe_audio_shutdown(void);

/* voices: start returns a handle (generation << 8 | voice) or -1 */
int vcpe_voice_start(int voice, int sfx_id, float pitch, int loop);
void vcpe_voice_stop(int voice);
int vcpe_voice_active(int voice);
void vcpe_voice_pause(int voice, int on);
void vcpe_voice_pitch(int voice, float pitch);
void vcpe_voice_gain(int voice, int gain256);
int vcpe_voice_count(void);
int vcpe_handle_voice(int handle);     /* the voice still playing that handle's sound, or -1 */
void vcpe_voices_stop_all(void);

/* music: play fades the current track out, then starts the new one (-1: silence) */
void vcpe_music_play(int track);
int vcpe_music_current(void);
void vcpe_music_stop_now(void);
void vcpe_music_pause(int on);

void vcpe_sfx_gain(int gain256);
void vcpe_music_gain(int gain256);
void vcpe_music_scale(int num, int den);   /* extra music factor: ducking, muffling (1/1 = none) */
uint32_t vcpe_audio_ms(void);              /* audio time mixed so far, in ms */

#endif
