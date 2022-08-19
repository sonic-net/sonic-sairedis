/*
 * $Id: progress.h,v 1.2 Broadcom SDK $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * Progress Report module: diag/progress.c
 */

#include <sal/types.h>

void progress_init(uint32 total_count, int start_seconds, int disable);
void progress_status(char *status_str);
void progress_report(uint32 count_incr);
void progress_done(void);
