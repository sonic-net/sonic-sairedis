#include "MACsecIngressFilter.h"

#include "swss/logger.h"

#include <unistd.h>
#include <string.h>

using namespace saivs;

MACsecIngressFilter::MACsecIngressFilter(
        _In_ const std::string &macsecInterfaceName) :
    MACsecFilter(macsecInterfaceName)
{
    SWSS_LOG_ENTER();

    // empty intentionally
}

TrafficFilter::FilterStatus MACsecIngressFilter::forward(
        _In_ const void *buffer,
        _In_ size_t length)
{
    SWSS_LOG_ENTER();

    // MACsec interface will automatically forward ingress MACsec traffic
    // by Linux Kernel.
    // So this filter just need to drop all ingress MACsec traffic directly

    return TrafficFilter::TERMINATE;
}
