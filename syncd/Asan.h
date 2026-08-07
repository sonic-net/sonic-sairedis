/*
 * SPDX-FileCopyrightText: NVIDIA CORPORATION & AFFILIATES
 * Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Testable ASAN helpers for syncd. Production builds enable them via the
 * constructor in AsanCtor.cpp; unit tests call asan_init_impl() /
 * asan_sigterm_handler_impl() with injected dependencies and leave
 * AsanCtor.cpp out of the link.
 */

#pragma once

#include <csignal>
#include <cstddef>

// Unique size to identify the intentional ASAN test-leak allocation in reports.
static constexpr size_t ASAN_TEST_LEAK_SIZE = 9861842;

// Function pointers for real implementations or test doubles.
// signal() from csignal
using AsanSignalFn = sighandler_t (*)(int, sighandler_t);
// access() from unistd.h
using AsanAccessFn = int (*)(const char *, int);
// malloc() from cstdlib
using AsanMallocFn = void *(*)(size_t);
// __lsan_do_leak_check() from sanitizer/lsan_interface.h
using AsanLsanLeakCheckFn = void (*)(void);
// raise() from csignal
using AsanRaiseFn = int (*)(int);

// SIGTERM handler installed by asan_init_impl(). Thin wrapper around
// asan_sigterm_handler_impl() that passes g_lsan_leak_check and the real
// libc entry points. Exposed so tests can verify the handler pointer that was
// passed to signal().
void asan_sigterm_handler(int signo);

// Testable SIGTERM-handler body. Production wrapper passes g_lsan_leak_check,
// ::signal, and ::raise; unit tests inject doubles.
void asan_sigterm_handler_impl(int signo,
                               AsanLsanLeakCheckFn leak_check_fn,
                               AsanSignalFn signal_fn,
                               AsanRaiseFn raise_fn);

// Allocate (and never free) the intentional test leak via malloc_fn.
void asan_inject_test_leak(AsanMallocFn malloc_fn);

// Set up ASAN helpers:
// - Installs a SIGTERM handler that runs leak_check_fn.
// - When /etc/sonic/inject_asan_test_leak_enabled exists, injects a known test
//   leak via malloc_fn.
// - Returns false if signal-handler installation fails; true otherwise
//   (including when leak injection is skipped or malloc_fn returns nullptr).
bool asan_init_impl(AsanSignalFn signal_fn,
                    AsanAccessFn access_fn,
                    AsanMallocFn malloc_fn,
                    AsanLsanLeakCheckFn leak_check_fn);
