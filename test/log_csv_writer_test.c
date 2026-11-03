// SPDX-License-Identifier: GPL-2.0-or-later

#include "direwolf.h"

#include <stdio.h>
#include <string.h>

#include "log.h"
#include "log_csv_record.h"

static void address(unsigned char *bytes, const char name[6], int last)
{
  int i;
  for (i = 0; i < 6; ++i) bytes[i] = (unsigned char)name[i] << 1;
  bytes[6] = (unsigned char)(last ? 0x61 : 0x60);
}

static int write_frame(const char source[6], int valid_expected)
{
  unsigned char frame[17];
  decode_aprs_t decoded;
  alevel_t level;
  packet_t pp;
  int valid;
  memset(&decoded, 0, sizeof(decoded));
  memset(&level, 0, sizeof(level));
  address(frame, "APRS  ", 0);
  address(frame + 7, source, 1);
  frame[14] = 0x03;
  frame[15] = 0xf0;
  frame[16] = '!';
  pp = ax25_from_frame(frame, sizeof(frame), level);
  if (pp == NULL) return 0;
  valid = ax25_check_addresses(pp);
  if ((valid != 0) != valid_expected) {
    ax25_delete(pp);
    return 0;
  }
  ax25_get_addr_with_ssid(pp, AX25_SOURCE, decoded.g_src);
  memcpy(decoded.g_name, "STATION", 8);
  memcpy(decoded.g_comment, "ordinary", 9);
  decoded.g_symbol_table = '/';
  decoded.g_symbol_code = '>';
  decoded.g_lat = 51.5;
  decoded.g_lon = -0.1;
  log_write(0, &decoded, pp, level, (retry_t)0);
  ax25_delete(pp);
  return 1;
}

int main(void)
{
  char path[] = "log-csv-writer-test.csv";
  char record[DW_CSV_RECORD_SIZE];
  char *fields[DW_CSV_FIELD_COUNT];
  const char *expected[] = {"N0CALL", "A,B", "A\nB"};
  FILE *fp;
  int i, ok;

  remove(path);
  log_init(0, path);
  ok = write_frame("N0CALL", 1) &&
       write_frame("A,B   ", 0) &&
       write_frame("A\nB   ", 0);
  log_term();
  if (!ok) {
    remove(path);
    return 1;
  }
  fp = fopen(path, "rb");
  if (fp == NULL) return 2;
  ok = dw_csv_read_record(fp, record, sizeof(record)) == 1 &&
       dw_csv_split_record(record, fields, DW_CSV_FIELD_COUNT) &&
       strcmp(fields[0], "chan") == 0;
  for (i = 0; i < 3 && ok; ++i) {
    ok = dw_csv_read_record(fp, record, sizeof(record)) == 1 &&
         dw_csv_split_record(record, fields, DW_CSV_FIELD_COUNT) &&
         strcmp(fields[3], expected[i]) == 0 &&
         strcmp(fields[4], expected[i]) == 0 &&
         strcmp(fields[8], "STATION") == 0 &&
         strcmp(fields[21], "ordinary") == 0;
  }
  if (ok) ok = dw_csv_read_record(fp, record, sizeof(record)) == 0;
  fclose(fp);
  remove(path);
  return ok ? 0 : 3;
}
