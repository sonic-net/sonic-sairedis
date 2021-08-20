#include "HostInterfaceInfo.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

TEST(HostInterfaceInfo, async_process_packet_for_fdb_event)
{
    auto eq = std::make_shared<EventQueue>(std::make_shared<Signal>());

    int s = socket(AF_INET, SOCK_DGRAM, 0);
    int fd = socket(AF_INET, SOCK_DGRAM, 0);

    HostInterfaceInfo hii(0, s, fd, "tap", 0, eq);

    usleep(100*1000); // give some time to start thread

    struct sockaddr_in server;

    memset(&server, 0, sizeof(server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_port = htons(4000);

    unsigned char buffer[200];

    ssize_t res = sendto(fd, buffer, 200, 0,
                (const struct sockaddr*)&server, sizeof(server));

    EXPECT_FALSE(res < 0);

    res = sendto(s, buffer, 200, 0,
                (const struct sockaddr*)&server, sizeof(server));

    EXPECT_FALSE(res < 0);

    usleep(100*1000); // give some time to process async

    close(s);
    close(fd);
}
