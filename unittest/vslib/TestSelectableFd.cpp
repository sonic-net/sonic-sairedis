#include "SelectableFd.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(SelectableFd, ctr)
{
    EXPECT_THROW(std::make_shared<SelectableFd>(-1), std::runtime_error);

    SelectableFd fd(0);
}

TEST(SelectableFd, getFd)
{
    SelectableFd fd(7);

    EXPECT_EQ(fd.getFd(), 7);
}

TEST(SelectableFd, readData)
{
    SelectableFd fd(7);

    EXPECT_EQ(fd.readData(), 0);
}
