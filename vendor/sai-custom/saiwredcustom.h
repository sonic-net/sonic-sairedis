/**
 * SPDX-FileCopyrightText: NVIDIA CORPORATION & AFFILIATES
 * Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file    saiwredcustom.h
 *
 * @brief   Custom WRED attributes for relative/percentage-based thresholds
 *
 * @warning This module is a SAI custom module
 */

#ifndef __SAIWREDCUSTOM_H_
#define __SAIWREDCUSTOM_H_

#include <saiwred.h>
#include <saitypes.h>

/**
 * @brief WRED threshold mode
 */
typedef enum _sai_wred_threshold_mode_t
{
    SAI_WRED_THRESHOLD_MODE_ABSOLUTE = 0,

    SAI_WRED_THRESHOLD_MODE_RELATIVE = 1,

} sai_wred_threshold_mode_t;

/**
 * @brief Custom WRED attributes for relative thresholds.
 *
 * When SAI_WRED_ATTR_THRESHOLD_MODE is RELATIVE, the *_RELATIVE threshold
 * attributes are used (uint8, 0-100 representing percentage of the port/TC
 * alpha quota). The standard absolute threshold attributes are ignored.
 *
 * @flags free
 */
typedef enum _sai_wred_attr_custom_t
{
    /**
     * @brief Threshold mode: absolute (bytes) or relative (percentage)
     *
     * When set to RELATIVE, the *_RELATIVE threshold attrs below are used.
     * Default is ABSOLUTE for backward compatibility.
     *
     * @type sai_wred_threshold_mode_t
     * @flags CREATE_AND_SET
     * @default SAI_WRED_THRESHOLD_MODE_ABSOLUTE
     */
    SAI_WRED_ATTR_THRESHOLD_MODE = SAI_WRED_ATTR_CUSTOM_RANGE_START,

    /**
     * @brief Green minimum threshold (relative, 0-100%)
     *
     * @type sai_uint8_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_WRED_ATTR_GREEN_MIN_THRESHOLD_RELATIVE,

    /**
     * @brief Green maximum threshold (relative, 0-100%)
     *
     * @type sai_uint8_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_WRED_ATTR_GREEN_MAX_THRESHOLD_RELATIVE,

    /**
     * @brief Yellow minimum threshold (relative, 0-100%)
     *
     * @type sai_uint8_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_WRED_ATTR_YELLOW_MIN_THRESHOLD_RELATIVE,

    /**
     * @brief Yellow maximum threshold (relative, 0-100%)
     *
     * @type sai_uint8_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_WRED_ATTR_YELLOW_MAX_THRESHOLD_RELATIVE,

    /**
     * @brief Red minimum threshold (relative, 0-100%)
     *
     * @type sai_uint8_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_WRED_ATTR_RED_MIN_THRESHOLD_RELATIVE,

    /**
     * @brief Red maximum threshold (relative, 0-100%)
     *
     * @type sai_uint8_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_WRED_ATTR_RED_MAX_THRESHOLD_RELATIVE,

    /**
     * @brief ECN green minimum threshold (relative, 0-100%)
     *
     * @type sai_uint8_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_WRED_ATTR_ECN_GREEN_MIN_THRESHOLD_RELATIVE,

    /**
     * @brief ECN green maximum threshold (relative, 0-100%)
     *
     * @type sai_uint8_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_WRED_ATTR_ECN_GREEN_MAX_THRESHOLD_RELATIVE,

    /**
     * @brief ECN yellow minimum threshold (relative, 0-100%)
     *
     * @type sai_uint8_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_WRED_ATTR_ECN_YELLOW_MIN_THRESHOLD_RELATIVE,

    /**
     * @brief ECN yellow maximum threshold (relative, 0-100%)
     *
     * @type sai_uint8_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_WRED_ATTR_ECN_YELLOW_MAX_THRESHOLD_RELATIVE,

    /**
     * @brief ECN red minimum threshold (relative, 0-100%)
     *
     * @type sai_uint8_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_WRED_ATTR_ECN_RED_MIN_THRESHOLD_RELATIVE,

    /**
     * @brief ECN red maximum threshold (relative, 0-100%)
     *
     * @type sai_uint8_t
     * @flags CREATE_AND_SET
     * @default 0
     */
    SAI_WRED_ATTR_ECN_RED_MAX_THRESHOLD_RELATIVE,

} sai_wred_attr_custom_t;

#endif /* __SAIWREDCUSTOM_H_ */
