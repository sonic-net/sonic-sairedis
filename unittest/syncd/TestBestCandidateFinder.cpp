#include "BestCandidateFinder.h"
#include "MockableSaiSwitchInterface.h"

#include "meta/sai_serialize.h"

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

TEST(BestCandidateFinder, matchVirtualRoutersByVlanInterfaces)
{
    const sai_object_id_t switchId = 0x21000000000000;
    const sai_object_id_t currentVrf1 = 0x3000000000001;
    const sai_object_id_t currentVrf2 = 0x3000000000002;
    const sai_object_id_t temporaryVrf1 = 0x3000000000101;
    const sai_object_id_t temporaryVrf2 = 0x3000000000102;
    const sai_object_id_t currentVlan1 = 0x26000000000001;
    const sai_object_id_t currentVlan2 = 0x26000000000002;
    const sai_object_id_t currentVlan3 = 0x26000000000003;
    const sai_object_id_t currentVlan4 = 0x26000000000004;
    const sai_object_id_t temporaryVlan1 = 0x26000000000101;
    const sai_object_id_t temporaryVlan2 = 0x26000000000102;
    const sai_object_id_t temporaryVlan3 = 0x26000000000103;
    const sai_object_id_t temporaryVlan4 = 0x26000000000104;
    const sai_object_id_t currentRif1 = 0x6000000000001;
    const sai_object_id_t currentRif2 = 0x6000000000002;
    const sai_object_id_t currentRif3 = 0x6000000000003;
    const sai_object_id_t currentRif4 = 0x6000000000004;
    const sai_object_id_t temporaryRif1 = 0x6000000000101;
    const sai_object_id_t temporaryRif2 = 0x6000000000102;
    const sai_object_id_t temporaryRif3 = 0x6000000000103;
    const sai_object_id_t temporaryRif4 = 0x6000000000104;

    auto key = [](const std::string& objectType, sai_object_id_t objectId)
    {
        return objectType + ":" + sai_serialize_object_id(objectId);
    };

    swss::TableDump current = {
        { key("SAI_OBJECT_TYPE_SWITCH", switchId), {} },
        { key("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", currentVrf1), {} },
        { key("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", currentVrf2), {} },
        { key("SAI_OBJECT_TYPE_VLAN", currentVlan1),
            { { "SAI_VLAN_ATTR_VLAN_ID", "1001" } } },
        { key("SAI_OBJECT_TYPE_VLAN", currentVlan2),
            { { "SAI_VLAN_ATTR_VLAN_ID", "1002" } } },
        { key("SAI_OBJECT_TYPE_VLAN", currentVlan3),
            { { "SAI_VLAN_ATTR_VLAN_ID", "2001" } } },
        { key("SAI_OBJECT_TYPE_VLAN", currentVlan4),
            { { "SAI_VLAN_ATTR_VLAN_ID", "2002" } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", currentRif1), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(currentVrf1) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(currentVlan1) } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", currentRif2), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(currentVrf1) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(currentVlan2) } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", currentRif3), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(currentVrf2) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(currentVlan3) } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", currentRif4), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(currentVrf2) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(currentVlan4) } } },
    };

    swss::TableDump temporary = {
        { key("SAI_OBJECT_TYPE_SWITCH", switchId), {} },
        { key("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", temporaryVrf1), {} },
        { key("SAI_OBJECT_TYPE_VIRTUAL_ROUTER", temporaryVrf2), {} },
        { key("SAI_OBJECT_TYPE_VLAN", temporaryVlan1),
            { { "SAI_VLAN_ATTR_VLAN_ID", "1001" } } },
        { key("SAI_OBJECT_TYPE_VLAN", temporaryVlan2),
            { { "SAI_VLAN_ATTR_VLAN_ID", "1002" } } },
        { key("SAI_OBJECT_TYPE_VLAN", temporaryVlan3),
            { { "SAI_VLAN_ATTR_VLAN_ID", "2001" } } },
        { key("SAI_OBJECT_TYPE_VLAN", temporaryVlan4),
            { { "SAI_VLAN_ATTR_VLAN_ID", "2002" } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", temporaryRif1), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(temporaryVrf1) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(temporaryVlan4) } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", temporaryRif2), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(temporaryVrf1) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(temporaryVlan3) } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", temporaryRif3), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(temporaryVrf2) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(temporaryVlan2) } } },
        { key("SAI_OBJECT_TYPE_ROUTER_INTERFACE", temporaryRif4), {
            { "SAI_ROUTER_INTERFACE_ATTR_TYPE", "SAI_ROUTER_INTERFACE_TYPE_VLAN" },
            { "SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID", sai_serialize_object_id(temporaryVrf2) },
            { "SAI_ROUTER_INTERFACE_ATTR_VLAN_ID", sai_serialize_object_id(temporaryVlan1) } } },
    };

    AsicView currentView(current);
    AsicView temporaryView(temporary);
    auto sw = std::make_shared<MockableSaiSwitchInterface>(0, 0);
    BestCandidateFinder finder(currentView, temporaryView, sw);

    auto match1 = finder.findCurrentBestMatch(temporaryView.m_oOids.at(temporaryVrf1));
    auto match2 = finder.findCurrentBestMatch(temporaryView.m_oOids.at(temporaryVrf2));

    ASSERT_NE(match1, nullptr);
    ASSERT_NE(match2, nullptr);
    EXPECT_EQ(match1->getVid(), currentVrf2);
    EXPECT_EQ(match2->getVid(), currentVrf1);
}
