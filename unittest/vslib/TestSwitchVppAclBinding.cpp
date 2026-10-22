#include "ContextConfigContainer.h"
#include "RealObjectIdManager.h"
#include "sai_serialize.h"
#include "saivs.h"
#include "vpp/SwitchVpp.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <tuple>
#include <vector>

using namespace saivs;

namespace
{
    struct BindingCall
    {
        std::string interface;
        uint32_t acl;
        bool input;
        bool bind;
    };

    std::vector<BindingCall> bindingCalls;
    using InterfaceKey = std::pair<std::string, bool>;
    using FailureKey = std::tuple<std::string, uint32_t, bool, bool>;
    std::map<InterfaceKey, std::vector<uint32_t>> actualBindings;
    std::map<FailureKey, unsigned int> bindingFailures;
    const uint32_t defaultAclIndex = 900;
    uint32_t failNextBindIndex = 0;

    bool failBinding(const char *interface, uint32_t index, bool input, bool bind)
    {
        auto it = bindingFailures.find(FailureKey(interface, index, input, bind));
        if (it == bindingFailures.end()) {
            return false;
        }
        if (--it->second == 0) {
            bindingFailures.erase(it);
        }
        return true;
    }
}

// Linker wrappers isolate only the VPP boundary. The constructors, object
// store, getter, PORT/LAG setters and ACL binding bookkeeping remain real.
extern "C" sai_status_t __wrap__ZN5saivs9SwitchVpp17vpp_dp_initializeEv(SwitchVpp *)
{
    return SAI_STATUS_SUCCESS;
}

extern "C" int __wrap_vpp_want_l2_macs_events2(bool, vpp_mac_event_cb_fn, void *)
{
    return 0;
}

extern "C" int __wrap_vpp_acl_add_replace(vpp_acl_t *, uint32_t *index, bool)
{
    *index = defaultAclIndex;
    return 0;
}

extern "C" int __wrap_vpp_acl_interface_bind(const char *interface, uint32_t index, bool input)
{
    bindingCalls.push_back({interface, index, input, true});
    if (index == failNextBindIndex) {
        failNextBindIndex = 0;
        return -1;
    }
    if (failBinding(interface, index, input, true)) {
        return -1;
    }
    auto &order = actualBindings[InterfaceKey(interface, input)];
    // acl_interface_add_del appends; its already-bound status is normalized
    // to success by the SAI boundary without changing the existing order.
    if (std::find(order.begin(), order.end(), index) == order.end()) {
        order.push_back(index);
    }
    return 0;
}

extern "C" int __wrap_vpp_acl_interface_unbind(const char *interface, uint32_t index, bool input)
{
    bindingCalls.push_back({interface, index, input, false});
    if (failBinding(interface, index, input, false)) {
        return -1;
    }
    auto &order = actualBindings[InterfaceKey(interface, input)];
    order.erase(std::remove(order.begin(), order.end(), index), order.end());
    return 0;
}

extern "C" int __wrap_vpp_sonic_ext_feature_get(const char *, bool *enabled)
{
    *enabled = false;
    return 0;
}

class SwitchVppAclBindingTest : public ::testing::TestWithParam<std::tuple<sai_object_type_t, bool>>
{
protected:
    struct Group
    {
        sai_object_id_t oid;
        sai_object_id_t table;
        sai_object_id_t member;
        uint32_t index;
    };

    void SetUp() override
    {
        bindingCalls.clear();
        actualBindings.clear();
        bindingFailures.clear();
        failNextBindIndex = 0;
        m_type = std::get<0>(GetParam());
        m_input = std::get<1>(GetParam());
        if (m_type == SAI_OBJECT_TYPE_PORT) {
            m_attr = m_input ? SAI_PORT_ATTR_INGRESS_ACL : SAI_PORT_ATTR_EGRESS_ACL;
        } else {
            m_attr = m_input ? SAI_LAG_ATTR_INGRESS_ACL : SAI_LAG_ATTR_EGRESS_ACL;
        }

        auto contexts = ContextConfigContainer::getDefault();
        auto context = contexts->get(0);
        auto config = context->m_scc->getConfig(0);
        config->m_profileMap[SAI_KEY_VS_PORT_CONFIG_FILE] = "/nonexistent/unit-test-port-config.ini";
        m_manager = std::make_shared<RealObjectIdManager>(context->m_guid, context->m_scc);
        m_switchId = m_manager->allocateNewSwitchObjectId("");
        m_switch = std::make_shared<SwitchVpp>(m_switchId, m_manager, config);

        m_port = m_manager->allocateNewObjectId(m_type, m_switchId);
        ASSERT_EQ(SAI_STATUS_SUCCESS, m_switch->create_internal(
                    m_type, sai_serialize_object_id(m_port), m_switchId, 0, nullptr));
        if (m_type == SAI_OBJECT_TYPE_PORT) {
            m_interface = "Ethernet0";
            ASSERT_NE(nullptr, m_switch->m_ifaceRegistry.addPhysicalPort(m_interface, m_interface, m_port));
        } else {
            m_interface = "BondEthernet1";
            ASSERT_NE(nullptr, m_switch->m_ifaceRegistry.addLag(1, 10, m_port));
        }
    }

    Group createGroup(uint32_t index, bool input)
    {
        Group group = {
            m_manager->allocateNewObjectId(SAI_OBJECT_TYPE_ACL_TABLE_GROUP, m_switchId),
            m_manager->allocateNewObjectId(SAI_OBJECT_TYPE_ACL_TABLE, m_switchId),
            m_manager->allocateNewObjectId(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, m_switchId),
            index
        };
        sai_attribute_t stage = {};
        stage.id = SAI_ACL_TABLE_GROUP_ATTR_ACL_STAGE;
        stage.value.s32 = input ? SAI_ACL_STAGE_INGRESS : SAI_ACL_STAGE_EGRESS;
        EXPECT_EQ(SAI_STATUS_SUCCESS, m_switch->create_internal(SAI_OBJECT_TYPE_ACL_TABLE_GROUP,
                    sai_serialize_object_id(group.oid), m_switchId, 1, &stage));
        EXPECT_EQ(SAI_STATUS_SUCCESS, m_switch->create_internal(SAI_OBJECT_TYPE_ACL_TABLE,
                    sai_serialize_object_id(group.table), m_switchId, 0, nullptr));
        // Rule compilation is independent of attachment. Supply its VPP index
        // so these tests cover attachment through the real VPP bind boundary.
        m_switch->m_acl_swindex_map[group.table] = index;
        return group;
    }

    Group createGroup(uint32_t index)
    {
        return createGroup(index, m_input);
    }

    Group createTableMember(sai_object_id_t group, uint32_t index)
    {
        Group member = {
            group,
            m_manager->allocateNewObjectId(SAI_OBJECT_TYPE_ACL_TABLE, m_switchId),
            m_manager->allocateNewObjectId(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, m_switchId),
            index
        };
        EXPECT_EQ(SAI_STATUS_SUCCESS, m_switch->create_internal(SAI_OBJECT_TYPE_ACL_TABLE,
                    sai_serialize_object_id(member.table), m_switchId, 0, nullptr));
        m_switch->m_acl_swindex_map[member.table] = index;
        return member;
    }

    sai_status_t addMember(const Group &group, uint32_t priority = 10)
    {
        sai_attribute_t attrs[3] = {};
        attrs[0].id = SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_GROUP_ID;
        attrs[0].value.oid = group.oid;
        attrs[1].id = SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_ID;
        attrs[1].value.oid = group.table;
        attrs[2].id = SAI_ACL_TABLE_GROUP_MEMBER_ATTR_PRIORITY;
        attrs[2].value.u32 = priority;
        return m_switch->createAclGrpMbr(group.member, m_switchId, 3, attrs);
    }

    sai_status_t removeMember(const Group &group)
    {
        return m_switch->remove(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER,
                sai_serialize_object_id(group.member));
    }

    std::vector<Group> createBoundThreeTableGroup()
    {
        auto first = createGroup(100);
        auto second = createTableMember(first.oid, 200);
        auto third = createTableMember(first.oid, 300);
        EXPECT_EQ(SAI_STATUS_SUCCESS, addMember(first, 30));
        EXPECT_EQ(SAI_STATUS_SUCCESS, addMember(second, 20));
        EXPECT_EQ(SAI_STATUS_SUCCESS, addMember(third, 10));
        EXPECT_EQ(SAI_STATUS_SUCCESS, setGroup(first.oid));
        return {first, second, third};
    }

    sai_object_id_t createAdditionalPort(const std::string &interface)
    {
        auto port = m_manager->allocateNewObjectId(m_type, m_switchId);
        EXPECT_EQ(SAI_STATUS_SUCCESS, m_switch->create_internal(
                    m_type, sai_serialize_object_id(port), m_switchId, 0, nullptr));
        if (m_type == SAI_OBJECT_TYPE_PORT) {
            EXPECT_NE(nullptr, m_switch->m_ifaceRegistry.addPhysicalPort(interface, interface, port));
        } else {
            EXPECT_NE(nullptr, m_switch->m_ifaceRegistry.addLag(2, 20, port));
        }
        return port;
    }

    void addBridgedChild(const std::string &interface)
    {
        auto parent = m_switch->m_ifaceRegistry.findByHwif(interface);
        ASSERT_NE(nullptr, parent);
        ASSERT_NE(nullptr, m_switch->m_ifaceRegistry.addSubInterface(parent, 100, 100));
        ASSERT_TRUE(m_switch->m_ifaceRegistry.setBdId(interface + ".100", 100));
    }

    void failUnbind(const std::string &interface, uint32_t index, unsigned int count = 1)
    {
        bindingFailures[FailureKey(interface, index, m_input, false)] = count;
    }

    void failBind(const std::string &interface, uint32_t index, unsigned int count = 1)
    {
        bindingFailures[FailureKey(interface, index, m_input, true)] = count;
    }

    void expectPolicy(const std::string &interface, const std::vector<uint32_t> &expected, bool input)
    {
        auto it = actualBindings.find(InterfaceKey(interface, input));
        const std::vector<uint32_t> empty;
        EXPECT_EQ(expected, it == actualBindings.end() ? empty : it->second);
    }

    void expectPolicy(const std::string &interface, const std::vector<uint32_t> &expected)
    {
        expectPolicy(interface, expected, m_input);
    }

    void expectMembers(sai_object_id_t group, const std::vector<Group> &expected)
    {
        auto it = m_switch->m_acl_tbl_grp_mbr_map.find(group);
        ASSERT_NE(m_switch->m_acl_tbl_grp_mbr_map.end(), it);
        std::vector<sai_object_id_t> members;
        for (const auto &member : expected) {
            members.push_back(member.member);
        }
        EXPECT_EQ(members, std::vector<sai_object_id_t>(it->second.begin(), it->second.end()));
    }

    void expectMemberObject(const Group &member, bool present)
    {
        sai_attribute_t attr = {};
        attr.id = SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_GROUP_ID;
        auto status = m_switch->get(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, member.member, 1, &attr);
        if (!present) {
            EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, status);
            return;
        }
        ASSERT_EQ(SAI_STATUS_SUCCESS, status);
        EXPECT_EQ(member.oid, attr.value.oid);
        attr.id = SAI_ACL_TABLE_GROUP_MEMBER_ATTR_ACL_TABLE_ID;
        ASSERT_EQ(SAI_STATUS_SUCCESS,
                m_switch->get(SAI_OBJECT_TYPE_ACL_TABLE_GROUP_MEMBER, member.member, 1, &attr));
        EXPECT_EQ(member.table, attr.value.oid);
    }

    sai_status_t setGroup(sai_object_id_t group, sai_object_id_t port)
    {
        sai_attribute_t attr = {};
        attr.id = m_attr;
        attr.value.oid = group;
        return m_switch->set(m_type, port, &attr);
    }

    sai_status_t setGroup(sai_object_id_t group)
    {
        return setGroup(group, m_port);
    }

    void expectStoredGroup(sai_object_id_t group, sai_object_id_t port)
    {
        sai_attribute_t attr = {};
        attr.id = m_attr;
        ASSERT_EQ(SAI_STATUS_SUCCESS, m_switch->get(m_type, port, 1, &attr));
        EXPECT_EQ(group, attr.value.oid);
    }

    void expectStoredGroup(sai_object_id_t group)
    {
        expectStoredGroup(group, m_port);
    }

    void expectAssociation(sai_object_id_t group, bool present, sai_object_id_t port)
    {
        const auto &ports = m_switch->m_acl_tbl_grp_ports_map;
        auto it = ports.find(group);
        bool found = it != ports.end() &&
            std::find(it->second.begin(), it->second.end(), port) != it->second.end();
        EXPECT_EQ(present, found);
        if (found) {
            EXPECT_EQ(1, std::count(it->second.begin(), it->second.end(), port));
        }
    }

    void expectAssociation(sai_object_id_t group, bool present)
    {
        expectAssociation(group, present, m_port);
    }

    void expectCall(size_t offset, uint32_t index, bool bind, const std::string &interface)
    {
        ASSERT_LT(offset, bindingCalls.size());
        EXPECT_EQ(interface, bindingCalls[offset].interface);
        EXPECT_EQ(index, bindingCalls[offset].acl);
        EXPECT_EQ(m_input, bindingCalls[offset].input);
        EXPECT_EQ(bind, bindingCalls[offset].bind);
    }

    void expectCall(size_t offset, uint32_t index, bool bind)
    {
        expectCall(offset, index, bind, m_interface);
    }

    std::shared_ptr<RealObjectIdManager> m_manager;
    std::shared_ptr<SwitchVpp> m_switch;
    sai_object_id_t m_switchId = SAI_NULL_OBJECT_ID;
    sai_object_id_t m_port = SAI_NULL_OBJECT_ID;
    sai_object_type_t m_type = SAI_OBJECT_TYPE_NULL;
    sai_attr_id_t m_attr = 0;
    bool m_input = false;
    std::string m_interface;
};

TEST_P(SwitchVppAclBindingTest, FirstAssignmentBindsAfterMemberCreation)
{
    sai_attribute_t attr = {};
    attr.id = m_attr;
    ASSERT_EQ(SAI_STATUS_ITEM_NOT_FOUND, m_switch->get(m_type, m_port, 1, &attr));
    auto group = createGroup(100);

    // SONiC first sets the port's group, then adds the group's table member.
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    expectStoredGroup(group.oid);
    expectAssociation(group.oid, true);
    ASSERT_TRUE(bindingCalls.empty());
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(2u, bindingCalls.size());
    expectCall(0, group.index, true);
    expectCall(1, defaultAclIndex, true);

    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    expectAssociation(group.oid, true);
    EXPECT_EQ(2u, bindingCalls.size());
}

TEST_P(SwitchVppAclBindingTest, ExplicitNullDefaultBindsAndUnbinds)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    expectStoredGroup(SAI_NULL_OBJECT_ID);
    EXPECT_TRUE(bindingCalls.empty());
    auto group = createGroup(100);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    expectStoredGroup(group.oid);
    ASSERT_EQ(2u, bindingCalls.size());

    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    expectStoredGroup(SAI_NULL_OBJECT_ID);
    expectAssociation(group.oid, false);
    ASSERT_EQ(4u, bindingCalls.size());
    expectCall(2, group.index, false);
    expectCall(3, defaultAclIndex, false);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    EXPECT_EQ(4u, bindingCalls.size());
    EXPECT_TRUE(m_switch->m_port_acl_tables.empty());
}

TEST_P(SwitchVppAclBindingTest, FirstBindingIncludesBridgedChild)
{
    auto parent = m_switch->m_ifaceRegistry.findByHwif(m_interface);
    ASSERT_NE(nullptr, parent);
    ASSERT_NE(nullptr, m_switch->m_ifaceRegistry.addSubInterface(parent, 100, 100));
    const auto child = m_interface + ".100";
    ASSERT_TRUE(m_switch->m_ifaceRegistry.setBdId(child, 100));
    auto group = createGroup(100);

    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(4u, bindingCalls.size());
    expectCall(0, group.index, true);
    expectCall(1, defaultAclIndex, true);
    expectCall(2, group.index, true, child);
    expectCall(3, defaultAclIndex, true, child);

    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    ASSERT_EQ(8u, bindingCalls.size());
    expectCall(4, group.index, false);
    expectCall(5, defaultAclIndex, false);
    expectCall(6, group.index, false, child);
    expectCall(7, defaultAclIndex, false, child);
    EXPECT_TRUE(m_switch->m_port_acl_tables.empty());
}

TEST_P(SwitchVppAclBindingTest, ReplacementMovesTheBinding)
{
    auto oldGroup = createGroup(100);
    auto newGroup = createGroup(200);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(oldGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(newGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(oldGroup.oid));
    bindingCalls.clear();

    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(newGroup.oid));
    expectStoredGroup(newGroup.oid);
    expectAssociation(oldGroup.oid, false);
    expectAssociation(newGroup.oid, true);
    ASSERT_EQ(4u, bindingCalls.size());
    expectCall(0, oldGroup.index, false);
    expectCall(1, defaultAclIndex, false);
    expectCall(2, newGroup.index, true);
    expectCall(3, defaultAclIndex, true);
}

TEST_P(SwitchVppAclBindingTest, MissingObjectsRemainErrors)
{
    auto missing = m_manager->allocateNewObjectId(m_type, m_switchId);
    auto group = createGroup(100);
    ASSERT_EQ(SAI_STATUS_ITEM_NOT_FOUND, setGroup(group.oid, missing));
    ASSERT_EQ(SAI_STATUS_ITEM_NOT_FOUND, setGroup(SAI_NULL_OBJECT_ID, missing));
    EXPECT_TRUE(bindingCalls.empty());
    EXPECT_TRUE(m_switch->m_acl_tbl_grp_ports_map.empty());
    EXPECT_EQ(0u, m_switch->m_objectHash.at(m_type).count(sai_serialize_object_id(missing)));
}

TEST_P(SwitchVppAclBindingTest, FailedInitialBindDoesNotStoreAttribute)
{
    auto group = createGroup(100);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    failNextBindIndex = group.index;
    ASSERT_EQ(SAI_STATUS_FAILURE, setGroup(group.oid));
    expectAssociation(group.oid, false);
    sai_attribute_t attr = {};
    attr.id = m_attr;
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, m_switch->get(m_type, m_port, 1, &attr));
    ASSERT_EQ(1u, bindingCalls.size());
    expectCall(0, group.index, true);
    EXPECT_TRUE(m_switch->m_port_acl_tables.empty());
}

TEST_P(SwitchVppAclBindingTest, FailedReplacementRestoresOldBinding)
{
    auto oldGroup = createGroup(100);
    auto newGroup = createGroup(200);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(oldGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(newGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(oldGroup.oid));
    bindingCalls.clear();
    failNextBindIndex = newGroup.index;

    ASSERT_EQ(SAI_STATUS_FAILURE, setGroup(newGroup.oid));
    expectStoredGroup(oldGroup.oid);
    expectAssociation(oldGroup.oid, true);
    expectAssociation(newGroup.oid, false);
    ASSERT_EQ(5u, bindingCalls.size());
    expectCall(0, oldGroup.index, false);
    expectCall(1, defaultAclIndex, false);
    expectCall(2, newGroup.index, true);
    expectCall(3, oldGroup.index, true);
    expectCall(4, defaultAclIndex, true);
}

TEST_P(SwitchVppAclBindingTest, PartialDetachKeepsPrefixDefaultAndReplayOrder)
{
    auto members = createBoundThreeTableGroup();
    auto opposite = createGroup(500, !m_input);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(opposite));
    sai_attribute_t otherAttr = {};
    if (m_type == SAI_OBJECT_TYPE_PORT) {
        otherAttr.id = m_input ? SAI_PORT_ATTR_EGRESS_ACL : SAI_PORT_ATTR_INGRESS_ACL;
    } else {
        otherAttr.id = m_input ? SAI_LAG_ATTR_EGRESS_ACL : SAI_LAG_ATTR_INGRESS_ACL;
    }
    otherAttr.value.oid = opposite.oid;
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_switch->set(m_type, m_port, &otherAttr));
    expectPolicy(m_interface, {100, 200, 300, defaultAclIndex});
    bindingCalls.clear();
    failUnbind(m_interface, 200);

    ASSERT_EQ(SAI_STATUS_FAILURE,
            m_switch->aclBindUnbindPort(m_port, members[0].oid, m_input, false));
    // Remove a low-priority suffix, then stop before either the higher
    // priority survivor or shared default can be detached.
    ASSERT_EQ(2u, bindingCalls.size());
    expectCall(0, 300, false);
    expectCall(1, 200, false);
    expectPolicy(m_interface, {100, 200, defaultAclIndex});
    expectAssociation(members[0].oid, true);

    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_switch->aclBindUnbindPort(m_port, members[0].oid, m_input, true));
    expectPolicy(m_interface, {100, 200, 300, defaultAclIndex});
    expectPolicy(m_interface, {500, defaultAclIndex}, !m_input);
    expectMembers(members[0].oid, members);
    expectStoredGroup(members[0].oid);
    for (const auto &call : bindingCalls) {
        EXPECT_EQ(m_input, call.input);
    }
}

TEST_P(SwitchVppAclBindingTest, FailedMemberRemovalPreservesPolicyAndObject)
{
    auto members = createBoundThreeTableGroup();
    failUnbind(m_interface, 200);

    ASSERT_EQ(SAI_STATUS_FAILURE, removeMember(members[1]));
    expectPolicy(m_interface, {100, 200, 300, defaultAclIndex});
    expectMembers(members[0].oid, members);
    expectAssociation(members[0].oid, true);
    expectStoredGroup(members[0].oid);
    for (const auto &member : members) {
        expectMemberObject(member, true);
    }

    ASSERT_EQ(SAI_STATUS_SUCCESS, removeMember(members[1]));
    expectPolicy(m_interface, {100, 300, defaultAclIndex});
    expectMembers(members[0].oid, {members[0], members[2]});
    expectMemberObject(members[1], false);
    expectAssociation(members[0].oid, true);
}

TEST_P(SwitchVppAclBindingTest, FailedMemberAdditionRemovesTentativeObject)
{
    auto members = createBoundThreeTableGroup();
    auto added = createTableMember(members[0].oid, 400);
    failUnbind(m_interface, 200);

    ASSERT_EQ(SAI_STATUS_FAILURE, addMember(added, 40));
    expectPolicy(m_interface, {100, 200, 300, defaultAclIndex});
    expectMembers(members[0].oid, members);
    expectMemberObject(added, false);
    expectAssociation(members[0].oid, true);
    expectStoredGroup(members[0].oid);

    // Reuse the same OID: a failed create must not leave an object that makes
    // the retry fail with ITEM_ALREADY_EXISTS.
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(added, 40));
    expectPolicy(m_interface, {400, 100, 200, 300, defaultAclIndex});
    expectMembers(members[0].oid, {members[0], members[1], members[2], added});
    expectMemberObject(added, true);
}

TEST_P(SwitchVppAclBindingTest, FailedChildDetachRestoresAllPorts)
{
    auto members = createBoundThreeTableGroup();
    const std::string secondInterface = m_type == SAI_OBJECT_TYPE_PORT ? "Ethernet4" : "BondEthernet2";
    auto secondPort = createAdditionalPort(secondInterface);
    addBridgedChild(secondInterface);
    const auto child = secondInterface + ".100";
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(members[0].oid, secondPort));
    failUnbind(child, 200);

    ASSERT_EQ(SAI_STATUS_FAILURE, removeMember(members[1]));
    // The first port and the second port's parent were already detached
    // before the failure on its child. Recovery must include all three.
    for (const auto &interface : {m_interface, secondInterface, child}) {
        expectPolicy(interface, {100, 200, 300, defaultAclIndex});
    }
    expectMembers(members[0].oid, members);
    expectMemberObject(members[1], true);
    expectAssociation(members[0].oid, true);
    expectAssociation(members[0].oid, true, secondPort);
    expectStoredGroup(members[0].oid);
    expectStoredGroup(members[0].oid, secondPort);

    ASSERT_EQ(SAI_STATUS_SUCCESS, removeMember(members[1]));
    for (const auto &interface : {m_interface, secondInterface, child}) {
        expectPolicy(interface, {100, 300, defaultAclIndex});
    }
    expectMemberObject(members[1], false);
    expectAssociation(members[0].oid, true);
    expectAssociation(members[0].oid, true, secondPort);
}

TEST_P(SwitchVppAclBindingTest, FailedDefaultDetachRestoresMemberPolicy)
{
    auto members = createBoundThreeTableGroup();
    bindingCalls.clear();
    failUnbind(m_interface, defaultAclIndex);

    ASSERT_EQ(SAI_STATUS_FAILURE, removeMember(members[1]));
    ASSERT_GE(bindingCalls.size(), 4u);
    expectCall(0, 300, false);
    expectCall(1, 200, false);
    expectCall(2, 100, false);
    expectCall(3, defaultAclIndex, false);
    expectPolicy(m_interface, {100, 200, 300, defaultAclIndex});
    expectMembers(members[0].oid, members);
    expectMemberObject(members[1], true);
    expectAssociation(members[0].oid, true);

    ASSERT_EQ(SAI_STATUS_SUCCESS, removeMember(members[1]));
    expectPolicy(m_interface, {100, 300, defaultAclIndex});
    expectMemberObject(members[1], false);
}

TEST_P(SwitchVppAclBindingTest, FailedNewMemberBindRestoresOldPolicy)
{
    auto members = createBoundThreeTableGroup();
    auto added = createTableMember(members[0].oid, 400);
    bindingCalls.clear();
    failBind(m_interface, added.index);

    ASSERT_EQ(SAI_STATUS_FAILURE, addMember(added, 25));
    ASSERT_GE(bindingCalls.size(), 7u);
    expectCall(4, 100, true);
    expectCall(5, added.index, true);
    // Stop at the failed bind, then undo the successful prefix. Lower
    // priority members must not be appended ahead of the failed member.
    expectCall(6, 100, false);
    expectPolicy(m_interface, {100, 200, 300, defaultAclIndex});
    expectMembers(members[0].oid, members);
    expectMemberObject(added, false);
    expectAssociation(members[0].oid, true);
    expectStoredGroup(members[0].oid);

    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(added, 25));
    expectPolicy(m_interface, {100, 400, 200, 300, defaultAclIndex});
    expectMemberObject(added, true);
}

TEST_P(SwitchVppAclBindingTest, RepeatedRecoveryFailuresRemainRetryable)
{
    auto members = createBoundThreeTableGroup();
    failUnbind(m_interface, 200, 2);
    failBind(m_interface, 300, 2);

    for (unsigned int attempt = 0; attempt < 2; ++attempt) {
        SCOPED_TRACE(attempt);
        ASSERT_EQ(SAI_STATUS_FAILURE, removeMember(members[1]));
        // Recovery also failed, so preserve truthful partial bindings and
        // desired membership. Neither attempt may be reported as success.
        expectPolicy(m_interface, {100, 200, defaultAclIndex});
        expectMembers(members[0].oid, members);
        expectMemberObject(members[1], true);
        expectAssociation(members[0].oid, true);
        expectStoredGroup(members[0].oid);
    }
    EXPECT_TRUE(bindingFailures.empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, removeMember(members[1]));
    expectPolicy(m_interface, {100, 300, defaultAclIndex});
    expectMembers(members[0].oid, {members[0], members[2]});
    expectMemberObject(members[1], false);
    expectAssociation(members[0].oid, true);
}

TEST_P(SwitchVppAclBindingTest, NullClearRemovesTentativeTableAfterFailedRollback)
{
    auto members = createBoundThreeTableGroup();
    const std::string secondInterface = m_type == SAI_OBJECT_TYPE_PORT ? "Ethernet4" : "BondEthernet2";
    auto secondPort = createAdditionalPort(secondInterface);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(members[0].oid, secondPort));
    auto added = createTableMember(members[0].oid, 400);
    failBind(secondInterface, added.index);
    // The first port accepts the tentative policy. Both undoing that policy
    // and restoring the old policy then fail to remove its extra table.
    failUnbind(m_interface, added.index, 2);

    ASSERT_EQ(SAI_STATUS_FAILURE, addMember(added, 25));
    expectPolicy(m_interface, {100, 400, defaultAclIndex});
    expectPolicy(secondInterface, {100, 200, 300, defaultAclIndex});
    expectMembers(members[0].oid, members);
    expectMemberObject(added, false);
    expectAssociation(members[0].oid, true);
    expectAssociation(members[0].oid, true, secondPort);
    EXPECT_TRUE(bindingFailures.empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    expectPolicy(m_interface, {});
    expectPolicy(secondInterface, {100, 200, 300, defaultAclIndex});
    expectStoredGroup(SAI_NULL_OBJECT_ID);
    expectAssociation(members[0].oid, false);
    // Rebinding the old group must not leave the removed tentative table in
    // front of its desired members, even though that table is no longer a member.
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(members[0].oid));
    expectPolicy(m_interface, {100, 200, 300, defaultAclIndex});
    expectAssociation(members[0].oid, true);
}

TEST_P(SwitchVppAclBindingTest, EmptyOldGroupCleansFailedFirstMemberRollback)
{
    auto group = createGroup(100);
    const std::string secondInterface = m_type == SAI_OBJECT_TYPE_PORT ? "Ethernet4" : "BondEthernet2";
    auto secondPort = createAdditionalPort(secondInterface);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid, secondPort));
    failBind(secondInterface, group.index);
    failUnbind(m_interface, group.index, 2);

    ASSERT_EQ(SAI_STATUS_FAILURE, addMember(group));
    expectPolicy(m_interface, {group.index, defaultAclIndex});
    expectPolicy(secondInterface, {});
    EXPECT_EQ(0u, m_switch->m_acl_tbl_grp_mbr_map.count(group.oid));
    expectMemberObject(group, false);
    expectAssociation(group.oid, true);
    expectAssociation(group.oid, true, secondPort);
    EXPECT_TRUE(bindingFailures.empty());

    // There is no old member list to enumerate. Clearing the SAI attachment
    // must nevertheless remove the actually programmed table and permit.
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID, secondPort));
    expectPolicy(m_interface, {});
    expectPolicy(secondInterface, {});
    expectAssociation(group.oid, false);
    expectAssociation(group.oid, false, secondPort);

    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid, secondPort));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    expectPolicy(m_interface, {group.index, defaultAclIndex});
    expectPolicy(secondInterface, {group.index, defaultAclIndex});
    expectMembers(group.oid, {group});
    expectMemberObject(group, true);
}

TEST_P(SwitchVppAclBindingTest, FailedReplacementDoesNotLeaveNewGroupAssociated)
{
    auto oldGroup = createGroup(100);
    auto newGroup = createGroup(200);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(oldGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(newGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(oldGroup.oid));
    failBind(m_interface, defaultAclIndex);
    failUnbind(m_interface, newGroup.index);

    // The new real ACL binds, but its default and its first rollback detach
    // fail. Restoring the old group must remove both the actual new ACL and
    // its desired port association.
    ASSERT_EQ(SAI_STATUS_FAILURE, setGroup(newGroup.oid));
    expectPolicy(m_interface, {oldGroup.index, defaultAclIndex});
    expectStoredGroup(oldGroup.oid);
    expectAssociation(oldGroup.oid, true);
    expectAssociation(newGroup.oid, false);
    EXPECT_TRUE(bindingFailures.empty());

    // A later update to the unbound new group must not replace this port's
    // restored old policy through an association left by the failed SET.
    auto added = createTableMember(newGroup.oid, 300);
    bindingCalls.clear();
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(added));
    EXPECT_TRUE(bindingCalls.empty());
    expectMembers(newGroup.oid, {newGroup, added});
    expectPolicy(m_interface, {oldGroup.index, defaultAclIndex});
    expectStoredGroup(oldGroup.oid);
    expectAssociation(oldGroup.oid, true);
    expectAssociation(newGroup.oid, false);
}

TEST_P(SwitchVppAclBindingTest, NullClearRepairsFailedInitialAssignmentRollback)
{
    auto group = createGroup(100);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    failBind(m_interface, defaultAclIndex);
    // The table is installed, but its permit fails. Both the initial undo
    // and restoration of the old null policy fail to remove the table.
    failUnbind(m_interface, group.index, 2);

    ASSERT_EQ(SAI_STATUS_FAILURE, setGroup(group.oid));
    expectPolicy(m_interface, {group.index});
    expectAssociation(group.oid, false);
    sai_attribute_t attr = {};
    attr.id = m_attr;
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, m_switch->get(m_type, m_port, 1, &attr));
    EXPECT_TRUE(bindingFailures.empty());

    // The stored value is already null, but the failed operation left real
    // attachments behind. An explicit null assignment must reconcile them.
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    expectPolicy(m_interface, {});
    expectStoredGroup(SAI_NULL_OBJECT_ID);
    expectAssociation(group.oid, false);
    EXPECT_TRUE(m_switch->m_port_acl_tables.empty());
}

INSTANTIATE_TEST_SUITE_P(PortAndLagDirections, SwitchVppAclBindingTest,
        ::testing::Values(std::make_tuple(SAI_OBJECT_TYPE_PORT, true),
                          std::make_tuple(SAI_OBJECT_TYPE_PORT, false),
                          std::make_tuple(SAI_OBJECT_TYPE_LAG, true),
                          std::make_tuple(SAI_OBJECT_TYPE_LAG, false)));
