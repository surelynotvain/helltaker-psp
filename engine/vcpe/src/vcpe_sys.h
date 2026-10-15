/* VCPE system: HOME / sleep callbacks, CPU clock, files next to the EBOOT. */
#ifndef VCPE_SYS_H
#define VCPE_SYS_H

extern volatile int vcpe_running;      /* cleared by HOME > Exit or vcpe_quit() */
extern volatile int vcpe_resume_gen;   /* bumped after every sleep/resume: file readers reopen their files */

void vcpe_sys_init(int argc, char **argv);              /* callbacks thread, 333 MHz, game folder */
void vcpe_path(char *out, int n, const char *file);    /* "ms0:/PSP/GAME/<game>/" + file */
void vcpe_quit(void);
/* set by vcpe_gfx_pipeline(1): shows the frame handed to the GE (autotest reads, fatal screens). 0 when off */
extern void (*vcpe_present_hook)(void);
void vcpe_fatal(const char *title, const char *text);   /* readable start-up error, never returns */
/* largest block malloc can give right now, in KB. A game built with PSP_LARGE_MEMORY gets about 50 MB on a
 * PSP-2000/3000/Street/Go and about 20 MB on a PSP-1000, so this tells a 32 MB console from a 64 MB one */
int vcpe_heap_kb(void);
/* start-up checks with the standard player-facing messages (vcpe_fatal when they fail):
 * the 64 MB models (PSP-2000/3000/Go) - a PSP-1000 has ~20 MB for homebrew */
void vcpe_require_64mb(const char *title);
/* a file next to the EBOOT; what = how the player gets it ("Build it with tools/pack.py from your own copy...") */
void vcpe_require_file(const char *file, const char *title, const char *what);

/* Steps: what the engine (and the game) is doing right now, in words - "Loading textures: bundle 3 (7196 KB)" -
 * with OK / FAILED at the end. The loading screen (vcpe_load) shows them as a boot log in the top-left corner;
 * nothing listens otherwise, and then a step costs one test. Engine modules report their own steps (PAK reads,
 * texture bundles, volatile memory, sounds, music); a game adds its own around its work:
 *   int s = vcpe_step_begin("Creating level %d", n);  ...  vcpe_step_end(s, ok);
 * A step started inside another one is not reported (a bundle load's PAK read is part of "Loading textures"). */
enum { VCPE_STEP_RUN, VCPE_STEP_OK, VCPE_STEP_FAIL, VCPE_STEP_INFO };
extern void (*vcpe_step_hook)(int id, int status, const char *text);   /* text 0: only the status changed */
int vcpe_step_begin(const char *fmt, ...) __attribute__((format(printf, 1, 2)));   /* -1: not reported */
void vcpe_step_end(int id, int ok);
void vcpe_step_note(int status, const char *fmt, ...) __attribute__((format(printf, 2, 3)));   /* a one-line event */

#endif
