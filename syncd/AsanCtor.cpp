/*
 * SPDX-FileCopyrightText: NVIDIA CORPORATION & AFFILIATES
 * Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * ASAN process bootstrap. Adds code for ASAN builds, including:
 * - a constructor that wires asan_init_impl() into process initialization
 *   before main().
 * - default LSan suppressions
 *
 * Include this file only in ASAN builds where you want that functionality
 * enabled (ENABLE_ASAN=y syncd). To run unit tests against the implementation
 * in Asan.cpp in a non-ASAN build without installing a SIGTERM handler,
 * injecting a test leak, or pulling in sanitizer symbols, leave this file out
 * of the build and call asan_init_impl() with test doubles for the
 * dependencies.
 */

#include "Asan.h"

#include "swss/logger.h"

#include <cstdlib>
#include <sanitizer/lsan_interface.h>
#include <unistd.h>

extern "C" {
    const char* __lsan_default_suppressions() {
        // SWSS_LOG_ENTER(); // disabled
        return "leak:__static_initialization_and_destruction_0\n";
    }
}

__attribute__((constructor))
static void asan_init()
{
    SWSS_LOG_ENTER();

    if (!asan_init_impl(::signal, ::access, std::malloc, __lsan_do_leak_check))
    {
        exit(EXIT_FAILURE);
    }
}
