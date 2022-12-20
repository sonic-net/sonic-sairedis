#include "MockSwitchStateBase.h"

#include "meta/System.h"

#include <gtest/gtest.h>

using namespace saivs;

static int mock_open(const char *pathname, int flags, ...)
{
    SWSS_LOG_ENTER();
    return 100;
}

static int mock_ioctl(int fd, unsigned long request, ...)
{
    SWSS_LOG_ENTER();
    return 100;
}

static int mock_socket(int domain, int type, int protocol)
{
    SWSS_LOG_ENTER();
    return 100;
}

static int mock_setsockopt(int sockfd, int level, int optname, const void *optval, socklen_t optlen)
{
    SWSS_LOG_ENTER();
    return 100;
}

static int mock_bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    SWSS_LOG_ENTER();
    return 100;
}

static unsigned int mock_if_nametoindex(const char *ifname)
{
    SWSS_LOG_ENTER();
    return 100;
}

TEST(SwitchStateBaseHostif, vs_create_tap_device)
{
    saimeta::System::open_ptr = &mock_open;
    saimeta::System::ioctl_ptr = &mock_ioctl;
    saimeta::System::socket_ptr = &mock_socket;
    saimeta::System::setsockopt_ptr = &mock_setsockopt;

    int ret = MockSwitchStateBase::call_vs_create_tap_device("foo", 0);

    EXPECT_TRUE(ret > 0);

    saimeta::System::reset();
}

TEST(SwitchStateBaseHostif, ifup)
{
    saimeta::System::open_ptr = &mock_open;
    saimeta::System::ioctl_ptr = &mock_ioctl;
    saimeta::System::socket_ptr = &mock_socket;
    saimeta::System::setsockopt_ptr = &mock_setsockopt;

    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto scc = std::make_shared<SwitchConfigContainer>();

    MockSwitchStateBase obj(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    int ret = obj.call_ifup("foo", 0x100000003, false, false);

    EXPECT_TRUE(ret > 0);

    saimeta::System::reset();
}

TEST(SwitchStateBaseHostif, promisc)
{
    saimeta::System::open_ptr = &mock_open;
    saimeta::System::ioctl_ptr = &mock_ioctl;
    saimeta::System::socket_ptr = &mock_socket;
    saimeta::System::setsockopt_ptr = &mock_setsockopt;

    int ret = SwitchStateBase::promisc("foo");

    EXPECT_TRUE(ret > 0);

    saimeta::System::reset();
}

TEST(SwitchStateBaseHostif, call_hostif_create_tap_veth_forwarding)
{
    saimeta::System::open_ptr = &mock_open;
    saimeta::System::ioctl_ptr = &mock_ioctl;
    saimeta::System::socket_ptr = &mock_socket;
    saimeta::System::setsockopt_ptr = &mock_setsockopt;
    saimeta::System::bind_ptr = &mock_bind;
    saimeta::System::if_nametoindex_ptr = &mock_if_nametoindex;

    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto scc = std::make_shared<SwitchConfigContainer>();

    MockSwitchStateBase obj(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    bool ret = obj.call_hostif_create_tap_veth_forwarding("tapname", 100, 0x100000003);

    EXPECT_FALSE(ret);

    saimeta::System::reset();
}

TEST(SwitchStateBaseHostif, call_vs_create_hostif_tap_interface)
{
    saimeta::System::open_ptr = &mock_open;
    saimeta::System::ioctl_ptr = &mock_ioctl;
    saimeta::System::socket_ptr = &mock_socket;
    saimeta::System::setsockopt_ptr = &mock_setsockopt;
    saimeta::System::bind_ptr = &mock_bind;
    saimeta::System::if_nametoindex_ptr = &mock_if_nametoindex;

    auto sc = std::make_shared<SwitchConfig>(0, "");
    auto scc = std::make_shared<SwitchConfigContainer>();

    MockSwitchStateBase obj(
            0x2100000000,
            std::make_shared<RealObjectIdManager>(0, scc),
            sc);

    sai_attribute_t attrs[5];

    attrs[0].id = SAI_HOSTIF_ATTR_TYPE;
    attrs[0].value.s32 = SAI_HOSTIF_TYPE_NETDEV;

    attrs[1].id = SAI_HOSTIF_ATTR_OBJ_ID;
    attrs[1].value.oid = 0x100000003;

    attrs[2].id = SAI_HOSTIF_ATTR_NAME;

    strcpy(attrs[2].value.chardata, "foo");

    sai_status_t ret = obj.call_vs_create_hostif_tap_interface(3, attrs);

    EXPECT_NE(ret, SAI_STATUS_SUCCESS);

    saimeta::System::reset();
}
