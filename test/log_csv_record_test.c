// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <string.h>

#include "log_csv_record.h"
#include "log_csv.h"

static int check(const char *input, int expected_read, int expected_split,
                 const char *expected_last)
{
  FILE *fp = tmpfile();
  char record[DW_CSV_RECORD_SIZE];
  char *fields[DW_CSV_FIELD_COUNT];
  int result, split = 0;
  if (fp == NULL) return 0;
  if (fputs(input, fp) == EOF || fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    return 0;
  }
  result = dw_csv_read_record(fp, record, sizeof(record));
  if (result == 1) split = dw_csv_split_record(record, fields, DW_CSV_FIELD_COUNT);
  fclose(fp);
  return result == expected_read && split == expected_split &&
         (!split || strcmp(fields[21], expected_last) == 0);
}

static int check_station_fields(void)
{
  FILE *fp = tmpfile();
  char encoded_source[32], encoded_heard[32];
  char record[DW_CSV_RECORD_SIZE];
  char *fields[DW_CSV_FIELD_COUNT];
  const char *encoded[DW_CSV_FIELD_COUNT];
  size_t i;
  int ok;
  if (fp == NULL) return 0;
  if (!quote_for_csv(encoded_source, sizeof(encoded_source), "A,B", 4) ||
      !quote_for_csv(encoded_heard, sizeof(encoded_heard), "C\r\nD", 5)) {
    fclose(fp);
    return 0;
  }
  for (i = 0; i < DW_CSV_FIELD_COUNT; ++i) encoded[i] = "";
  encoded[0] = "0";
  encoded[2] = "2026-11-03T16:00:00Z";
  encoded[3] = encoded_source;
  encoded[4] = encoded_heard;
  encoded[8] = "STATION";
  encoded[10] = "51.5";
  encoded[11] = "-0.1";
  encoded[21] = "ok";
  for (i = 0; i < DW_CSV_FIELD_COUNT; ++i) {
    if ((i > 0 && fputc(',', fp) == EOF) || fputs(encoded[i], fp) == EOF) {
      fclose(fp);
      return 0;
    }
  }
  if (fputc('\n', fp) == EOF || fseek(fp, 0, SEEK_SET) != 0) {
    fclose(fp);
    return 0;
  }
  ok = dw_csv_read_record(fp, record, sizeof(record)) == 1 &&
       dw_csv_split_record(record, fields, DW_CSV_FIELD_COUNT) &&
       strcmp(fields[3], "A,B") == 0 && strcmp(fields[4], "C\r\nD") == 0 &&
       strcmp(fields[10], "51.5") == 0 && strcmp(fields[21], "ok") == 0;
  fclose(fp);
  return ok;
}

int main(void)
{
  char long_row[DW_CSV_RECORD_SIZE + 1];
  size_t at = 0;
  int i;

  if (!check("0,0,2026-11-03T16:00:00Z,N0CALL,N0CALL,,0,,STATION,,51.5,-0.1,,,,,,,,,,hello\n",
             1, 1, "hello")) return 1;
  if (!check("0,0,2026-11-03T16:00:00Z,N0CALL,N0CALL,,0,,STATION,,51.5,-0.1,,,,,,,,,,\"a,b\"\n",
             1, 1, "a,b")) return 2;
  if (!check("0,0,2026-11-03T16:00:00Z,N0CALL,N0CALL,,0,,STATION,,51.5,-0.1,,,,,,,,,,\"a\tb\nc\"\n",
             1, 1, "a\tb\nc")) return 3;
  if (!check("0,0,2026-11-03T16:00:00Z,N0CALL,N0CALL,,0,,STATION,,51.5,-0.1,,,,,,,,,,\"unclosed\n",
             -1, 0, "")) return 4;
  if (!check("0,0,2026-11-03T16:00:00Z,N0CALL,N0CALL,,0,,STATION,,51.5,-0.1,,,,,,,,,\n",
             1, 0, "")) return 5;

  for (i = 0; i < 21; ++i) {
    long_row[at++] = 'x';
    long_row[at++] = ',';
  }
  long_row[at++] = '"';
  for (i = 0; i < 255; ++i) {
    long_row[at++] = '"';
    long_row[at++] = '"';
  }
  long_row[at++] = '"';
  long_row[at++] = '\n';
  long_row[at] = '\0';
  {
    char expected[256];
    memset(expected, '"', 255);
    expected[255] = '\0';
    if (!check(long_row, 1, 1, expected)) return 6;
  }
  memset(long_row, 'x', sizeof(long_row) - 1);
  long_row[sizeof(long_row) - 1] = '\0';
  if (!check(long_row, -1, 0, "")) return 7;
  if (!check_station_fields()) return 8;
  if (!check("0,0,2026-11-03T16:00:00Z,N0CALL,N0CALL,,0,,STATION,,51.5,-0.1,,,,,,,,,,\"bad\001byte\"\n",
             -1, 0, "")) return 9;
  if (!check("0,0,2026-11-03T16:00:00Z,N0CALL,N0CALL,,0,,STATION,,51.5,-0.1,,,,,,,,,,bad\377" "byte\n",
             -1, 0, "")) return 10;
  if (!check("0,0,2026-11-03T16:00:00Z,N0CALL,N0CALL,,0,,STATION,,51.5,-0.1,,,,,,,,,,caf\303\251\n",
             1, 1, "caf\303\251")) return 11;
  return 0;
}
