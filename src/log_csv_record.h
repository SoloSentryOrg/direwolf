// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_LOG_CSV_RECORD_H
#define DW_LOG_CSV_RECORD_H

#include <stdio.h>
#include <stddef.h>

/* More than twice the current producer's bounded 22-field maximum. */
#define DW_CSV_RECORD_SIZE 4096
#define DW_CSV_FIELD_COUNT 22

/* Return 1 for a complete record, 0 for EOF, or -1 for invalid/overlong input. */
static int dw_csv_read_record(FILE *fp, char *record, size_t capacity)
{
  size_t length = 0;
  int ch;
  int quoted = 0;

  if (fp == NULL || record == NULL || capacity < 2) return -1;
  while ((ch = fgetc(fp)) != EOF) {
    if (ch == 0 || (ch < 32 && ch != '\t' && ch != '\r' && ch != '\n') ||
        length >= capacity - 1) return -1;
    record[length++] = (char)ch;
    if (ch == '"') quoted = !quoted;
    if (ch == '\n' && !quoted) break;
  }
  if (ferror(fp) || quoted) return -1;
  if (length == 0) return 0;
  if (record[length - 1] == '\n') --length;
  if (length > 0 && record[length - 1] == '\r') --length;
  record[length] = '\0';
  return 1;
}

/* Decode exactly 22 fields in place, retaining tabs and quoted line breaks. */
static int dw_csv_split_record(char *record, char **fields, size_t field_count)
{
  size_t read_at = 0, write_at = 0, count = 0;
  int delimiter;

  if (record == NULL || fields == NULL || field_count == 0) return 0;
  for (;;) {
    if (count == field_count) return 0;
    fields[count++] = record + write_at;
    if (record[read_at] == '"') {
      ++read_at;
      for (;;) {
        char ch = record[read_at++];
        if (ch == '\0') return 0;
        if (ch == '"') {
          if (record[read_at] == '"') {
            record[write_at++] = '"';
            ++read_at;
          } else if (record[read_at] == ',' || record[read_at] == '\0') {
            break;
          } else {
            return 0;
          }
        } else {
          record[write_at++] = ch;
        }
      }
    } else {
      while (record[read_at] != '\0' && record[read_at] != ',') {
        if (record[read_at] == '"' || record[read_at] == '\r' ||
            record[read_at] == '\n') return 0;
        record[write_at++] = record[read_at++];
      }
    }
    delimiter = record[read_at];
    if (delimiter == ',') ++read_at;
    record[write_at++] = '\0';
    if (delimiter == '\0') return count == field_count;
  }
}

#endif
