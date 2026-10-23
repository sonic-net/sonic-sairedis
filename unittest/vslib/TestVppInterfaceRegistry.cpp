#include "vpp/VppInterfaceRegistry.h"

#include <gtest/gtest.h>

#include <set>

using namespace saivs;

TEST(VppInterfaceRegistry, rifLookupUsesSeparateIndexForEveryInterfaceType)
{
    VppInterfaceRegistry registry;
    auto port = registry.addPhysicalPort("TenGigabitEthernet0/0/0", "Ethernet0", 1);
    auto lag = registry.addLag(2, 20, 2);
    auto bvi = registry.addBvi(100, 3);
    ASSERT_NE(nullptr, port);
    ASSERT_NE(nullptr, lag);
    ASSERT_NE(nullptr, bvi);
    ASSERT_NE(nullptr, registry.addSubInterface(port, 10, 10));
    ASSERT_NE(nullptr, registry.addSubInterface(lag, 20, 20));

    const std::vector<std::string> names = {
        "TenGigabitEthernet0/0/0", "BondEthernet2", "bvi100",
        "TenGigabitEthernet0/0/0.10", "BondEthernet2.20"
    };
    sai_object_id_t rif = 100;
    for (const auto& name : names)
    {
        ASSERT_TRUE(registry.setRifOid(name, rif));
        EXPECT_EQ(name, registry.resolveHwIfByRifOid(rif));
        EXPECT_EQ(nullptr, registry.findByOid(rif));
        ++rif;
    }
    EXPECT_EQ(port, registry.findByOid(1));
    EXPECT_EQ(lag, registry.findByOid(2));
    EXPECT_EQ(bvi, registry.findByOid(3));
}

TEST(VppInterfaceRegistry, rifBindingRejectsAliasesAndClearsPersistentInterfaces)
{
    VppInterfaceRegistry registry;
    auto port = registry.addPhysicalPort("port0", "Ethernet0", 1);
    auto lag = registry.addLag(2, 20, 2);
    ASSERT_NE(nullptr, port);
    ASSERT_NE(nullptr, lag);
    EXPECT_FALSE(registry.setRifOid("missing", 100));
    EXPECT_FALSE(registry.setRifOid("port0", SAI_NULL_OBJECT_ID));
    ASSERT_TRUE(registry.setRifOid("port0", 100));
    EXPECT_FALSE(registry.setRifOid("BondEthernet2", 100));
    EXPECT_EQ("port0", registry.resolveHwIfByRifOid(100));

    ASSERT_TRUE(registry.setRifOid("port0", 101));
    EXPECT_TRUE(registry.resolveHwIfByRifOid(100).empty());
    ASSERT_TRUE(registry.clearRifOid(101));
    EXPECT_FALSE(registry.clearRifOid(101));
    EXPECT_TRUE(registry.resolveHwIfByRifOid(101).empty());
    EXPECT_FALSE(port->hasRifOid());
    EXPECT_EQ(port, registry.findByOid(1));
    ASSERT_TRUE(registry.setRifOid("BondEthernet2", 101));
    EXPECT_EQ("BondEthernet2", registry.resolveHwIfByRifOid(101));
}

TEST(VppInterfaceRegistry, removingSubInterfaceLeavesParentRifBinding)
{
    VppInterfaceRegistry registry;
    auto port = registry.addPhysicalPort("port0", "Ethernet0", 1);
    ASSERT_NE(nullptr, registry.addSubInterface(port, 10, 10));
    ASSERT_TRUE(registry.setRifOid("port0", 100));
    ASSERT_TRUE(registry.setRifOid("port0.10", 101));
    EXPECT_EQ(1u, registry.remove("port0.10"));
    EXPECT_TRUE(registry.resolveHwIfByRifOid(101).empty());
    EXPECT_EQ("port0", registry.resolveHwIfByRifOid(100));
    ASSERT_NE(nullptr, registry.addSubInterface(port, 10, 10));
    ASSERT_TRUE(registry.setRifOid("port0.10", 101));
    EXPECT_EQ(2u, registry.removeByOid(1));
    EXPECT_TRUE(registry.resolveHwIfByRifOid(100).empty());
    EXPECT_TRUE(registry.resolveHwIfByRifOid(101).empty());
}

TEST(VppInterfaceRegistry, clearingRegistryDoesNotRetainRecycledRifOid)
{
    VppInterfaceRegistry registry;
    ASSERT_NE(nullptr, registry.addBvi(100, 1));
    ASSERT_TRUE(registry.setRifOid("bvi100", 100));
    registry.clear();
    EXPECT_TRUE(registry.resolveHwIfByRifOid(100).empty());
    ASSERT_NE(nullptr, registry.addBvi(200, 2));
    ASSERT_TRUE(registry.setRifOid("bvi200", 100));
    EXPECT_EQ("bvi200", registry.resolveHwIfByRifOid(100));
}

TEST(VppInterfaceRegistry, collectBdIdsReturnsTheBridgeDomainsOfInterfacesInOne)
{
    VppInterfaceRegistry registry;
    auto port = registry.addPhysicalPort("port0", "Ethernet0", 1);
    ASSERT_NE(nullptr, port);
    ASSERT_NE(nullptr, registry.addPhysicalPort("port1", "Ethernet4", 2));
    ASSERT_NE(nullptr, registry.addLag(2, 20, 3));
    ASSERT_NE(nullptr, registry.addSubInterface(port, 100, 100));
    EXPECT_TRUE(registry.collectBdIds().empty());

    ASSERT_TRUE(registry.setBdId("port0.100", 100));
    ASSERT_TRUE(registry.setBdId("port1", 1000));
    ASSERT_TRUE(registry.setBdId("BondEthernet2", 1000));
    EXPECT_EQ(std::set<uint32_t>({100, 1000}), registry.collectBdIds());

    ASSERT_TRUE(registry.clearBdId("port1"));
    EXPECT_EQ(std::set<uint32_t>({100, 1000}), registry.collectBdIds());
    ASSERT_TRUE(registry.clearBdId("BondEthernet2"));
    EXPECT_EQ(std::set<uint32_t>({100}), registry.collectBdIds());
    EXPECT_EQ(1u, registry.remove("port0.100"));
    EXPECT_TRUE(registry.collectBdIds().empty());
}

