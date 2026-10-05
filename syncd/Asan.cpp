/*
 * ASAN support helpers:
 * 1. Install a SIGTERM handler that runs an injected LSan leak check.
 * 2. When /etc/sonic/inject_asan_test_leak_enabled exists, inject a known test
 *    leak used to verify the ASAN/LSan path is working as expected.
 *
 * ENABLE_ASAN=y syncd builds also link AsanCtor.cpp, whose constructor calls
 * asan_init_impl() before main(). Unit tests leave AsanCtor.cpp out and call
 * asan_init_impl() / asan_sigterm_handler_impl() with test-double functions for
 * the dependencies.
 */

#include "Asan.h"

#include "swss/logger.h"

#include <cstdlib>
#include <cstring>
#include <exception>
#include <thread>
#include <unistd.h>

/* ASAN test-leak injection
 *
 * When ASAN is enabled and /etc/sonic/inject_asan_test_leak_enabled exists,
 * allocate a block and deliberately never free it so LSan has a known leak to
 * report on process exit or in the SIGTERM handler. This is useful for
 * verifying that the ASAN build, configuration, and SIGTERM handlers are
 * working as expected.
 *
 * The memory block has to still look unreachable when the leak check runs,
 * which is difficult. LSan scans thread stacks conservatively, and at -O2 ASAN
 * moves the injector's locals into a "fake stack" frame that lives on the heap
 * for the lifetime of the thread. Overwriting the real stack never reaches
 * those copies, so a leak injected on the main thread stays reachable and is
 * silently dropped by __lsan_do_leak_check() in the SIGTERM handler. syncd is
 * long-running and is stopped via SIGTERM, so that is the path that matters.
 *
 * Injecting from a short-lived helper thread sidesteps that: once the thread is
 * joined, both its stack and its ASAN fake stack are gone, so no stale pointer
 * survives for LSan to trip over. This works at every optimization level and
 * needs no ASAN_OPTIONS tuning.
 *
 * The intentional leak is injected at startup so it is present when the leak
 * check runs in the SIGTERM handler. Do not call the LSan leak-check callback
 * here: that terminates the process when leaks are present; call it only from
 * the SIGTERM handler.
 */

// Set by asan_init_impl(); invoked from the SIGTERM handler.
static AsanLsanLeakCheckFn g_lsan_leak_check = nullptr;

__attribute__((noinline))
void asan_inject_test_leak(AsanMallocFn malloc_fn)
{
    SWSS_LOG_ENTER();

    void *probe = malloc_fn(ASAN_TEST_LEAK_SIZE);
    if (!probe)
    {
        SWSS_LOG_ERROR("failed to allocate %zu bytes for the ASAN test leak, no leak injected",
                       ASAN_TEST_LEAK_SIZE);
        return;
    }

    std::memset(probe, 0xCD, ASAN_TEST_LEAK_SIZE);

    // Feed the pointer to an opaque asm that also reads memory, so -O2 cannot
    // drop the malloc and memset as dead stores. Nothing stores the pointer, so
    // the block remains unreachable.
    __asm__ __volatile__("" : : "r"(probe) : "memory");
}

void asan_sigterm_handler_impl(int signo,
                               AsanLsanLeakCheckFn leak_check_fn,
                               AsanSignalFn signal_fn,
                               AsanRaiseFn raise_fn)
{
    SWSS_LOG_ENTER();

    if (leak_check_fn)
    {
        leak_check_fn();
    }

    signal_fn(signo, SIG_DFL);
    raise_fn(signo);
}

void asan_sigterm_handler(int signo)
{
    SWSS_LOG_ENTER();

    asan_sigterm_handler_impl(signo, g_lsan_leak_check, ::signal, ::raise);
}

bool asan_init_impl(AsanSignalFn signal_fn,
                    AsanAccessFn access_fn,
                    AsanMallocFn malloc_fn,
                    AsanLsanLeakCheckFn leak_check_fn)
{
    SWSS_LOG_ENTER();

    g_lsan_leak_check = leak_check_fn;

    if (signal_fn(SIGTERM, asan_sigterm_handler) == SIG_ERR)
    {
        SWSS_LOG_ERROR("failed to setup SIGTERM action");
        return false;
    }

    if (access_fn("/etc/sonic/inject_asan_test_leak_enabled", F_OK) == 0)
    {
        try
        {
            // See comment above asan_inject_test_leak() for why this must run
            // in a separate thread.
            std::thread(asan_inject_test_leak, malloc_fn).join();
        }
        catch (const std::exception& e)
        {
            SWSS_LOG_ERROR("failed to inject ASAN test leak: %s", e.what());
        }
    }

    return true;
}
