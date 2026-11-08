#include "FdbInfo.h"

#include <linux/if.h>

#include <gtest/gtest.h>

using namespace saivs;

TEST(FdbInfo, getPortId)
{
    FdbInfo fdb;

    EXPECT_EQ(fdb.getPortId(), 0);
}

TEST(FdbInfo, getVlanId)
{
    FdbInfo fdb;

    EXPECT_EQ(fdb.getVlanId(), 0);
}

TEST(FdbInfo, getBridgePortId)
{
    FdbInfo fdb;

    EXPECT_EQ(fdb.getBridgePortId(), 0);
}

TEST(FdbInfo, getFdbEntry)
{
    FdbInfo fdb;

    auto entry = fdb.getFdbEntry();

    EXPECT_EQ(entry.switch_id, 0);
}

TEST(FdbInfo, getTimestamp)
{
    FdbInfo fdb;

    EXPECT_EQ(fdb.getTimestamp(), 0);
}

TEST(FdbInfo, serialize)
{
    FdbInfo fdb;

    auto str = fdb.serialize();

    EXPECT_EQ(str,
            "{\"bridge_port_id\":\"oid:0x0\","
            "\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x0\\\",\\\"mac\\\":\\\"00:00:00:00:00:00\\\",\\\"switch_id\\\":\\\"oid:0x0\\\"}\","
            "\"port_id\":\"oid:0x0\","
            "\"timestamp\":\"0\","
            "\"vlan_id\":\"0\"}");
}

TEST(FdbInfo, deserialize)
{
    std::string str =
            "{\"bridge_port_id\":\"oid:0x1\","
            "\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x0\\\",\\\"mac\\\":\\\"00:00:00:00:00:00\\\",\\\"switch_id\\\":\\\"oid:0x0\\\"}\","
            "\"port_id\":\"oid:0x0\","
            "\"timestamp\":\"0\","
            "\"vlan_id\":\"0\"}";

    auto fdb = FdbInfo::deserialize(str);

    EXPECT_EQ(fdb.getBridgePortId(), 1);
}

TEST(FdbInfo, setFdbEntry)
{
    FdbInfo fdb;

    sai_fdb_entry_t entry;

    entry.switch_id = 2;

    fdb.setFdbEntry(entry);

    auto en = fdb.getFdbEntry();

    EXPECT_EQ(en.switch_id, 2);
}

TEST(FdbInfo, setVlanId)
{
    FdbInfo info;

    info.setVlanId(7);

    EXPECT_EQ(info.getVlanId(), 7);
}

TEST(FdbInfo, setPortId)
{
    FdbInfo info;

    info.setPortId(7);

    EXPECT_EQ(info.getPortId(), 7);
}

TEST(FdbInfo, setBridgePortId)
{
    FdbInfo info;

    info.setBridgePortId(7);

    EXPECT_EQ(info.getBridgePortId(), 7);
}

TEST(FdbInfo, setTimestamp)
{
    FdbInfo info;

    info.setTimestamp(7);

    EXPECT_EQ(info.getTimestamp(), 7);
}

TEST(FdbInfo, operator_bracket)
{
    std::string strA =
            "{\"bridge_port_id\":\"oid:0x1\","
            "\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x0\\\",\\\"mac\\\":\\\"00:00:00:00:00:00\\\",\\\"switch_id\\\":\\\"oid:0x0\\\"}\","
            "\"port_id\":\"oid:0x0\","
            "\"timestamp\":\"0\","
            "\"vlan_id\":\"0\"}";

    std::string strB =
            "{\"bridge_port_id\":\"oid:0x1\","
            "\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x0\\\",\\\"mac\\\":\\\"00:00:00:00:00:01\\\",\\\"switch_id\\\":\\\"oid:0x0\\\"}\","
            "\"port_id\":\"oid:0x0\","
            "\"timestamp\":\"0\","
            "\"vlan_id\":\"0\"}";

    auto a = FdbInfo::deserialize(strA);
    auto b = FdbInfo::deserialize(strB);

    EXPECT_EQ(a.operator()(a,b), true);
    EXPECT_EQ(a.operator()(b,a), false);
}

TEST(FdbInfo, operator_lt)
{
    std::string strA =
            "{\"bridge_port_id\":\"oid:0x1\","
            "\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x0\\\",\\\"mac\\\":\\\"00:00:00:00:00:00\\\",\\\"switch_id\\\":\\\"oid:0x0\\\"}\","
            "\"port_id\":\"oid:0x0\","
            "\"timestamp\":\"0\","
            "\"vlan_id\":\"0\"}";

    std::string strB =
            "{\"bridge_port_id\":\"oid:0x1\","
            "\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x0\\\",\\\"mac\\\":\\\"00:00:00:00:00:01\\\",\\\"switch_id\\\":\\\"oid:0x0\\\"}\","
            "\"port_id\":\"oid:0x0\","
            "\"timestamp\":\"0\","
            "\"vlan_id\":\"0\"}";

    auto a = FdbInfo::deserialize(strA);
    auto b = FdbInfo::deserialize(strB);

    EXPECT_EQ(a < b, true);
    EXPECT_EQ(b < a, false);
    EXPECT_EQ(a < a, false);
    EXPECT_EQ(b < b, false);

    std::string strC =
            "{\"bridge_port_id\":\"oid:0x1\","
            "\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x0\\\",\\\"mac\\\":\\\"00:00:00:00:00:00\\\",\\\"switch_id\\\":\\\"oid:0x0\\\"}\","
            "\"port_id\":\"oid:0x0\","
            "\"timestamp\":\"0\","
            "\"vlan_id\":\"1\"}";

    std::string strD =
            "{\"bridge_port_id\":\"oid:0x1\","
            "\"fdb_entry\":\"{\\\"bvid\\\":\\\"oid:0x0\\\",\\\"mac\\\":\\\"00:00:00:00:00:00\\\",\\\"switch_id\\\":\\\"oid:0x0\\\"}\","
            "\"port_id\":\"oid:0x0\","
            "\"timestamp\":\"0\","
            "\"vlan_id\":\"2\"}";

    auto c = FdbInfo::deserialize(strC);
    auto d = FdbInfo::deserialize(strD);

    EXPECT_EQ(c < d, true);
    EXPECT_EQ(d < c, false);
    EXPECT_EQ(c < c, false);
    EXPECT_EQ(d < d, false);

}
