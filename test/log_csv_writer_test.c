// SPDX-License-Identifier: GPL-2.0-or-later

#include <stdio.h>
#include <string.h>

#include "log.h"
#include "log_csv_record.h"

/* The logger's packet metadata boundary is exercised with synthetic values. */
int ax25_get_num_addr(packet_t pp) { (void)pp; return 2; }
int ax25_get_heard(packet_t pp) { (void)pp; return AX25_SOURCE; }
void ax25_get_addr_with_ssid(packet_t pp, int n, char *station)
{
  (void)pp;
  (void)n;
  memcpy(station, "C\r\nD", 5);
}
int ax25_get_dti(packet_t pp) { (void)pp; return '!'; }
int ax25_get_h(packet_t pp, int n) { (void)pp; (void)n; return 0; }
int ax25_get_rr(packet_t pp, int n) { (void)pp; (void)n; return 0; }
int ax25_alevel_to_text(alevel_t level, char text[AX25_ALEVEL_TO_TEXT_SIZE])
{
  (void)level;
  memcpy(text, "0", 2);
  return 1;
}

int main(void)
{
  char path[] = "log-csv-writer-test.csv";
  char record[DW_CSV_RECORD_SIZE];
  char *fields[DW_CSV_FIELD_COUNT];
  decode_aprs_t decoded;
  alevel_t level;
  FILE *fp;
  int fake_packet = 1;
  int ok;

  memset(&decoded, 0, sizeof(decoded));
  memset(&level, 0, sizeof(level));
  memcpy(decoded.g_src, "A,B", 4);
  memcpy(decoded.g_name, "STATION", 8);
  memcpy(decoded.g_comment, "ordinary", 9);
  decoded.g_symbol_table = '/';
  decoded.g_symbol_code = '>';
  decoded.g_lat = 51.5;
  decoded.g_lon = -0.1;

  remove(path);
  log_init(0, path);
  log_write(0, &decoded, (packet_t)&fake_packet, level, (retry_t)0);
  log_term();
  fp = fopen(path, "rb");
  if (fp == NULL) return 1;
  ok = dw_csv_read_record(fp, record, sizeof(record)) == 1 &&
       dw_csv_split_record(record, fields, DW_CSV_FIELD_COUNT) &&
       strcmp(fields[0], "chan") == 0 &&
       dw_csv_read_record(fp, record, sizeof(record)) == 1 &&
       dw_csv_split_record(record, fields, DW_CSV_FIELD_COUNT) &&
       strcmp(fields[3], "A,B") == 0 &&
       strcmp(fields[4], "C\r\nD") == 0 &&
       strcmp(fields[8], "STATION") == 0 &&
       strcmp(fields[21], "ordinary") == 0 &&
       dw_csv_read_record(fp, record, sizeof(record)) == 0;
  fclose(fp);
  remove(path);
  return ok ? 0 : 2;
}
