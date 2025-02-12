#include "BestCandidateFinder.h"
#include "MockableSaiSwitchInterface.h"

#include <gtest/gtest.h>

using namespace syncd;
using namespace unittests;

TEST(BestCandidateFinder, getSaiAttrFromDefaultValue)
{
    AsicView av;

    auto *meta = sai_metadata_get_attr_metadata(
            SAI_OBJECT_TYPE_SWITCH,
            SAI_SWITCH_ATTR_VXLAN_DEFAULT_ROUTER_MAC);

    EXPECT_NE(meta, nullptr);

    auto sw = std::make_shared<MockableSaiSwitchInterface>(0,0);

    auto attr = BestCandidateFinder::getSaiAttrFromDefaultValue(av, sw, *meta);
    EXPECT_NE(attr, nullptr);
}
