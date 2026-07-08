// Harness-only LD_PRELOAD shim: route SAI VS library SWSS_LOG_* to stdout so
// run_test.sh capture in /var/log/sai-server.log works without pulling SONiC
// logger setup back into the SAI server binary.
#include <swss/logger.h>

namespace {

__attribute__((constructor))
static void route_swss_log_to_stdout()
{
    SWSS_LOG_ENTER();
    swss::Logger::setMinPrio(swss::Logger::SWSS_DEBUG);
    swss::Logger::swssOutputNotify("saiserver", "STDOUT");
}

} // namespace
