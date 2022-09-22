#include "System.h"

#include <gtest/gtest.h>

#include <memory>

using namespace saimeta;

TEST(System, reset)
{
    System::reset();
}

TEST(System, open)
{
    System::open_ptr = nullptr;

    int ret = System::open("/f/o/o", O_RDWR);

    EXPECT_TRUE(ret < 0);

    System::open_ptr = &::open;

    ret = System::open("/f/o/o", O_RDWR);

    EXPECT_TRUE(ret < 0);
}

TEST(System, socket)
{
    System::socket_ptr = nullptr;

    int ret = System::socket(100,100,100);

    EXPECT_TRUE(ret < 0);

    System::socket_ptr = &::socket;

    ret = System::socket(100,100,100);

    EXPECT_TRUE(ret < 0);

}

TEST(System, ioctl)
{
    System::ioctl_ptr = nullptr;

    int ret = System::ioctl(-1,0);

    EXPECT_TRUE(ret < 0);

    System::ioctl_ptr= &::ioctl;

    ret = System::ioctl(-1,0);

    EXPECT_TRUE(ret < 0);

}

TEST(System, setsockopt)
{
    System::setsockopt_ptr = nullptr;

    int ret = System::setsockopt(-1,0,0,0,0);

    EXPECT_TRUE(ret < 0);

    System::setsockopt_ptr= &::setsockopt;

    ret = System::setsockopt(-1,0,0,0,0);

    EXPECT_TRUE(ret < 0);

}

TEST(System, if_nametoindex)
{
    System::bind_ptr = nullptr;

    unsigned int ret = System::if_nametoindex("foo");

    EXPECT_TRUE(ret == 0);

    System::if_nametoindex_ptr = &::if_nametoindex;

    ret = System::if_nametoindex("foo");

    EXPECT_TRUE(ret == 0);
}
