#ifndef AUDIO_H
#define AUDIO_H

int audio_init(const char *music_pak);
void audio_shutdown(void);

/* RandomizeSfx: optional +-5% pitch like Manager.RandomizeSfx(pitchBool=true) */
void sfx_play(int id, int random_pitch);
/* pick one of n ids at random (Manager.RandomizeSfx(params clips)) */
void sfx_play_any(const int *ids, int n, int random_pitch);
void sfx_stop_all(void);
void sfx_loop_start(int id);
void sfx_loop_stop(void);
void sfx_mute_new(int on); /* Manager.deadSound */

/* Manager.SongChange: fade out current track then start (or stop with -1) */
void music_change(int track);
int music_current(void);
void music_stop_now(void);
void music_set_volume(int level); /* 0..3 like the pause menu */
void sfx_set_volume(int level);
void music_duck(int on);          /* Manager.mute = 0.1 */
void music_pause(int on);

#endif
