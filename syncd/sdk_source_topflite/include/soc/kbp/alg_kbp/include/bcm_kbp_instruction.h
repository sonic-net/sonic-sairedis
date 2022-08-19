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

#ifndef _BCM_KBP_INSTRUCTION_H
#define _BCM_KBP_INSTRUCTION_H

/* Includes*/
#include <stdint.h>

/* constant */
#define REGISTER_DATA_ARRAY_ELEMENT_SIZE (16U)
#define REGISTER_80_BIT_DATA_ARRAY_LENGTH     \
    (80U / REGISTER_DATA_ARRAY_ELEMENT_SIZE + \
     (80U % REGISTER_DATA_ARRAY_ELEMENT_SIZE > 0))
#define REGISTER_32_BIT_DATA_ARRAY_LENGTH     \
    (32U / REGISTER_DATA_ARRAY_ELEMENT_SIZE + \
     (32U % REGISTER_DATA_ARRAY_ELEMENT_SIZE > 0))
#define MAX_KBP_COUNT (4U)

/* Functions */
#define bcm_kbp_get_80_bit_array_index(bit_index) \
    ((REGISTER_80_BIT_DATA_ARRAY_LENGTH)-1 -      \
     ((bit_index) / (REGISTER_DATA_ARRAY_ELEMENT_SIZE)))
#define bcm_kbp_get_32_bit_array_index(bit_index) \
    ((REGISTER_32_BIT_DATA_ARRAY_LENGTH)-1 -      \
     ((bit_index) / (REGISTER_DATA_ARRAY_ELEMENT_SIZE)))
#define bcm_kbp_get_array_element_bit_offset(bit_index) \
    ((bit_index) % (REGISTER_DATA_ARRAY_ELEMENT_SIZE))
#define bcm_kbp_printf_expand_80_bit_array(data_array, start_index)   \
    data_array[(start_index) + 0], data_array[(start_index) + 1],     \
        data_array[(start_index) + 2], data_array[(start_index) + 3], \
        data_array[(start_index) + 4]

/* Instruction functionality */
uint32_t bcm_kbp_instruction_register_write(int unit, int core,
                                            uint8_t device_id, uint32_t addr,
                                            uint16_t* data);
uint32_t bcm_kbp_instruction_register_read(int unit, int core,
                                           uint8_t device_id, uint32_t addr,
                                           uint16_t* data,
                                           uint8_t*  parity_error);
uint32_t bcm_kbp_instruction_database_write_xy(int unit, int core,
                                               uint8_t device_id, uint32_t addr,
                                               uint8_t vbit, uint16_t* x,
                                               uint16_t* y);
uint32_t bcm_kbp_instruction_database_read_x(int unit, int core,
                                             uint8_t device_id, uint32_t addr,
                                             uint8_t* vbit, uint8_t* parity,
                                             uint16_t* x,
                                             uint8_t*  parity_error);
uint32_t bcm_kbp_instruction_database_read_y(int unit, int core,
                                             uint8_t device_id, uint32_t addr,
                                             uint8_t* parity, uint16_t* y,
                                             uint8_t* parity_error);
uint32_t bcm_kbp_instruction_user_data_array_write(int unit, int core,
                                                   uint8_t  device_id,
                                                   uint32_t addr,
                                                   uint32_t data);
uint32_t bcm_kbp_instruction_user_data_array_read(int unit, int core,
                                                  uint8_t  device_id,
                                                  uint32_t addr, uint32_t* data,
                                                  uint8_t* ecc_error);
uint32_t bcm_kbp_instruction_database_block_copy(
    int unit, int core, uint8_t device_id, uint16_t addr_count,
    uint8_t direction, uint32_t src_addr, uint32_t dst_addr);
uint32_t bcm_kbp_instruction_database_block_clear(int unit, int core,
                                                  uint8_t  device_id,
                                                  uint16_t addr_count,
                                                  uint32_t addr);

/* MDIO functionality */
uint32_t bcm_kbp_mdio_register_write_16(int unit, uint32_t kbp_mdio_id,
                                        uint8_t kbp_select, uint8_t device_id,
                                        uint16_t addr, uint16_t data);
uint32_t bcm_kbp_mdio_register_read_16(int unit, uint32_t kbp_mdio_id,
                                       uint8_t kbp_select, uint8_t device_id,
                                       uint16_t addr, uint16_t* data);
uint32_t bcm_kbp_mdio_register_write_32(int unit, uint32_t kbp_mdio_id,
                                        uint8_t kbp_select, uint8_t device_id,
                                        uint16_t addr, uint16_t* data);
uint32_t bcm_kbp_mdio_register_read_32(int unit, uint32_t kbp_mdio_id,
                                       uint8_t kbp_select, uint8_t device_id,
                                       uint16_t addr, uint16_t* data);
uint32_t bcm_kbp_mdio_register_write_80(int unit, uint32_t kbp_mdio_id,
                                        uint8_t kbp_select, uint8_t device_id,
                                        uint16_t addr, uint16_t* data);
uint32_t bcm_kbp_mdio_register_read_80(int unit, uint32_t kbp_mdio_id,
                                       uint8_t kbp_select, uint8_t device_id,
                                       uint16_t addr, uint16_t* data);

#endif /* _BCM_KBP_INSTRUCTION_H */
