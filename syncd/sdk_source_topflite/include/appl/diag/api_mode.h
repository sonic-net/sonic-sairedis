/* 
 * $Id: api_mode.h,v 1.1 Broadcom SDK $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * File:        api_mode.h
 * Purpose:     API Mode public interface
 */

#ifndef   _DIAG_API_MODE_H_
#define   _DIAG_API_MODE_H_

#include <appl/diag/shell.h>

#ifdef INCLUDE_APIMODE
extern cmd_result_t diag_api_mode_process_command(int u, char *s);
extern void diag_api_mode_completion_enable(int flag);
#endif

#endif /* _DIAG_API_MODE_H_ */
