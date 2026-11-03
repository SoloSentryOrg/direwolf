// SPDX-License-Identifier: GPL-2.0-or-later
#include "direwolf.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !__WIN32__
#include <unistd.h>
#endif

#include "ax25_pad.h"
#include "speech_runner.h"

static const unsigned char ordinary[] = "normal speech";
static const unsigned char hostile[] = "\"; echo no-shell; $(printf text) & | < >";
static const unsigned char multiline[] = "first line\nsecond line";

static int child_main (int channel)
{
  unsigned char got[AX25_MAX_INFO_LEN + 1];
  unsigned char expected_max[AX25_MAX_INFO_LEN];
  const unsigned char *expected = NULL;
  size_t expected_len = 0, used = 0;
  int ch;

  if (channel == 4) return 7;
  if (channel == 5) {
#if __WIN32__
    Sleep(2000);
#else
    sleep(2);
#endif
    return 0;
  }
  if (channel == 0) {
    expected = ordinary;
    expected_len = sizeof(ordinary) - 1;
  } else if (channel == 1) {
    expected = hostile;
    expected_len = sizeof(hostile) - 1;
  } else if (channel == 2) {
    expected = multiline;
    expected_len = sizeof(multiline) - 1;
  } else if (channel == 3) {
    memset(expected_max, 'A', sizeof(expected_max));
    expected = expected_max;
    expected_len = sizeof(expected_max);
  } else return 8;
  while ((ch = getchar()) != EOF) {
    if (used >= sizeof(got)) return 9;
    got[used++] = (unsigned char)ch;
  }
  return used == expected_len && memcmp(got, expected, used) == 0 ? 0 : 10;
}

static int expect_ok (const char *runner, int channel,
                      const unsigned char *data, size_t len)
{
  int rc = speech_runner_run(runner, channel, data, len);
  if (rc != 0) fprintf(stderr, "SPEECH_STDIN case %d failed: %d\n", channel, rc);
  return rc == 0;
}

int main (int argc, char **argv)
{
  unsigned char maximum[AX25_MAX_INFO_LEN];
  const unsigned char nul_text[] = {'x', 0, 'y'};
  const unsigned char binary_text[] = {'x', 1, 'y'};
  int channel;
  if (argc == 2) {
    channel = atoi(argv[1]);
    return child_main(channel);
  }
  if (argc != 1 || !speech_runner_validate(argv[0])) return 1;
  memset(maximum, 'A', sizeof(maximum));
  if (!expect_ok(argv[0], 0, ordinary, sizeof(ordinary) - 1) ||
      !expect_ok(argv[0], 1, hostile, sizeof(hostile) - 1) ||
      !expect_ok(argv[0], 2, multiline, sizeof(multiline) - 1) ||
      !expect_ok(argv[0], 3, maximum, sizeof(maximum))) return 2;
  if (speech_runner_run(argv[0], 0, ordinary, 0) == 0 ||
      speech_runner_run(argv[0], 0, nul_text, sizeof(nul_text)) == 0 ||
      speech_runner_run(argv[0], 0, binary_text, sizeof(binary_text)) == 0 ||
      speech_runner_run(argv[0], 0, ordinary, AX25_MAX_INFO_LEN + 1) == 0 ||
      speech_runner_run(argv[0], -1, ordinary, sizeof(ordinary) - 1) == 0 ||
      speech_runner_run("relative-runner", 0, ordinary, sizeof(ordinary) - 1) == 0) return 3;
  if (speech_runner_run(argv[0], 4, ordinary, sizeof(ordinary) - 1) == 0 ||
      speech_runner_run(argv[0], 5, ordinary, sizeof(ordinary) - 1) == 0) return 4;
#if __WIN32__
  {
    char batch[MAX_PATH];
    size_t n = strlen(argv[0]);
    if (n < 4 || n + 1 >= sizeof(batch)) return 5;
    strcpy(batch, argv[0]);
    strcpy(batch + n - 4, ".bat");
    if (!expect_ok(batch, 0, ordinary, sizeof(ordinary) - 1)) return 6;
  }
#endif
  return 0;
}
