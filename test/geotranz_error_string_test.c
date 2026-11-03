// SPDX-License-Identifier: GPL-2.0-or-later

#include <assert.h>
#include <string.h>

#include "../external/geotranz/error_string.h"
#include "../external/geotranz/utm.h"
#include "../external/geotranz/mgrs.h"
#include "../external/geotranz/usng.h"

int main (void)
{
  struct {
    char output[8];
    char guard[4];
  } bounded;
  char complete[256];

  memset (&bounded, 'G', sizeof(bounded));
  utm_error_string (UTM_LAT_ERROR | UTM_LON_ERROR, bounded.output, sizeof(bounded.output));
  assert (bounded.output[sizeof(bounded.output) - 1] == 0);
  assert (memcmp (bounded.guard, "GGGG", sizeof(bounded.guard)) == 0);

  memset (&bounded, 'G', sizeof(bounded));
  mgrs_error_string (MGRS_LAT_ERROR | MGRS_LON_ERROR, bounded.output, sizeof(bounded.output));
  assert (bounded.output[sizeof(bounded.output) - 1] == 0);
  assert (memcmp (bounded.guard, "GGGG", sizeof(bounded.guard)) == 0);

  memset (&bounded, 'G', sizeof(bounded));
  usng_error_string (USNG_LAT_ERROR | USNG_LON_ERROR, bounded.output, sizeof(bounded.output));
  assert (bounded.output[sizeof(bounded.output) - 1] == 0);
  assert (memcmp (bounded.guard, "GGGG", sizeof(bounded.guard)) == 0);

  utm_error_string (UTM_LAT_ERROR | UTM_LON_ERROR, complete, sizeof(complete));
  assert (strstr (complete, "Latitude outside") != NULL);
  assert (strstr (complete, "\nLongitude outside") != NULL);

  return 0;
}
