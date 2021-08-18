#include "RealObjectIdManager.h"

#include <gtest/gtest.h>

using namespace saivs;

TEST(RealObjectIdManager, ctr)
{
    EXPECT_THROW(std::make_shared<RealObjectIdManager>(10000, nullptr), std::runtime_error);

    EXPECT_THROW(std::make_shared<RealObjectIdManager>(0, nullptr), std::runtime_error);

    auto scc = std::make_shared<SwitchConfigContainer>();

    std::make_shared<RealObjectIdManager>(0, scc);
}

TEST(RealObjectIdManager, saiSwitchIdQuery)
{
    auto scc = std::make_shared<SwitchConfigContainer>();

    RealObjectIdManager mgr(0, scc);

    EXPECT_EQ(mgr.saiSwitchIdQuery(0), 0);

    EXPECT_THROW(mgr.saiSwitchIdQuery(1), std::runtime_error);
}

TEST(RealObjectIdManager, saiObjectTypeQuery)
{
    auto scc = std::make_shared<SwitchConfigContainer>();

    RealObjectIdManager mgr(0, scc);

    EXPECT_EQ(mgr.saiObjectTypeQuery(0), 0);

    EXPECT_EQ(mgr.saiObjectTypeQuery(-1), 0);
}

TEST(RealObjectIdManager, clear)
{
    auto scc = std::make_shared<SwitchConfigContainer>();

    RealObjectIdManager mgr(0, scc);

    mgr.clear();
}

TEST(RealObjectIdManager, updateWarmBootObjectIndex)
{
    auto scc = std::make_shared<SwitchConfigContainer>();

    RealObjectIdManager mgr(0, scc);

    EXPECT_THROW(mgr.updateWarmBootObjectIndex(0), std::runtime_error);
}

TEST(RealObjectIdManager, switchIdQuery)
{
    EXPECT_EQ(RealObjectIdManager::switchIdQuery(1), 0);
}

TEST(RealObjectIdManager, allocateNewSwitchObjectId)
{
    auto scc = std::make_shared<SwitchConfigContainer>();

    RealObjectIdManager mgr(0, scc);

    EXPECT_EQ(mgr.allocateNewSwitchObjectId("foo"), 0);
}

TEST(RealObjectIdManager, allocateNewObjectId)
{
    auto scc = std::make_shared<SwitchConfigContainer>();

    RealObjectIdManager mgr(0, scc);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    EXPECT_THROW(mgr.allocateNewObjectId((sai_object_type_t)1000,(sai_object_id_t)-1), std::runtime_error);
#pragma GCC diagnostic pop

    EXPECT_THROW(mgr.allocateNewObjectId(SAI_OBJECT_TYPE_SWITCH, (sai_object_id_t)-1), std::runtime_error);

}



