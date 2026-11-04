/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef DW_PRIVATE_OUTPUT_H
#define DW_PRIVATE_OUTPUT_H
#include <stdio.h>
typedef enum { DW_OUTPUT_TEXT, DW_OUTPUT_BINARY } dw_output_format_t;
typedef enum { DW_OUTPUT_REPLACE, DW_OUTPUT_NEW } dw_output_policy_t;
/* New files are owner-private; an explicitly selected existing regular output
 * retains its permissions/ACL. Replacement truncates only after object checks.
 * New-file creation fails closed when the filesystem cannot enforce private
 * permissions (including Windows volumes without persistent ACL support).
 * The caller owns the returned stream. Parent directories must be trusted. */
FILE *dw_open_output(const char *path, dw_output_format_t format, dw_output_policy_t policy);
#endif
