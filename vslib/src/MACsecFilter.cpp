#include "MACsecFilter.h"

#include <swss/logger.h>
#include <swss/select.h>

#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <net/if.h>

using namespace saivs;

#define EAPOL_ETHER_TYPE (0x888e)

MACsecFilter::MACsecFilter(
    _In_ const std::string &macsec_interface_name,
    _In_ int macsecfd):
    m_macsecfd(macsecfd),
    m_macsec_interface_name(macsec_interface_name)
{
    SWSS_LOG_ENTER();

    // empty intentionally
}

TrafficFilter::FilterStatus MACsecFilter::execute(
    _Inout_ void *buffer,
    _Inout_ ssize_t &length)
{
    SWSS_LOG_ENTER();

    auto mac_hdr = static_cast<const ethhdr *>(buffer);

    if (ntohs(mac_hdr->h_proto) == EAPOL_ETHER_TYPE)
    {
        // EAPOL traffic bypass macsec interface
        // and directly forward to tap interface
        return TrafficFilter::CONTINUE;
    }

    return forward(buffer, length);
}
