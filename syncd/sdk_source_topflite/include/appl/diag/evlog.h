/*
 * $Id: evlog.h,v 1.1 Broadcom SDK $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * File:        evlog.h
 * Purpose:     
 */

#ifndef   _EVLOG_H_
#define   _EVLOG_H_

#include <shared/evlog.h>

#if defined(INCLUDE_SHARED_EVLOG)
extern shared_evlog_t * diag_evlog_create(int max_args, int max_entries);
extern void diag_evlog_destroy(shared_evlog_t *ev);
extern int diag_evlog_dump(shared_evlog_t *ev, int count, int oldest);
#endif

#endif /* _EVLOG_H_ */
