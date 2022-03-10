#include "swss/logger.h"

#if defined(ASAN_ENABLED)
#include <csignal>
#include <sanitizer/lsan_interface.h>

void sigterm_handler(int signo)
{
    SWSS_LOG_ENTER();

    __lsan_do_leak_check();
    signal(signo, SIG_DFL);
    raise(signo);
}
#endif

int syncd_main(int argc, char **argv);

int main(int argc, char **argv)
{
    SWSS_LOG_ENTER();

#if defined(ASAN_ENABLED)
    if (signal(SIGTERM, sigterm_handler) == SIG_ERR)
    {
        SWSS_LOG_ERROR("failed to setup SIGTERM action");
        exit(1);
    }
#endif

    return syncd_main(argc, argv);
}
