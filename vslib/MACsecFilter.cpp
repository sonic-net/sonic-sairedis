#include "MACsecFilter.h"
#include "MACsecFilterStateGuard.h"

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
    m_macsecInterfaceName(macsecInterfaceName),
    m_state(MACsecFilter::MACsecFilterState::MACSEC_FILTER_STATE_IDLE)
{
    SWSS_LOG_ENTER();

    // empty intentionally
}

void MACsecFilter::enable_macsec_device(
        _In_ bool enable)
{
    SWSS_LOG_ENTER();

    m_macsecDeviceEnable = enable;

    // The function, execute(), may be running in another thread,
    // the macsec device enable state cannot be changed in the busy state.
    // Because if the macsec device was deleted in the busy state,
    // the error value of function, forward, will be returned
    // to the caller of execute()
    // and the caller thread will exit due to the error return value.
    while(get_state() == MACsecFilter::MACsecFilterState::MACSEC_FILTER_STATE_BUSY)
    {
        // Waiting the MACsec filter exit from the BUSY state
    }
}

void MACsecFilter::set_macsec_fd(
        _In_ int macsecfd)
{
    SWSS_LOG_ENTER();

    m_macsecfd = macsecfd;
}

MACsecFilter::MACsecFilterState MACsecFilter::get_state() const
{
    SWSS_LOG_ENTER();

    return m_state;
}

TrafficFilter::FilterStatus MACsecFilter::execute(
        _Inout_ void *buffer,
        _Inout_ size_t &length)
{
    SWSS_LOG_ENTER();

    MACsecFilterStateGuard state_guard(m_state, MACsecFilter::MACsecFilterState::MACSEC_FILTER_STATE_BUSY);
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
