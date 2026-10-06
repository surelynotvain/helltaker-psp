/* VCPE system: HOME / sleep callbacks, CPU clock, files next to the EBOOT. */
#ifndef VCPE_SYS_H
#define VCPE_SYS_H

extern volatile int vcpe_running;      /* cleared by HOME > Exit or vcpe_quit() */
extern volatile int vcpe_resume_gen;   /* bumped after every sleep/resume: file readers reopen their files */

void vcpe_sys_init(int argc, char **argv);              /* callbacks thread, 333 MHz, game folder */
void vcpe_path(char *out, int n, const char *file);    /* "ms0:/PSP/GAME/<game>/" + file */
void vcpe_quit(void);

#endif
