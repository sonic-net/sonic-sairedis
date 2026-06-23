/**
 * SPDX-FileCopyrightText: NVIDIA CORPORATION & AFFILIATES
 * Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @file    saicustom.h
 *
 * @brief   Upscale vendor custom SAI headers
 *
 * @warning This module is a SAI custom module
 */

#ifndef __SAICUSTOM_H_
#define __SAICUSTOM_H_

#include <sai.h>
#include <saitypes.h>

#include "saiwredcustom.h"

/**
 * @brief Custom SAI APIs placeholder
 *
 * @flags free
 */
typedef enum _sai_api_custom_t
{
    SAI_API_CUSTOM_RANGE_START = SAI_API_CUSTOM_RANGE_BASE,

    SAI_API_CUSTOM_RANGE_END

} sai_api_custom_t;

#endif /* __SAICUSTOM_H_ */
