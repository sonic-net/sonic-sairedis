#include <cstdint>

#include <memory>
#include <vector>
#include <array>

#include <gtest/gtest.h>

#include "meta/DummySaiInterface.h"

#include "Sai.h"
#include "Proxy.h"

using namespace saiproxy;

TEST(Proxy, ctr)
{
    Sai sai;

    std::shared_ptr<sairedis::SaiInterface> dummy = std::make_shared<saimeta::DummySaiInterface>();

    // will test loadProfileMap

    auto proxy = std::make_shared<Proxy>(dummy);
}
