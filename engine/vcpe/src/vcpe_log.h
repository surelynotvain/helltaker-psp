/* VCPE log: a text log next to the EBOOT (the first thing to ask a player for when something goes wrong on their
 * PSP). Each line is flushed by default, so the log survives a crash; vcpe_log_buffered(1) keeps lines in memory
 * until vcpe_log_flush() / close / a fatal error (memory sticks are slow: use it for chatty per-frame logs). */
#ifndef VCPE_LOG_H
#define VCPE_LOG_H

int vcpe_log_open(const char *file);   /* e.g. "GAME.LOG": created fresh each start; 0 ok */
void vcpe_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void vcpe_log_buffered(int on);
void vcpe_log_flush(void);
void vcpe_log_close(void);
int vcpe_log_is_open(void);

#endif
