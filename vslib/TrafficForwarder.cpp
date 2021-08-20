#include "TrafficForwarder.h"

#include "swss/logger.h"

#include <memory.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
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

#define IEEE_8021Q_ETHER_TYPE (0x8100)
#define MAC_ADDRESS_SIZE (6)
#define VLAN_TAG_SIZE (4)

bool TrafficForwarder::addVlanTag(
        _Inout_ unsigned char *buffer,
        _Inout_ size_t &length,
        _Inout_ struct msghdr &msg)
{
    SWSS_LOG_ENTER();

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

            if ((length + VLAN_TAG_SIZE) > ETH_FRAME_BUFFER_SIZE)
            {
                SWSS_LOG_THROW("The VLAN packet size %lu exceeds the ETH_FRAME_BUFFER_SIZE", length + VLAN_TAG_SIZE);
            }

            // inject vlan tag into frame

            // for overlapping buffers
            memmove(buffer + 2 * MAC_ADDRESS_SIZE + VLAN_TAG_SIZE,
                    buffer + 2 * MAC_ADDRESS_SIZE,
                    length - (2 * MAC_ADDRESS_SIZE));

            uint16_t tci = htons(aux->tp_vlan_tci);
            uint16_t tpid = htons(IEEE_8021Q_ETHER_TYPE);

            uint8_t* pvlan =  (uint8_t *)(buffer + 2 * MAC_ADDRESS_SIZE);
            memcpy(pvlan, &tpid, sizeof(uint16_t));
            memcpy(pvlan + sizeof(uint16_t), &tci, sizeof(uint16_t));

            length += VLAN_TAG_SIZE;

            return true;
        }
    }

    return false;
}

bool TrafficForwarder::sendTo(
        _In_ int fd,
        _In_ const unsigned char *buffer,
        _In_ size_t length) const
{
    SWSS_LOG_ENTER();

    if (write(fd, buffer, static_cast<int>(length)) < 0)
    {
        /*
         * We filter out EIO because of this patch:
         * https://github.com/torvalds/linux/commit/1bd4978a88ac2589f3105f599b1d404a312fb7f6
         */

        if (errno != ENETDOWN && errno != EIO)
        {
            SWSS_LOG_ERROR("failed to write to device fd %d, errno(%d): %s",
                    fd,
                    errno,
                    strerror(errno));
        }

        if (errno == EBADF)
        {
            // bad file descriptor, just end thread
            SWSS_LOG_NOTICE("ending forward for fd %d", fd);

            return false;
        }
    }

    return true;
}
