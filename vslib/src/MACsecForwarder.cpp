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

MACsecForwarder::MACsecForwarder(
    _In_ const std::string &macsecInterfaceName,
    _In_ std::shared_ptr<HostInterfaceInfo> info):
    m_macsecInterfaceName(macsecInterfaceName),
    m_runThread(true),
    m_info(info)
{
    SWSS_LOG_ENTER();

    if (m_info == nullptr)
    {
        SWSS_LOG_THROW("The HostInterfaceInfo on the MACsec port %s is empty", m_macsecInterfaceName.c_str());
    }

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
        struct msghdr msg;
        memset(&msg, 0, sizeof(struct msghdr));

        struct sockaddr_storage srcAddr;

        struct iovec iov[1];

        iov[0].iov_base = buffer; // buffer for message
        iov[0].iov_len = sizeof(buffer);

        char control[CONTROL_MESSAGE_BUFFER_SIZE]; // buffer for control messages

        msg.msg_name = &srcAddr;
        msg.msg_namelen = sizeof(srcAddr);
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);

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

        ssize_t size = recvmsg(m_macsecfd, &msg, 0);

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
        else if (size < (ssize_t)sizeof(ethhdr))
        {
            SWSS_LOG_ERROR("invalid ethernet frame length: %zu", msg.msg_controllen);

            continue;
        }

        size_t length = static_cast<size_t>(size);

        addVlanTag(buffer, length, msg);

        m_info->async_process_packet_for_fdb_event(buffer, length);

        if (!sendTo(m_info->m_tapfd, buffer, length))
        {
            break;
        }
    }

    SWSS_LOG_NOTICE(
        "ending thread proc for %s",
        m_macsecInterfaceName.c_str());
}
