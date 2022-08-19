/* 
 * $Id: arefHelper.h,v 1.3 Broadcom SDK $
 *
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 */
#include <shared/bsl.h>
#include <bcm/error.h>

#ifndef AREFHELPER_H
#define AREFHELPER_H


#define APICALLCHK(cmd, arg) { int rv; if ( (rv = (cmd arg)) < 0 ) { \
                               bsl_printf("Error %d : %s %s at file %s line %d\n", rv, #cmd, bcm_errmsg(rv), __FILE__, __LINE__); \
                               return rv; } }

#define APICALLCHK_NR(cmd, arg) { int rv; if ( (rv = (cmd arg)) < 0 ) { \
                               bsl_printf("Error %d : %s at file %s line %d\n", rv, #cmd,__FILE__, __LINE__); return; } }



#endif
