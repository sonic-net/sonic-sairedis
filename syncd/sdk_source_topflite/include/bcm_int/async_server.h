/*
 * $Id: async_server.h,v 1.3 Broadcom SDK $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * Asynchronous BCM API Server
 */

#ifndef	_BCM_INT_ASYNC_SERVER_H
#define	_BCM_INT_ASYNC_SERVER_H

#ifdef	BCM_ASYNC_SUPPORT

#include <bcm_int/async_req.h>

extern void	bcm_async_add(bcm_async_req_t *req);
extern int	bcm_async_start(void);
extern int	bcm_async_stop(void);
extern void	bcm_async_run(bcm_async_req_t *req);

#endif	/* BCM_ASYNC_SUPPORT */
#endif	/* _BCM_INT_ASYNC_SERVER_H */
