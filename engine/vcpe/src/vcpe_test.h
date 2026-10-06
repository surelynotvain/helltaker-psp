/* VCPE test harness: if AUTOTEST.TXT sits next to the EBOOT, the input comes from that script
 * (fixed frames, repeatable runs); "rec N" saves every Nth displayed frame to REC.RAW.
 * Built-in commands: wait N | press B | hold B N | rec N | quit. Games add their own. */
#ifndef VCPE_TEST_H
#define VCPE_TEST_H

enum { VCPE_CMD_UNKNOWN = 0, VCPE_CMD_NEXT = 1, VCPE_CMD_FRAME = 2 };   /* game command results */

typedef struct {
    unsigned (*button)(const char *name);   /* button name -> the game's input bits */
    /* game commands: VCPE_CMD_NEXT keeps reading this frame, VCPE_CMD_FRAME ends it */
    int (*command)(const char *cmd, const char *arg, int num, int num2, const char *line, unsigned *held);
    void (*pre)(void);                      /* every script frame first (e.g. a held stick) */
    int (*busy)(unsigned *held);            /* multi-frame actions (bot, goto): 1 = this frame is handled */
    int every_command_takes_a_frame;        /* the Helltaker harness timing: each line ends the frame */
} VcpeTestHooks;

extern int vcpe_autotest;                   /* AUTOTEST.TXT was found */
int vcpe_test_load(const VcpeTestHooks *hooks);   /* 1 if a script runs */
void vcpe_test_step(unsigned *held);        /* the script's input for this frame */
void vcpe_test_frame_end(void);             /* after the frame is shown: frame recording */

#endif
