#include "MACsecFilter.h"

#include "swss/logger.h"
#include "swss/select.h"

#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <net/if.h>

using namespace saivs;

#define EAPOL_ETHER_TYPE (0x888e)

MACsecFilter::MACsecFilter(
    _In_ const std::string &macsecInterfaceName):
    m_macsecDeviceEnable(false),
    m_macsecfd(0),
    m_macsecInterfaceName(macsecInterfaceName)
{
    SWSS_LOG_ENTER();

    // empty intentionally
}

void MACsecFilter::enable_macsec_device(
    _In_ bool enable)
{
    SWSS_LOG_ENTER();

    m_macsecDeviceEnable = enable;
}

void MACsecFilter::set_macsec_fd(
    _In_ int macsecfd)
{
    SWSS_LOG_ENTER();

    m_macsecfd = macsecfd;
}

TrafficFilter::FilterStatus MACsecFilter::execute(
    _Inout_ void *buffer,
    _Inout_ size_t &length)
{
    SWSS_LOG_ENTER();

    auto mac_hdr = static_cast<const ethhdr *>(buffer);

    if (ntohs(mac_hdr->h_proto) == EAPOL_ETHER_TYPE)
    {
        // EAPOL traffic will never be delivered to MACsec device
        return TrafficFilter::CONTINUE;
    }

    if (m_macsecDeviceEnable)
    {
        return forward(buffer, length);
    }

    // Drop all non-EAPOL packets if macsec device haven't been enable.
    return TrafficFilter::TERMINATE;
}
