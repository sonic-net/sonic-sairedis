/*******************************************************************************
 * Copyright 2014 Broadcom Corporation
 * This program is the proprietary software of Broadcom Corporation and/or its
 * licensors, and may only be used, duplicated, modified or distributed pursuant
 *to
 * the terms and conditions of a separate, written license agreement executed
 *between
 * you and Broadcom (an "Authorized License"). Except as set forth in an
 * Authorized License, Broadcom grants no license (express or implied), right to
 *use,
 * or waiver of any kind with respect to the Software, and Broadcom expressly
 *reserves
 * all rights in and to the Software and all intellectual property rights
 *therein.
 * IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU HAVE NO RIGHT TO USE THIS
 *SOFTWARE IN
 * ANY WAY, AND SHOULD IMMEDIATELY NOTIFY BROADCOM AND DISCONTINUE ALL USE OF
 *THE SOFTWARE.
 *
 * Except as expressly set forth in the Authorized License,
 * 1. This program, including its structure, sequence and organization,
 *constitutes the
 *    valuable trade secrets of Broadcom, and you shall use all reasonable
 *efforts to
 *    protect the confidentiality thereof, and to use this information only in
 *connection
 *    with your use of Broadcom integrated circuit products.
 * 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *AND
 *    WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
 *WARRANTIES,
 *    EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO THE
 *SOFTWARE.
 *    BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED WARRANTIES OF TITLE,
 *    MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK
 *OF VIRUSES,
 *    ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 *CORRESPONDENCE TO
 *    DESCRIPTION. YOU ASSUME THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE
 *OF THE SOFTWARE.
 * 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
 *LICENSORS
 *    BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT, OR
 *EXEMPLARY DAMAGES
 *    WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO YOUR USE OF OR
 *INABILITY TO USE
 *    THE SOFTWARE EVEN IF BROADCOM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 *DAMAGES;
 *    OR (ii) ANY AMOUNT IN EXCESS OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE
 *ITSELF OR
 *    U.S. $1, WHICHEVER IS GREATER. THESE LIMITATIONS SHALL APPLY
 *NOTWITHSTANDING ANY
 *    FAILURE OF ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
 *******************************************************************************/

#ifndef _BCM_KBP_INITIALIZE_H
#define _BCM_KBP_INITIALIZE_H

/* include header */
#include <stdint.h>

/* constant */
//#define SERDES_SPEED_1_25G    (4U)
//#define SERDES_SPEED_3_125G   (1U)
#define SERDES_SPEED_6_25G (2U)
#define SERDES_SPEED_10_3125G (3U)
#define SERDES_SPEED_12_5G (0U)

/* flag */
#ifndef INITIALIZE_SERDES_EYE_SCAN
#define INITIALIZE_SERDES_EYE_SCAN (1U)
#endif

/* function */
uint32_t bcm_kbp_assert_reset_pins(uint8_t srst_l, uint8_t crst_l);
uint32_t bcm_kbp_configure_static_pins(void);
uint32_t bcm_kbp_serdes_soft_reset(int unit, uint32_t kbp_mdio_id,
                                   uint8_t  kbp_select_host,
                                   uint8_t  kbp_select_cascade,
                                   uint32_t rx_lane_select,
                                   uint32_t tx_lane_select);
uint32_t bcm_kbp_initialize(int unit, uint32_t kbp_mdio_id,
                            uint8_t kbp_select_host, uint8_t kbp_select_cascade,
                            uint32_t rx_lane_select, uint32_t tx_lane_select,
                            uint8_t  serdes_speed,
                            uint8_t  tx_serdes_reference_quad_index,
                            uint16_t metaframe_length, uint8_t port_select);
uint32_t bcm_kbp_system_reset(int unit, uint32_t kbp_mdio_id,
                              uint8_t  kbp_select_host,
                              uint8_t  kbp_select_cascade,
                              uint32_t rx_lane_select, uint32_t tx_lane_select,
                              uint8_t  serdes_speed,
                              uint8_t  tx_serdes_reference_quad_index,
                              uint16_t metaframe_length, uint8_t port_select);
uint32_t bcm_kbp_core_reset(uint8_t kbp_select_host,
                            uint8_t kbp_select_cascade);
uint32_t bcm_kbp_check_link_status(int unit, uint32_t kbp_mdio_id,
                                   uint8_t kbp_select_host,
                                   uint8_t kbp_select_cascade,
                                   uint8_t port_select);
uint32_t bcm_kbp_initialize_clear_error(
    int unit, uint32_t kbp_mdio_id, uint8_t kbp_select_host,
    uint8_t kbp_select_cascade, uint8_t port_select,
    void (*kbp_register_write)(uint8_t, uint32_t, uint16_t*, uint8_t));
uint32_t bcm_kbp_check_core_status(int unit, uint32_t kbp_mdio_id,
                                   uint8_t kbp_select_host,
                                   uint8_t kbp_select_cascade);
uint32_t bcm_kbp_static_and_slow_speed_pins_initialize(void);
uint32_t bcm_kbp_static_and_slow_speed_pins_terminate(void);

#endif /* _BCM_KBP_INITIALIZE_H */
