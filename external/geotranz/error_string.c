
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "utm.h"
#include "mgrs.h"
#include "usng.h"

#include "error_string.h"


// Convert error codes to text.
// Note that the code is a bit mask so it is possible to have multiple messages.
// Caller supplies the output capacity, including the terminating NUL.

struct error_entry {
	long mask;
	const char *msg;
};

static const struct error_entry utm_err [] = {
	
	{ UTM_NO_ERROR,           "No errors occurred in function" },
	{ UTM_LAT_ERROR,          "Latitude outside of valid range (-80.5 to 84.5 degrees)" },
	{ UTM_LON_ERROR,          "Longitude outside of valid range (-180 to 360 degrees)" },
	{ UTM_EASTING_ERROR,      "Easting outside of valid range (100,000 to 900,000 meters)" },
	{ UTM_NORTHING_ERROR,     "Northing outside of valid range (0 to 10,000,000 meters)" },
	{ UTM_ZONE_ERROR,         "Zone outside of valid range (1 to 60)" },
	{ UTM_HEMISPHERE_ERROR,   "Invalid hemisphere ('N' or 'S')" },
	{ UTM_ZONE_OVERRIDE_ERROR,"Zone outside of valid range (1 to 60) and within 1 of 'natural' zone" },
	{ UTM_A_ERROR,            "Semi-major axis less than or equal to zero" },
 	{ UTM_INV_F_ERROR,        "Inverse flattening outside of valid range (250 to 350)" },
	{ 0, NULL } };


static const struct error_entry mgrs_err [] = {
	
	{ MGRS_NO_ERROR,          "No errors occurred in function" },
	{ MGRS_LAT_ERROR,         "Latitude outside of valid range (-90 to 90 degrees)" },
	{ MGRS_LON_ERROR,         "Longitude outside of valid range (-180 to 360 degrees)" },
	{ MGRS_STRING_ERROR,      "An MGRS string error: string too long, too short, or badly formed" },
	{ MGRS_PRECISION_ERROR,   "The precision must be between 0 and 5 inclusive." },
	{ MGRS_A_ERROR,           "Inverse flattening outside of valid range (250 to 350)" },
	{ MGRS_INV_F_ERROR,       "Invalid hemisphere ('N' or 'S')" },
	{ MGRS_EASTING_ERROR,     "Easting outside of valid range (100,000 to 900,000 meters for UTM) (0 to 4,000,000 meters for UPS)" },
	{ MGRS_NORTHING_ERROR,    "Northing outside of valid range (0 to 10,000,000 meters for UTM) (0 to 4,000,000 meters for UPS)" },
 	{ MGRS_ZONE_ERROR,        "Zone outside of valid range (1 to 60)" },
 	{ MGRS_HEMISPHERE_ERROR,  "Invalid hemisphere ('N' or 'S')" },
 	{ MGRS_LAT_WARNING,       "Latitude warning ???" },
	{ 0, NULL } };


static const struct error_entry usng_err [] = {
	
	{ USNG_NO_ERROR,          "No errors occurred in function" },
	{ USNG_LAT_ERROR,         "Latitude outside of valid range (-90 to 90 degrees)" },
	{ USNG_LON_ERROR,         "Longitude outside of valid range (-180 to 360 degrees)" },
	{ USNG_STRING_ERROR,      "A USNG string error: string too long, too short, or badly formed" },
	{ USNG_PRECISION_ERROR,   "The precision must be between 0 and 5 inclusive." },
	{ USNG_A_ERROR,           "Inverse flattening outside of valid range (250 to 350)" },
	{ USNG_INV_F_ERROR,       "Invalid hemisphere ('N' or 'S')" },
	{ USNG_EASTING_ERROR,     "Easting outside of valid range (100,000 to 900,000 meters for UTM) (0 to 4,000,000 meters for UPS)" },
	{ USNG_NORTHING_ERROR,    "Northing outside of valid range (0 to 10,000,000 meters for UTM) (0 to 4,000,000 meters for UPS)" },
 	{ USNG_ZONE_ERROR,        "Zone outside of valid range (1 to 60)" },
 	{ USNG_HEMISPHERE_ERROR,  "Invalid hemisphere ('N' or 'S')" },
 	{ USNG_LAT_WARNING,       "Latitude warning ???" },
	{ 0, NULL } };



static void format_errors (const struct error_entry *entries, long err, char *str, size_t str_size)
{
	int n;
	size_t used = 0;
	int found = 0;

	if (str_size == 0) return;
	str[0] = '\0';

	for (n = 1; entries[n].mask != 0; n++) {
	  if (err & entries[n].mask) {
	    int written;
	    size_t remaining = str_size - used;
	    found = 1;
	    written = snprintf (str + used, remaining, "%s%s", used ? "\n" : "", entries[n].msg);
	    if (written < 0 || (size_t)written >= remaining) {
	      str[str_size - 1] = '\0';
	      return;
	    }
	    used += (size_t)written;
	  }
	}

	if (!found) {
	  snprintf (str, str_size, "%s", entries[0].msg);
	}
}

void utm_error_string (long err, char *str, size_t str_size)
{
	format_errors (utm_err, err, str, str_size);
}

void mgrs_error_string (long err, char *str, size_t str_size)
{
	format_errors (mgrs_err, err, str, str_size);
}

void usng_error_string (long err, char *str, size_t str_size)
{
	format_errors (usng_err, err, str, str_size);
}


