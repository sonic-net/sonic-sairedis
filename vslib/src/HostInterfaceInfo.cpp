#include "HostInterfaceInfo.h"
#include "SwitchStateBase.h"
#include "SelectableFd.h"
#include "EventPayloadPacket.h"

#include "swss/logger.h"
#include "swss/select.h"

#include "meta/sai_serialize.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/if_tun.h>
#include <sys/ioctl.h>
#include <net/if_arp.h>
#include <unistd.h>
#include <net/ethernet.h>
#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>

using namespace saivs;

HostInterfaceInfo::HostInterfaceInfo(
        _In_ int ifindex,
        _In_ int socket,
        _In_ int tapfd,
        _In_ const std::string& tapname,
        _In_ sai_object_id_t portId,
        _In_ std::shared_ptr<EventQueue> eventQueue):
    m_ifindex(ifindex),
    m_packet_socket(socket),
    m_name(tapname),
    m_portId(portId),
    m_eventQueue(eventQueue),
    m_tapfd(tapfd)
{
    SWSS_LOG_ENTER();

    m_run_thread = true;

    m_e2t = std::make_shared<std::thread>(&HostInterfaceInfo::veth2tap_fun, this);
    m_t2e = std::make_shared<std::thread>(&HostInterfaceInfo::tap2veth_fun, this);
}

HostInterfaceInfo::~HostInterfaceInfo()
{
    SWSS_LOG_ENTER();

    m_run_thread = false;

    m_e2tEvent.notify();
    m_t2eEvent.notify();

    if (m_t2e)
    {
        m_t2e->join();
    }

    if (m_e2t)
    {
        m_e2t->join();
    }

    // remove tap device

    int err = close(m_tapfd);

    if (err)
    {
        SWSS_LOG_ERROR("failed to remove tap device: %s, err: %d", m_name.c_str(), err);
    }

    SWSS_LOG_NOTICE("joined threads for hostif: %s", m_name.c_str());
}

void HostInterfaceInfo::async_process_packet_for_fdb_event(
        _In_ const uint8_t *data,
        _In_ size_t size) const
{
    SWSS_LOG_ENTER();

    auto buffer = Buffer(data, size);

    auto payload = std::make_shared<EventPayloadPacket>(m_portId, m_ifindex, m_name, buffer); 

    m_eventQueue->enqueue(std::make_shared<Event>(EventType::EVENT_TYPE_PACKET, payload));
}

bool HostInterfaceInfo::installEth2TapFilter(
        _In_ int priority,
        _In_ std::shared_ptr<TrafficFilter> filter)
{
    SWSS_LOG_ENTER();

    return m_e2tFilters.installFilter(priority, filter);
}

bool HostInterfaceInfo::uninstallEth2TapFilter(
        _In_ std::shared_ptr<TrafficFilter> filter)
{
    SWSS_LOG_ENTER();

    return m_e2tFilters.uninstallFilter(filter);
}

bool HostInterfaceInfo::installTap2EthFilter(
        _In_ int priority,
        _In_ std::shared_ptr<TrafficFilter> filter)
{
    SWSS_LOG_ENTER();

    return m_t2eFilters.installFilter(priority, filter);
}

bool HostInterfaceInfo::uninstallTap2EthFilter(
        _In_ std::shared_ptr<TrafficFilter> filter)
{
    SWSS_LOG_ENTER();

    return m_t2eFilters.uninstallFilter(filter);
}

#define ETH_FRAME_BUFFER_SIZE (0x4000)
#define CONTROL_MESSAGE_BUFFER_SIZE (0x1000)
#define IEEE_8021Q_ETHER_TYPE (0x8100)
#define MAC_ADDRESS_SIZE (6)
#define VLAN_TAG_SIZE (4)

void HostInterfaceInfo::veth2tap_fun()
{
    SWSS_LOG_ENTER();

    unsigned char buffer[ETH_FRAME_BUFFER_SIZE];

    swss::Select s;
    SelectableFd fd(m_packet_socket);

    s.addSelectable(&m_e2tEvent);
    s.addSelectable(&fd);

    while (m_run_thread)
    {
        struct msghdr  msg;
        memset(&msg, 0, sizeof(struct msghdr));

        struct sockaddr_storage src_addr;

        struct iovec iov[1];

        iov[0].iov_base = buffer;       // buffer for message
        iov[0].iov_len = sizeof(buffer);

        char control[CONTROL_MESSAGE_BUFFER_SIZE];   // buffer for control messages

        msg.msg_name = &src_addr;
        msg.msg_namelen = sizeof(src_addr);
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;
        msg.msg_control = control;
        msg.msg_controllen = sizeof(control);

        swss::Selectable *sel = NULL;

        int result = s.select(&sel);

        if (result != swss::Select::OBJECT)
        {
            SWSS_LOG_ERROR("selectable failed: %d, ending thread for %s", result, m_name.c_str());
            return;
        }

        if (sel == &m_e2tEvent) // thread end event
            break;

        ssize_t size = recvmsg(m_packet_socket, &msg, 0);

        if (size < 0)
        {
            if (errno != ENETDOWN)
            {
                SWSS_LOG_ERROR("failed to read from socket fd %d, errno(%d): %s",
                        m_packet_socket, errno, strerror(errno));
            }

            continue;
        }

        if (size < (ssize_t)sizeof(ethhdr))
        {
            SWSS_LOG_ERROR("invalid ethernet frame length: %zu", msg.msg_controllen);
            continue;
        }

        struct cmsghdr *cmsg;

        for (cmsg = CMSG_FIRSTHDR(&msg); cmsg != NULL; cmsg = CMSG_NXTHDR(&msg, cmsg))
        {
            if (cmsg->cmsg_level != SOL_PACKET || cmsg->cmsg_type != PACKET_AUXDATA)
                continue;

            struct tpacket_auxdata* aux = (struct tpacket_auxdata*)CMSG_DATA(cmsg);

            if ((aux->tp_status & TP_STATUS_VLAN_VALID) &&
                    (aux->tp_status & TP_STATUS_VLAN_TPID_VALID))
            {
                SWSS_LOG_DEBUG("got vlan tci: 0x%x, vlanid: %d", aux->tp_vlan_tci, aux->tp_vlan_tci & 0xFFF);

                // inject vlan tag into frame

                // for overlapping buffers
                memmove(buffer + 2 * MAC_ADDRESS_SIZE + VLAN_TAG_SIZE,
                        buffer + 2 * MAC_ADDRESS_SIZE,
                        size - (2 * MAC_ADDRESS_SIZE));

                uint16_t tci = htons(aux->tp_vlan_tci);
                uint16_t tpid = htons(IEEE_8021Q_ETHER_TYPE);

                uint8_t* pvlan =  (uint8_t *)(buffer + 2 * MAC_ADDRESS_SIZE);
                memcpy(pvlan, &tpid, sizeof(uint16_t));
                memcpy(pvlan + sizeof(uint16_t), &tci, sizeof(uint16_t));

                size += VLAN_TAG_SIZE;

                break;
            }
        }

        async_process_packet_for_fdb_event(buffer, size);

        auto ret = m_e2tFilters.execute(buffer, size);
        if (ret == TrafficFilter::TERMINATE)
        {
            continue;
        }
        else if (ret == TrafficFilter::ERROR)
        {
            // Error log should be recorded in filter
            return;
        }

        if (write(m_tapfd, buffer, size) < 0)
        {
            /*
             * We filter out EIO because of this patch:
             * https://github.com/torvalds/linux/commit/1bd4978a88ac2589f3105f599b1d404a312fb7f6
             */

            if (errno != ENETDOWN && errno != EIO)
            {
                SWSS_LOG_ERROR("failed to write to tap device fd %d, errno(%d): %s",
                        m_tapfd, errno, strerror(errno));
            }

            if (errno == EBADF)
            {
                // bad file descriptor, just end thread
                SWSS_LOG_NOTICE("ending thread for tap fd %d", m_tapfd);
                return;
            }

            continue;
        }
    }

    SWSS_LOG_NOTICE("ending thread proc for %s", m_name.c_str());
}

void HostInterfaceInfo::tap2veth_fun()
{
    SWSS_LOG_ENTER();

    unsigned char buffer[ETH_FRAME_BUFFER_SIZE];

    swss::Select s;
    SelectableFd fd(m_tapfd);

    s.addSelectable(&m_t2eEvent);
    s.addSelectable(&fd);

    while (m_run_thread)
    {
        swss::Selectable *sel = NULL;

        int result = s.select(&sel);

        if (result != swss::Select::OBJECT)
        {
            SWSS_LOG_ERROR("selectable failed: %d, ending thread for %s", result, m_name.c_str());
            return;
        }

        if (sel == &m_t2eEvent) // thread end event
            break;

        ssize_t size = read(m_tapfd, buffer, sizeof(buffer));

        if (size < 0)
        {
            SWSS_LOG_ERROR("failed to read from tapfd fd %d, errno(%d): %s",
                    m_tapfd, errno, strerror(errno));

            if (errno == EBADF)
            {
                // bad file descriptor, just close the thread
                SWSS_LOG_NOTICE("ending thread for tap fd %d", m_tapfd);
                return;
            }

            continue;
        }

        auto ret = m_t2eFilters.execute(buffer, size);
        if (ret == TrafficFilter::TERMINATE)
        {
            continue;
        }
        else if (ret == TrafficFilter::ERROR)
        {
            // Error log should be recorded in filter
            return;
        }

        if (write(m_packet_socket, buffer, (int)size) < 0)
        {
            if (errno != ENETDOWN)
            {
                SWSS_LOG_ERROR("failed to write to socket fd %d, errno(%d): %s",
                        m_packet_socket, errno, strerror(errno));
            }

            continue;
        }
    }

    SWSS_LOG_NOTICE("ending thread proc for %s", m_name.c_str());
}
