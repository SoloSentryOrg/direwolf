// SPDX-License-Identifier: GPL-2.0-or-later

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../external/geotranz/mgrs.h"
#include "../external/geotranz/usng.h"

static int invalid_polar_mgrs(void)
{
  char bad[] = "CAA";
  char hemisphere = '?';
  double east = 0, north = 0, lat = 0, lon = 0;
  for (char first = 'C'; first <= 'X'; ++first) {
    bad[0] = first;
    if ((Convert_MGRS_To_UPS(bad, &hemisphere, &east, &north) & MGRS_STRING_ERROR) == 0 ||
        (Convert_MGRS_To_Geodetic(bad, &lat, &lon) & MGRS_STRING_ERROR) == 0)
      return 0;
  }
  return 1;
}

static int invalid_polar_usng(void)
{
  char bad[] = "CAA";
  char hemisphere = '?';
  double east = 0, north = 0, lat = 0, lon = 0;
  for (char first = 'C'; first <= 'X'; ++first) {
    bad[0] = first;
    if ((Convert_USNG_To_UPS(bad, &hemisphere, &east, &north) & USNG_STRING_ERROR) == 0 ||
        (Convert_USNG_To_Geodetic(bad, &lat, &lon) & USNG_STRING_ERROR) == 0)
      return 0;
  }
  return 1;
}

static int trailing_mgrs(void)
{
  char bad[] = "31UDQ12x";
  double lat = 0, lon = 0;
  return (Convert_MGRS_To_Geodetic(bad, &lat, &lon) & MGRS_STRING_ERROR) != 0;
}

static int trailing_usng(void)
{
  char bad[] = "31UDQ12x";
  double lat = 0, lon = 0;
  return (Convert_USNG_To_Geodetic(bad, &lat, &lon) & USNG_STRING_ERROR) != 0;
}

static int valid_coordinates(void)
{
  char mgrs[] = "31UDQ4825111932";
  char usng[] = "31UDQ4825111932";
  double lat = 0, lon = 0;
  if (Convert_MGRS_To_Geodetic(mgrs, &lat, &lon) != 0 ||
      fabs(lat - 0.852738) > 0.001 || fabs(lon - 0.040046) > 0.001)
    return 0;
  return Convert_USNG_To_Geodetic(usng, &lat, &lon) == 0 &&
         fabs(lat - 0.852738) <= 0.001 && fabs(lon - 0.040046) <= 0.001;
}

static int valid_polar_coordinates(void)
{
  char mgrs[32], usng[32], hemisphere = '?';
  double east = 0, north = 0;
  if (Convert_UPS_To_MGRS('N', 2000000.0, 2000000.0, 5, mgrs) != 0 ||
      Convert_MGRS_To_UPS(mgrs, &hemisphere, &east, &north) != 0 ||
      hemisphere != 'N' || fabs(east - 2000000.0) > 1.0 ||
      fabs(north - 2000000.0) > 1.0)
    return 0;
  hemisphere = '?';
  if (Convert_UPS_To_USNG('S', 2000000.0, 2000000.0, 5, usng) != 0 ||
      Convert_USNG_To_UPS(usng, &hemisphere, &east, &north) != 0 ||
      hemisphere != 'S' || fabs(east - 2000000.0) > 1.0 ||
      fabs(north - 2000000.0) > 1.0)
    return 0;
  return 1;
}

static int malformed_zone(void)
{
  char mgrs[] = "123UDQ12", usng[] = "123UDQ12", hemisphere = '?';
  long zone = 99;
  double east = 0, north = 0;
  if ((Convert_MGRS_To_UTM(mgrs, &zone, &hemisphere, &east, &north) &
       MGRS_STRING_ERROR) == 0 || zone != 0)
    return 0;
  zone = 99;
  return (Convert_USNG_To_UTM(usng, &zone, &hemisphere, &east, &north) &
          USNG_STRING_ERROR) != 0 && zone == 0;
}

static int short_polar(void)
{
  char empty[] = "", one[] = "C", zero[] = "0", long_zone[] = "123";
  char hemisphere = '?';
  char *bad[] = {empty, one, zero, long_zone};
  double east = 0, north = 0;
  for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); ++i) {
    if ((Convert_MGRS_To_UPS(bad[i], &hemisphere, &east, &north) &
         MGRS_STRING_ERROR) == 0 ||
        (Convert_USNG_To_UPS(bad[i], &hemisphere, &east, &north) &
         USNG_STRING_ERROR) == 0)
      return 0;
  }
  return 1;
}

static int non_ascii_coordinate(void)
{
  char mgrs[] = {'3', '1', 'U', (char)0x80, 'Q', '1', '2', '\0'};
  char usng[] = {'3', '1', 'U', (char)0x80, 'Q', '1', '2', '\0'};
  double lat = 0, lon = 0;
  return (Convert_MGRS_To_Geodetic(mgrs, &lat, &lon) & MGRS_STRING_ERROR) != 0 &&
         (Convert_USNG_To_Geodetic(usng, &lat, &lon) & USNG_STRING_ERROR) != 0;
}

int main(int argc, char **argv)
{
  int ok = 0;
  if (argc != 2) return 2;
  if (strcmp(argv[1], "invalid-polar-mgrs") == 0) ok = invalid_polar_mgrs();
  else if (strcmp(argv[1], "invalid-polar-usng") == 0) ok = invalid_polar_usng();
  else if (strcmp(argv[1], "trailing-mgrs") == 0) ok = trailing_mgrs();
  else if (strcmp(argv[1], "trailing-usng") == 0) ok = trailing_usng();
  else if (strcmp(argv[1], "valid") == 0) ok = valid_coordinates();
  else if (strcmp(argv[1], "valid-polar") == 0) ok = valid_polar_coordinates();
  else if (strcmp(argv[1], "malformed-zone") == 0) ok = malformed_zone();
  else if (strcmp(argv[1], "short-polar") == 0) ok = short_polar();
  else if (strcmp(argv[1], "non-ascii") == 0) ok = non_ascii_coordinate();
  else return 2;
  if (!ok) fprintf(stderr, "geotranz input case failed: %s\n", argv[1]);
  return ok ? 0 : 1;
}
