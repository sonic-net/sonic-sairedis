#include "TrafficForwarder.h"

#include <linux/if_packet.h>

#include <gtest/gtest.h>

using namespace saivs;

typedef union _control
{
    char control_data[CMSG_SPACE(sizeof(tpacket_auxdata))];
    struct cmsghdr cmsg;

} control;

static_assert(sizeof(cmsghdr) == 16, "header must be 8 bytes");
static_assert(sizeof(control) >= (sizeof(cmsghdr) + sizeof(tpacket_auxdata)), "control must at least include both");

TEST(TrafficForwarder, addVlanTag)
{
    uint8_t buffer[0x1000];

    size_t length = 1;

    struct msghdr hdr;

    memset(&hdr, 0, sizeof(hdr));

    EXPECT_FALSE(TrafficForwarder::addVlanTag(buffer, length, hdr));

    control p;

    memset(&p, 0, sizeof(p));

    hdr.msg_controllen = sizeof(p);
    hdr.msg_control = &p;

    EXPECT_FALSE(TrafficForwarder::addVlanTag(buffer, length, hdr));

    cmsghdr* cmsg = &p.cmsg;

    cmsg->cmsg_level = SOL_PACKET;
    cmsg->cmsg_type = PACKET_AUXDATA;

    EXPECT_FALSE(TrafficForwarder::addVlanTag(buffer, length, hdr));

    struct tpacket_auxdata* aux = (struct tpacket_auxdata*)CMSG_DATA(cmsg);

    // https://en.wikipedia.org/wiki/IEEE_802.1Q
    // 
    // TPID(16) |          TCI(16)
    //          |  PCP(3)  DEI(1) VID(12)

    aux->tp_status |= TP_STATUS_VLAN_VALID;
    aux->tp_status |= TP_STATUS_VLAN_TPID_VALID;

    length = ETH_FRAME_BUFFER_SIZE;

    EXPECT_THROW(TrafficForwarder::addVlanTag(buffer, length, hdr), std::runtime_error);

    length = 64;

    EXPECT_TRUE(TrafficForwarder::addVlanTag(buffer, length, hdr));

    EXPECT_EQ(length, 68); 
}
