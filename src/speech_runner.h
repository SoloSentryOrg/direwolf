#ifndef SPEECH_RUNNER_H
#define SPEECH_RUNNER_H

#include <stddef.h>

/* SPEECH_STDIN accepts an absolute local runner path. No packet bytes are
 * placed in an argument or passed to a command interpreter. */
int speech_runner_validate (const char *program);
int speech_runner_text_valid (const unsigned char *text, size_t len);
int speech_runner_run (const char *program, int channel,
                       const unsigned char *text, size_t text_len);

#endif
