/**
 * SPDX-FileCopyrightText: NVIDIA CORPORATION & AFFILIATES
 * Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file    saiarscustom.h
 *
 * @brief   Custom ARS attributes for adaptive routing busy threshold
 *
 * @warning This module is a SAI custom module
 */

#ifndef __SAIARSCUSTOM_H_
#define __SAIARSCUSTOM_H_

#include <saiars.h>
#include <saitypes.h>

/**
 * @brief Custom ARS attributes for adaptive routing.
 *
 * Controls the busy_threshold field in the AR profile. When busy_threshold > 0
 * in PER_PACKET_QUALITY mode, adaptive ECMP containers are created as STATEFUL
 * (with flow tables) instead of STATELESS. Required for ARN generation.
 *
 * @flags free
 */
typedef enum _sai_ars_attr_custom_t
{
    /**
     * @brief AR profile busy threshold
     *
     * @type sai_uint32_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_ARS_ATTR_BUSY_THRESHOLD = SAI_ARS_ATTR_CUSTOM_RANGE_START,

} sai_ars_attr_custom_t;

#endif /* __SAIARSCUSTOM_H_ */
