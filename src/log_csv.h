// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_LOG_CSV_H
#define DW_LOG_CSV_H

#include <stddef.h>
#include <string.h>

/* Bound both the source field and RFC 4180-style quoted CSV output. */
/* Return zero and leave an empty output if either boundary is invalid. */
static inline int quote_for_csv(char *out, size_t outsize,
                                const char *in, size_t in_capacity)
{
  size_t length = 0;
  size_t required = 1; /* NUL terminator. */
  size_t used = 0;
  int quoted = 0;

  if (out == NULL || outsize == 0 || in == NULL || in_capacity == 0)
    return 0;
  out[0] = '\0';

  while (length < in_capacity && in[length] != '\0') {
    if (in[length] == ',' || in[length] == '"' ||
        in[length] == '\r' || in[length] == '\n')
      quoted = 1;
    ++length;
  }
  if (length == in_capacity)
    return 0;

  if (quoted) {
    if (outsize < 3)
      return 0;
    required += 2; /* Surrounding quotes. */
  }
  for (size_t i = 0; i < length; ++i) {
    if (required >= outsize)
      return 0;
    ++required;
    if (quoted && in[i] == '"') {
      if (required >= outsize)
        return 0;
      ++required;
    }
  }

  if (!quoted) {
    memcpy(out, in, length);
    out[length] = '\0';
    return 1;
  }

  out[used++] = '"';
  for (size_t i = 0; i < length; ++i) {
    if (in[i] == '"')
      out[used++] = '"';
    out[used++] = in[i];
  }
  out[used++] = '"';
  out[used] = '\0';
  return 1;
}

#endif
