/*
 * $Id: spl.h,v 1.4 Broadcom SDK $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * File: 	spl.h
 * Purpose: 	Interrupt Blocking
 */

#ifndef _SAL_SPL_H
#define _SAL_SPL_H

extern int	sal_spl_init(void);
extern int	sal_spl(int level);
extern int	sal_splhi(void);

extern int      sal_int_locked(void);
extern int      sal_int_context(void);
extern int      sal_no_sleep(void);

#endif	/* !_SAL_SPL_H */
