/*******************************************************************************
 * Copyright 2016 Broadcom Corporation
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

/*MOD+************************************************************************
 * Module: HPE custom test header
 *
 * Copyright (C) 2016 Hewlett Packard Enterprise Development LP
 * All Rights Reserved.
 *
 * The contents of this software are proprietary and confidential
 * to the Hewlett Packard Enterprise Development LP. No part of
 * this program  may be photocopied, reproduced, or translated
 * into another programming language without prior written consent
 * of the Hewlett Packard Enterprise Development LP.
 *
 * Purpose: Declare functions and global structs used in hpe_custom_tests.c
 *
 **MOD-***********************************************************************/

#ifndef _HPE_CUSTOM_TESTS_H_
#define _HPE_CUSTOM_TESTS_H_

/* Function declarations */

int dune_mem_bist(int unit, int testType, int testArgs);
int dune_kbp_bist(int unit, int kbp_mdio_id, int core);

/* Macros */
#define TEST_NAME_STR_MAX_LEN 15

typedef enum {
    POST_MEM_ALLOC_FAILURE              = 0x00000001,
    POST_PARSE_STRING_FAILURE           = 0x00000002,
    POST_BIST_INIT_FAILURE              = 0x00000003,
    POST_INVALID_TEST_CONFIG            = 0x00000004,
    POST_SOFT_RESET_FAILURE             = 0x00000005,
    POST_DRAM_BIST_START_FAILURE        = 0x00000006,
    POST_DRAM_BIST_RESULT_READ_FAILURE  = 0x00000007,
    POST_DRAM_BIST_NZ_ERR_COUNT_FAILURE = 0x00000008,
    POST_KBP_DBA_WRITE_FAILURE          = 0x00000009,
    POST_KBP_DBA_COPY_FAILURE           = 0x0000000A,
    POST_KBP_DBA_READ_X_FAILURE         = 0x0000000B,
    POST_KBP_DBA_READ_Y_FAILURE         = 0x0000000C,
    POST_KBP_UDA_WRITE_FAILURE          = 0x0000000D,
    POST_KBP_UDA_READ_FAILURE           = 0x0000000E
} POST_RETURN_ERR_t;

#endif /* _HPE_CUSTOM_TESTS_H_ */
