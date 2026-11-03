// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef DW_LOG_CSV_RECORD_H
#define DW_LOG_CSV_RECORD_H

#include <stdio.h>
#include <stddef.h>

/* More than twice the current producer's bounded 22-field maximum. */
#define DW_CSV_RECORD_SIZE 4096
#define DW_CSV_FIELD_COUNT 22

/* GPX declares UTF-8; reject malformed sequences instead of emitting bad XML. */
static int dw_csv_utf8_valid(const char *text)
{
  const unsigned char *p = (const unsigned char *)text;
  while (*p != 0) {
    unsigned char first = *p;
    unsigned int length, i;
    if (first < 0x80) { ++p; continue; }
    if (first >= 0xc2 && first <= 0xdf) length = 2;
    else if (first >= 0xe0 && first <= 0xef) length = 3;
    else if (first >= 0xf0 && first <= 0xf4) length = 4;
    else return 0;
    for (i = 1; i < length; ++i) {
      if (p[i] == 0 || p[i] < 0x80 || p[i] > 0xbf) return 0;
    }
    if ((first == 0xe0 && p[1] < 0xa0) ||
        (first == 0xed && p[1] > 0x9f) ||
        (first == 0xf0 && p[1] < 0x90) ||
        (first == 0xf4 && p[1] > 0x8f) ||
        (first == 0xef && p[1] == 0xbf && p[2] >= 0xbe)) return 0;
    p += length;
  }
  return 1;
}

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
  if (!dw_csv_utf8_valid(record)) return -1;
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
