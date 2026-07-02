// Harness-only LD_PRELOAD shim: route libsaivs SWSS_LOG_* to stdout so
// run_test.sh capture in /var/log/saiserver.log works without pulling SONiC
// logger setup back into saiserver.cpp (see SAI saithriftv2 saiserver change).
#include <swss/logger.h>

namespace {

__attribute__((constructor))
static void route_swss_log_to_stdout()
{
    swss::Logger::setMinPrio(swss::Logger::SWSS_DEBUG);
    swss::Logger::swssOutputNotify("saiserver", "STDOUT");
}

} // namespace
