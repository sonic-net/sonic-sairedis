#include "ShareMemoryChannel.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;

TEST(ShmChannel, flush)
{
    auto c = std::make_shared<ShareMemoryChannel>("valid_shm_name", "valid_ntf_shm_name", nullptr);

    c->flush();
}

TEST(ShmChannel, wait)
{
    auto c = std::make_shared<ShareMemoryChannel>("valid_shm_name", "valid_ntf_shm_name", nullptr);

    c->setResponseTimeout(60);

    swss::KeyOpFieldsValuesTuple kco;

    EXPECT_NE(c->wait("foo", kco), SAI_STATUS_SUCCESS);
}
