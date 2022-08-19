/*
 * $Id: sdk_config.h,v 1.5 Broadcom SDK $
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 *
 */

#ifndef __SDK_CONFIG_H__
#define __SDK_CONFIG_H__

/*
 * Include custom overrides
 */
#ifdef SDK_INCLUDE_CUSTOM_CONFIG
#include <sdk_custom_config.h>
#endif


/*
 * Memory Barrier operation if required. 
 * Defaults to nothing. 
 */
#ifndef SDK_CONFIG_MEMORY_BARRIER
#define SDK_CONFIG_MEMORY_BARRIER
#endif



#endif /* __SDK_CONFIG_H__ */
