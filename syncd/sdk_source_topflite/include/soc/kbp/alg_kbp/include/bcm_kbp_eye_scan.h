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

#ifndef _BCM_KBP_EYE_SCAN_H
#define _BCM_KBP_EYE_SCAN_H

/* include header */
#include <stdint.h>

/* constant */
#define KBP_COUNT_MAX (4U)
#define KBP_SERDES_SET_COUNT_MAX (2U)
#define KBP_SERDES_LANE_COUNT_MAX (24U)

#define ARRAY_HOST_INDEX (0U)
#define ARRAY_CASCADE_INDEX (1U)

#define VERTICAL_MAX (15U)
#define VERTICAL_MIN (0U)
#define VERTICAL_RESOLUTION_MAX (VERTICAL_MAX - VERTICAL_MIN + 1)
#define HORIZONTAL_MAX (63)
#define HORIZONTAL_MIN (-63)
#define HORIZONTAL_RESOLUTION_MAX (HORIZONTAL_MAX - HORIZONTAL_MIN + 1)

#define LANE_COUNT_PER_QUAD (4U)

#define PRBS_TYPE_31 (0x0)
#define PRBS_TYPE_23 (0x1)
#define PRBS_TYPE_7 (0x2)

/* flag */
#ifndef PRINT_UNICODE_CHARACTER
#define PRINT_UNICODE_CHARACTER (1U)
#endif

/* function */
uint64_t bcm_kbp_serdes_eye_scan_initilize(int unit, uint32_t kbp_mdio_id,
                                           uint8_t kbp_select_host,
                                           uint8_t kbp_select_cascade);
uint64_t bcm_kbp_serdes_eye_scan_prbs(
    int unit, uint32_t kbp_mdio_id, uint8_t kbp_select_host,
    uint8_t kbp_select_cascade, uint32_t rx_lane_select,
    uint16_t vertical_sample_count, uint16_t horizontal_sample_count,
    uint64_t margin_count[KBP_COUNT_MAX][KBP_SERDES_SET_COUNT_MAX]
                         [KBP_SERDES_LANE_COUNT_MAX][VERTICAL_RESOLUTION_MAX]
                         [HORIZONTAL_RESOLUTION_MAX],
    uint8_t prbs_type);
uint64_t bcm_kbp_serdes_eye_scan_prbs_polarity_inversion(
    int unit, uint32_t kbp_mdio_id, uint8_t kbp_select_host,
    uint8_t kbp_select_cascade, uint32_t rx_lane_select,
    uint16_t vertical_sample_count, uint16_t horizontal_sample_count,
    uint64_t margin_count[KBP_COUNT_MAX][KBP_SERDES_SET_COUNT_MAX]
                         [KBP_SERDES_LANE_COUNT_MAX][VERTICAL_RESOLUTION_MAX]
                         [HORIZONTAL_RESOLUTION_MAX],
    uint8_t  prbs_type,
    uint32_t rx_polarity_inversion_lane_select[KBP_COUNT_MAX]
                                              [KBP_SERDES_SET_COUNT_MAX]);
uint64_t bcm_kbp_serdes_eye_scan_print_eyes(
    int unit, uint8_t kbp_select_host, uint8_t kbp_select_cascade,
    uint32_t rx_lane_select, uint16_t vertical_sample_count,
    uint16_t horizontal_sample_count,
    uint64_t margin_count[KBP_COUNT_MAX][KBP_SERDES_SET_COUNT_MAX]
                         [KBP_SERDES_LANE_COUNT_MAX][VERTICAL_RESOLUTION_MAX]
                         [HORIZONTAL_RESOLUTION_MAX]);
uint64_t bcm_kbp_serdes_eye_scan_find_eyes_dimension(
    uint8_t kbp_select_host, uint8_t kbp_select_cascade,
    uint32_t rx_lane_select, uint16_t vertical_sample_count,
    uint16_t horizontal_sample_count,
    uint64_t margin_count[KBP_COUNT_MAX][KBP_SERDES_SET_COUNT_MAX]
                         [KBP_SERDES_LANE_COUNT_MAX][VERTICAL_RESOLUTION_MAX]
                         [HORIZONTAL_RESOLUTION_MAX],
    uint16_t eye_height[KBP_COUNT_MAX][KBP_SERDES_SET_COUNT_MAX]
                       [KBP_SERDES_LANE_COUNT_MAX],
    uint16_t eye_width[KBP_COUNT_MAX][KBP_SERDES_SET_COUNT_MAX]
                      [KBP_SERDES_LANE_COUNT_MAX],
    int16_t eye_height_offset[KBP_COUNT_MAX][KBP_SERDES_SET_COUNT_MAX]
                             [KBP_SERDES_LANE_COUNT_MAX],
    int16_t eye_width_offset[KBP_COUNT_MAX][KBP_SERDES_SET_COUNT_MAX]
                            [KBP_SERDES_LANE_COUNT_MAX]);
uint64_t bcm_kbp_serdes_eye_scan_print_eyes_dimension(
    uint8_t kbp_select_host, uint8_t kbp_select_cascade,
    uint32_t rx_lane_select, uint16_t vertical_sample_count,
    uint16_t horizontal_sample_count,
    uint64_t margin_count[KBP_COUNT_MAX][KBP_SERDES_SET_COUNT_MAX]
                         [KBP_SERDES_LANE_COUNT_MAX][VERTICAL_RESOLUTION_MAX]
                         [HORIZONTAL_RESOLUTION_MAX],
    uint8_t precision);

#endif /* _BCM_KBP_EYE_SCAN_H */
