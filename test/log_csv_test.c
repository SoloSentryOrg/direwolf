// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <string.h>

#include "../src/log_csv.h"

static int check(const char *input, size_t input_size,
                 size_t output_size, const char *expected)
{
  char output[520];
  memset(output, 'X', sizeof(output));
  if (!quote_for_csv(output, output_size, input, input_size))
    return 0;
  return strcmp(output, expected) == 0;
}

int main(void)
{
  char quoted[256];
  char expected[513];
  char short_output[5] = {'X', 'X', 'X', 'X', 'Z'};
  char unterminated[3] = {'a', 'b', 'c'};

  if (!check("plain", 6, 6, "plain") ||
      !check("a,b", 4, 6, "\"a,b\"") ||
      !check("\"", 2, 5, "\"\"\"\"") ||
      !check("a\nb", 4, 6, "\"a\nb\"") ||
      !check("a\rb", 4, 6, "\"a\rb\"") ||
      !check("", 1, 1, ""))
    return 1;

  memset(quoted, '"', sizeof(quoted) - 1);
  quoted[sizeof(quoted) - 1] = '\0';
  memset(expected, '"', sizeof(expected) - 1);
  expected[sizeof(expected) - 1] = '\0';
  if (!check(quoted, sizeof(quoted), sizeof(expected), expected))
    return 2;

  if (quote_for_csv(short_output, 4, "a,b", 4) ||
      short_output[0] != '\0' || short_output[4] != 'Z')
    return 3;
  if (quote_for_csv(short_output, 4, unterminated, sizeof(unterminated)) ||
      short_output[0] != '\0' || short_output[4] != 'Z')
    return 4;

  puts("bounded CSV fields passed");
  return 0;
}
