/*
 * $Id: $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * Broadcom System Log Trace
 */

#ifndef _DIAG_BSLTRACE_H
#define _DIAG_BSLTRACE_H

int
bsltrace_init(void);

int
bsltrace_clear(void);

int
bsltrace_config_set(int nentry, int size);

int
bsltrace_config_get(int *nentry, int *size);

int
bsltrace_print(int entries);

#endif /* !_DIAG_BSLTRACE_H */
