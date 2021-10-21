#include "VirtualObjectIdManager.h"

#include "meta/NumberOidIndexGenerator.h"

#include "swss/logger.h"

#include <gtest/gtest.h>

#include <memory>

using namespace sairedis;
using namespace saimeta;

static auto createVirtualObjectIdManager()
{
    SWSS_LOG_ENTER();

    auto sc = std::make_shared<SwitchConfig>();

    sc->m_switchIndex = 0;
    sc->m_hardwareInfo = "";

    auto scc = std::make_shared<SwitchConfigContainer>();

    scc->insert(sc);

    return std::make_shared<VirtualObjectIdManager>(0, scc, std::make_shared<NumberOidIndexGenerator>());
}

TEST(VirtualObjectIdManager, ctr)
{
    EXPECT_THROW(std::make_shared<VirtualObjectIdManager>(1000, nullptr, nullptr), std::runtime_error);

    auto m = createVirtualObjectIdManager();
}

TEST(VirtualObjectIdManager, saiSwitchIdQuery)
{
    auto m = createVirtualObjectIdManager();

    EXPECT_EQ(m->saiSwitchIdQuery(SAI_NULL_OBJECT_ID), SAI_NULL_OBJECT_ID);

    EXPECT_THROW(m->saiSwitchIdQuery(1), std::runtime_error);
}

TEST(VirtualObjectIdManager, clear)
{
    auto m = createVirtualObjectIdManager();

    m->clear();
}

TEST(VirtualObjectIdManager, allocateNewSwitchObjectId)
{
    auto m = createVirtualObjectIdManager();

    m->allocateNewSwitchObjectId("");

    // currently is allowed to allocate same switch index twice

    EXPECT_NE(m->allocateNewSwitchObjectId(""), SAI_NULL_OBJECT_ID);

    EXPECT_EQ(m->allocateNewSwitchObjectId("foo"), SAI_NULL_OBJECT_ID);
}

TEST(VirtualObjectIdManager, releaseObjectId)
{
    auto m = createVirtualObjectIdManager();

    auto sid = m->allocateNewSwitchObjectId("");

    EXPECT_NE(sid, SAI_NULL_OBJECT_ID);

    m->releaseObjectId(sid);

    EXPECT_THROW(m->releaseObjectId(sid), std::runtime_error);
}

TEST(VirtualObjectIdManager, updateObjectIndex)
{
    EXPECT_THROW(VirtualObjectIdManager::updateObjectIndex(SAI_NULL_OBJECT_ID, 1), std::runtime_error);

    EXPECT_THROW(VirtualObjectIdManager::updateObjectIndex(1, 0x10000000000), std::runtime_error);

    EXPECT_THROW(VirtualObjectIdManager::updateObjectIndex(1, 1), std::runtime_error);
}

TEST(VirtualObjectIdManager, allocateNewObjectId)
{
    auto m = createVirtualObjectIdManager();

    EXPECT_THROW(m->allocateNewObjectId(SAI_OBJECT_TYPE_NULL, SAI_NULL_OBJECT_ID), std::runtime_error);

    auto sid = m->allocateNewSwitchObjectId("");

    EXPECT_THROW(m->allocateNewObjectId(SAI_OBJECT_TYPE_SWITCH, sid), std::runtime_error);

    EXPECT_THROW(m->allocateNewObjectId(SAI_OBJECT_TYPE_PORT, 1), std::runtime_error);
}
