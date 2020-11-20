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
    m_macsecInterfaceName(macsecInterfaceName),
    m_runThread(true)
{
    SWSS_LOG_ENTER();

    m_macsecfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));

    if (m_macsecfd < 0)
    {
        SWSS_LOG_THROW(
            "failed to open macsec socket %s, errno: %d",
            m_macsecInterfaceName.c_str(),
            errno);
    }

    struct sockaddr_ll sockAddress;
    memset(&sockAddress, 0, sizeof(sockAddress));

    sockAddress.sll_family = PF_PACKET;
    sockAddress.sll_protocol = htons(ETH_P_ALL);
    sockAddress.sll_ifindex = if_nametoindex(m_macsecInterfaceName.c_str());

    if (sockAddress.sll_ifindex == 0)
    {
        close(m_macsecfd);
        SWSS_LOG_THROW(
            "failed to get interface index for %s",
            m_macsecInterfaceName.c_str());
    }

    if (SwitchStateBase::promisc(m_macsecInterfaceName.c_str()))
    {
        close(m_macsecfd);
        SWSS_LOG_THROW(
            "promisc failed on %s",
            m_macsecInterfaceName.c_str());
    }

    if (bind(m_macsecfd, (struct sockaddr *)&sockAddress, sizeof(sockAddress)) < 0)
    {
        close(m_macsecfd);
        SWSS_LOG_THROW(
            "bind failed on %s",
            m_macsecInterfaceName.c_str());
    }

    m_forwardThread = std::make_shared<std::thread>(&MACsecForwarder::forward, this);

    SWSS_LOG_NOTICE(
        "setup MACsec forward rule for %s succeeded",
        m_macsecInterfaceName.c_str());
}

MACsecForwarder::~MACsecForwarder()
{
    SWSS_LOG_ENTER();

    m_runThread = false;
    m_exitEvent.notify();
    m_forwardThread->join();

    int err = close(m_macsecfd);

    if (err != 0)
    {
        SWSS_LOG_ERROR(
            "failed to close macsec device: %s, err: %d",
            m_macsecInterfaceName.c_str(),
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

    s.addSelectable(&m_exitEvent);
    s.addSelectable(&fd);

    while (m_runThread)
    {
        swss::Selectable *sel = NULL;
        int result = s.select(&sel);

        if (result != swss::Select::OBJECT)
        {
            SWSS_LOG_ERROR(
                "selectable failed: %d, ending thread for %s",
                result,
                m_macsecInterfaceName.c_str());

            break;
        }

        if (sel == &m_exitEvent) // thread end event
            break;

        ssize_t size = read(m_macsecfd, buffer, sizeof(buffer));

        if (size < 0)
        {
            SWSS_LOG_WARN(
                "failed to read from macsec device %s fd %d, errno(%d): %s",
                m_macsecInterfaceName.c_str(),
                m_macsecfd,
                errno,
                strerror(errno));

            if (errno == EBADF)
            {
                // bad file descriptor, just close the thread
                SWSS_LOG_NOTICE(
                    "ending thread for macsec device %s",
                    m_macsecInterfaceName.c_str());

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
                    m_macsecInterfaceName.c_str(),
                    m_macsecfd,
                    errno,
                    strerror(errno));
            }

            if (errno == EBADF)
            {
                // bad file descriptor, just end thread
                SWSS_LOG_ERROR(
                    "ending thread for macsec device %s fd %d",
                    m_macsecInterfaceName.c_str(),
                    m_macsecfd);
                return;
            }

            continue;
        }
    }

    SWSS_LOG_NOTICE(
        "ending thread proc for %s",
        m_macsecInterfaceName.c_str());
}

