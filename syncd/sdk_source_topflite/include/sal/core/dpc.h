/*
 * $Id: dpc.h,v 1.3 Broadcom SDK $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * File: 	dpc.h
 * Purpose: 	Deferred Procedure Call module
 */

#ifndef _SAL_DPC_H
#define	_SAL_DPC_H

#include <sal/core/time.h>
#include <sal/core/thread.h>

typedef void (*sal_dpc_fn_t)(void *owner, void *, void *, void *, void *);

extern int sal_dpc_init(void);
extern void sal_dpc_term(void);
extern int sal_dpc_config(int, int);

extern int sal_dpc(sal_dpc_fn_t, void *owner, void *, void *, void *, void *);
extern int sal_dpc_time(sal_usecs_t usec,
			sal_dpc_fn_t,
			void *owner, void *, void *, void *, void *);

extern void sal_dpc_cancel(void *owner);
extern int sal_dpc_enable(void *owner);
extern int sal_dpc_disable(void *owner);
extern int sal_dpc_disable_and_wait(void *owner);
extern void sal_dpc_cancel_and_disable(void *owner);

#endif	/* !_SAL_DPC_H */

