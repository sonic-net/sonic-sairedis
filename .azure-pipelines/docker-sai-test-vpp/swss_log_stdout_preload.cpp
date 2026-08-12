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
    // STDERR keeps SWSS_LOG_ENTER/EXIT out of swss::exec() captured stdout (e.g.
    // find_new_bond_id's ip|awk pipeline). run_test.sh redirects 2>&1 to SAISERVER_LOG.
    swss::Logger::swssOutputNotify("saiserver", "STDERR");
}

} // namespace
