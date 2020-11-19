#include "MACsecForwarder.h"
#include "SwitchStateBase.h"
#include "SelectableFd.h"

#include "swss/logger.h"
#include "swss/select.h"

#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>

using namespace saivs;

#define ETH_FRAME_BUFFER_SIZE (0x4000)

MACsecForwarder::MACsecForwarder(
    _In_ const std::string &macsecInterfaceName,
    _In_ int tapfd):
    m_tapfd(tapfd),
    m_macsec_interface_name(macsecInterfaceName),
    m_run_thread(true)
{
    SWSS_LOG_ENTER();

    m_macsecfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    if (m_macsecfd < 0)
    {
        SWSS_LOG_THROW(
            "failed to open macsec socket %s, errno: %d",
            m_macsec_interface_name.c_str(),
            errno);
    }

    struct sockaddr_ll sock_address;
    memset(&sock_address, 0, sizeof(sock_address));

    sock_address.sll_family = PF_PACKET;
    sock_address.sll_protocol = htons(ETH_P_ALL);
    sock_address.sll_ifindex = if_nametoindex(m_macsec_interface_name.c_str());

    if (sock_address.sll_ifindex == 0)
    {
        close(m_macsecfd);
        SWSS_LOG_THROW(
            "failed to get interface index for %s",
            m_macsec_interface_name.c_str());
    }

    if (SwitchStateBase::promisc(m_macsec_interface_name.c_str()))
    {
        close(m_macsecfd);
        SWSS_LOG_THROW(
            "promisc failed on %s",
            m_macsec_interface_name.c_str());
    }

    if (bind(m_macsecfd, (struct sockaddr *)&sock_address, sizeof(sock_address)) < 0)
    {
        close(m_macsecfd);
        SWSS_LOG_THROW(
            "bind failed on %s",
            m_macsec_interface_name.c_str());
    }

    m_forward_thread = std::make_shared<std::thread>(&MACsecForwarder::forward, this);

    SWSS_LOG_NOTICE(
        "setup MACsec forward rule for %s succeeded",
        m_macsec_interface_name.c_str());
}

MACsecForwarder::~MACsecForwarder()
{
    SWSS_LOG_ENTER();

    m_run_thread = false;
    m_exit_event.notify();
    m_forward_thread->join();

    int err = close(m_macsecfd);

    if (err != 0)
    {
        SWSS_LOG_ERROR(
            "failed to close macsec device: %s, err: %d",
            m_macsec_interface_name.c_str(),
            err);
    }
}

int MACsecForwarder::get_macsecfd() const
{
    SWSS_LOG_ENTER();

    return m_macsecfd;
}

void MACsecForwarder::forward()
{
    SWSS_LOG_ENTER();

    unsigned char buffer[ETH_FRAME_BUFFER_SIZE];
    swss::Select s;
    SelectableFd fd(m_macsecfd);

    s.addSelectable(&m_exit_event);
    s.addSelectable(&fd);

    while (m_run_thread)
    {
        swss::Selectable *sel = NULL;
        int result = s.select(&sel);

        if (result != swss::Select::OBJECT)
        {
            SWSS_LOG_ERROR(
                "selectable failed: %d, ending thread for %s",
                result,
                m_macsec_interface_name.c_str());

            break;
        }

        if (sel == &m_exit_event) // thread end event
            break;

        ssize_t size = read(m_macsecfd, buffer, sizeof(buffer));

        if (size < 0)
        {
            SWSS_LOG_WARN(
                "failed to read from macsec device %s fd %d, errno(%d): %s",
                m_macsec_interface_name.c_str(),
                m_macsecfd,
                errno,
                strerror(errno));

            if (errno == EBADF)
            {
                // bad file descriptor, just close the thread
                SWSS_LOG_NOTICE(
                    "ending thread for macsec device %s",
                    m_macsec_interface_name.c_str());

                break;
            }

            continue;
        }

        if (write(m_tapfd, buffer, static_cast<int>(size)) < 0)
        {

            if (errno != ENETDOWN && errno != EIO)
            {
                SWSS_LOG_ERROR(
                    "failed to write to macsec device %s fd %d, errno(%d): %s",
                    m_macsec_interface_name.c_str(),
                    m_macsecfd,
                    errno,
                    strerror(errno));
            }

            if (errno == EBADF)
            {
                // bad file descriptor, just end thread
                SWSS_LOG_ERROR(
                    "ending thread for macsec device %s fd %d",
                    m_macsec_interface_name.c_str(),
                    m_macsecfd);
                return;
            }

            continue;
        }
    }

    SWSS_LOG_NOTICE(
        "ending thread proc for %s",
        m_macsec_interface_name.c_str());
}

