#include "HostInterfaceInfo.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(HostInterfaceInfo, sendTo)
{
    auto eq = std::make_shared<EventQueue>(std::make_shared<Signal>());

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    HostInterfaceInfo hii(0, s, fd, "tap", 0, eq);

    usleep(100*1000);

    unsigned char buf[2];

    EXPECT_EQ(hii.sendTo(s, buf, 0), true);

    close(s);
    close(fd);

    EXPECT_EQ(hii.sendTo(s, buf, 0), false);
}
