/*
 * $Id: diag_mcs.h,v 1.0
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * File:        diag_mcs.h
 * Purpose:
 */

#ifndef   _DIAG_MCS_H_
#define   _DIAG_MCS_H_

#define MAX_UCS     3
typedef struct uKernel_info_s {
    char status[12];
    char fw_version[100];
    unsigned int cpsr;
    unsigned int type;
    unsigned int armReg[16];
} uKernel_info_t;

#endif /* _DIAG_MCS_H_ */
