/*
 * $Id: ukernel_debug.h,v 0.1 2014/10/11 samaresm Exp $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * File:    ukernel_debug.h
 * Purpose:  
 */

#ifndef _UKERNEL_DEBUG_H
#define _UKERNEL_DEBUG_H

#if defined(SOC_UKERNEL_DEBUG)

#include <soc/defs.h>

#if defined(BCM_CMICM_SUPPORT) || defined(BCM_IPROC_SUPPORT)

#include <appl/diag/shell.h>

extern cmd_result_t cmd_cmic_ucdebug(int unit, args_t *a);
#else
#define cmd_cmic_ucdebug(unit, a)
#endif
#else
#define cmd_cmic_ucdebug(unit, a)
#endif
#endif /*_UKERNEL_DEBUG_H*/
