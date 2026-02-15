#include "swss/logger.h"

#include <csignal>
#include <sanitizer/lsan_interface.h>

extern "C" {
    const char* __lsan_default_suppressions() {
        // SWSS_LOG_ENTER(); // disabled
        return "leak:__static_initialization_and_destruction_0\n";
    }
}

static void sigterm_handler(int signo)
{
    SWSS_LOG_ENTER();

    __lsan_do_leak_check();
    signal(signo, SIG_DFL);
    raise(signo);
}

__attribute__((constructor))
static void asan_init()
{
    SWSS_LOG_ENTER();

    if (signal(SIGTERM, sigterm_handler) == SIG_ERR)
    {
        SWSS_LOG_ERROR("failed to setup SIGTERM action");
        exit(1);
    }
}
