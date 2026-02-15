#include "MACsecEgressFilter.h"

#include <gtest/gtest.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <linux/if_packet.h>
#include <linux/if_ether.h>
#include <arpa/inet.h>
#include <net/if.h>

#define EAPOL_ETHER_TYPE (0x888e)

using namespace saivs;

TEST(MACsecEgressFilter, ctr)
{
    MACsecEgressFilter filter("foo");
}

TEST(MACsecEgressFilter, forward)
{
    MACsecEgressFilter filter("foo");

    filter.set_macsec_fd(0);

    uint8_t packet[4000];

    memset(packet, 0, sizeof(packet));

    ethhdr* eth = (ethhdr*)packet;

    eth->h_proto = ntohs(EAPOL_ETHER_TYPE);

    size_t len = sizeof(packet);

    EXPECT_EQ(filter.execute(packet, len), TrafficFilter::CONTINUE);

    eth->h_proto = ntohs(6);

    EXPECT_EQ(filter.execute(packet, len), TrafficFilter::TERMINATE);

    filter.enable_macsec_device(true);

    // fd is ok, stdout
    EXPECT_EQ(filter.execute(packet, len), TrafficFilter::TERMINATE);

    filter.set_macsec_fd(70); // bad fd

    EXPECT_EQ(filter.execute(packet, len), TrafficFilter::TERMINATE);
}
