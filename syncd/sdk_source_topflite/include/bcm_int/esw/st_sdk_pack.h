/*
 * $Id$
 * $Copyright: (c) 2020 Broadcom.
 * Broadcom Proprietary and Confidential. All rights reserved.$
 *
 * File:    st_sdk_pack.h
 */

#ifndef ST_SDK_PACK_H_
#define ST_SDK_PACK_H_

#include <bcm_int/esw/st_sdk_msg.h>
#include <bcm_int/esw/st_feature.h>

/*
 * ST Initialization control message - pack
 */
uint8*
st_sdk_msg_ctrl_init_pack(uint8 *buf,
        st_sdk_msg_ctrl_init_t *msg);
/*  
 * Create PB collector - pack
 */ 
uint8*
st_sdk_msg_ctrl_collector_create_pack(uint8* buf,
                                      st_sdk_msg_ctrl_collector_create_t *msg);
#endif /* ST_SDK_PACK_H_ */
