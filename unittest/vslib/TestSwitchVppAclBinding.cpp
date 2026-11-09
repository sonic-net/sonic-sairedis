#include "ContextConfigContainer.h"
#include "RealObjectIdManager.h"
#include "sai_serialize.h"
#include "saivs.h"
#include "vpp/SwitchVpp.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <map>
#include <set>
#include <tuple>
#include <vector>

using namespace saivs;

namespace
{
    using InterfaceKey = std::pair<std::string, bool>;
    struct BindingCall
    {
        std::string interface;
        std::vector<uint32_t> ingress;
        std::vector<uint32_t> egress;
        bool succeeded;
    };
    struct ListFailure
    {
        std::string interface;
        bool input;
        std::vector<uint32_t> policy;
        unsigned int remaining;
        bool commitBeforeFailure;
    };
    using TuntermKey = std::tuple<std::string, uint32_t, bool>;
    std::vector<BindingCall> bindingCalls;
    std::vector<ListFailure> listFailures;
    std::map<InterfaceKey, std::vector<uint32_t>> actualBindings;
    std::map<TuntermKey, unsigned int> tuntermFailures;
    std::map<std::pair<std::string, uint32_t>, bool> actualTunterm;
    std::vector<TuntermKey> tuntermCalls;
    std::map<std::string, std::set<uint32_t>> actualPbh;
    std::vector<TuntermKey> pbhCalls;
    std::map<TuntermKey, unsigned int> pbhFailures;
    std::map<std::string, unsigned int> readFailures;
    std::vector<std::string> readCalls;
    const uint32_t defaultAclIndex = 900;
}

// Only the VPP boundary is substituted. Object storage, getters, PORT/LAG
// setters, group-member lifecycle and attachment bookkeeping remain real.
extern "C" sai_status_t __wrap__ZN5saivs9SwitchVpp17vpp_dp_initializeEv(SwitchVpp *)
{
    return SAI_STATUS_SUCCESS;
}

extern "C" void __wrap_vpp_set_mac_event_cb(vpp_mac_event_cb_fn, void *)
{
}

extern "C" int __wrap_vpp_acl_add_replace(vpp_acl_t *, uint32_t *index, bool)
{
    *index = defaultAclIndex;
    return 0;
}

extern "C" int __wrap_vpp_acl_interface_set_acl_list(const char *interface,
        const uint32_t *ingress, uint32_t ingressCount,
        const uint32_t *egress, uint32_t egressCount)
{
    BindingCall call{interface, {}, {}, false};
    if (ingressCount) call.ingress.assign(ingress, ingress + ingressCount);
    if (egressCount) call.egress.assign(egress, egress + egressCount);
    bool fail = ingressCount + egressCount > 255;
    bool commitBeforeFailure = false;
    bool commitInput = false;
    for (auto it = listFailures.begin(); it != listFailures.end(); ++it) {
        if (it->interface == interface &&
                it->policy == (it->input ? call.ingress : call.egress)) {
            fail = true;
            commitBeforeFailure = it->commitBeforeFailure;
            commitInput = it->input;
            if (--it->remaining == 0) listFailures.erase(it);
            break;
        }
    }
    call.succeeded = !fail;
    bindingCalls.push_back(call);
    if (fail && !commitBeforeFailure) return -1;
    if (fail) {
        // Simulate a partially applied request or a lost error reply: one
        // direction changed, so failure must trigger readback before rollback.
        actualBindings[InterfaceKey(interface, commitInput)] =
            commitInput ? call.ingress : call.egress;
        return -1;
    }
    // A successful set replaces both directions, never appends individual ACLs.
    actualBindings[InterfaceKey(interface, true)] = call.ingress;
    actualBindings[InterfaceKey(interface, false)] = call.egress;
    return 0;
}

extern "C" int __wrap_vpp_acl_interface_get_acl_list(const char *interface,
        uint32_t *ingress, uint32_t *ingressCount, uint32_t *egress, uint32_t *egressCount)
{
    readCalls.push_back(interface);
    auto failure = readFailures.find(interface);
    if (failure != readFailures.end()) {
        if (--failure->second == 0) readFailures.erase(failure);
        return -1;
    }
    const auto &in = actualBindings[InterfaceKey(interface, true)];
    const auto &out = actualBindings[InterfaceKey(interface, false)];
    std::copy(in.begin(), in.end(), ingress);
    std::copy(out.begin(), out.end(), egress);
    *ingressCount = static_cast<uint32_t>(in.size());
    *egressCount = static_cast<uint32_t>(out.size());
    return 0;
}

extern "C" int __wrap_vpp_acl_interface_bind(const char *, uint32_t, bool)
{
    ADD_FAILURE() << "Individual bind used instead of complete ACL list";
    return -1;
}

extern "C" int __wrap_vpp_acl_interface_unbind(const char *, uint32_t, bool)
{
    ADD_FAILURE() << "Individual unbind used instead of complete ACL list";
    return -1;
}

extern "C" int __wrap_vpp_tunterm_acl_interface_add_del(uint32_t index,
        bool bind, const char *interface)
{
    const auto key = TuntermKey(interface, index, bind);
    tuntermCalls.push_back(key);
    auto it = tuntermFailures.find(key);
    if (it != tuntermFailures.end()) {
        if (--it->second == 0) tuntermFailures.erase(it);
        return -1;
    }
    actualTunterm[{interface, index}] = bind;
    return 0;
}

extern "C" int __wrap_vpp_pbh_interface_attach_detach(const char *interface,
        uint32_t index, bool bind)
{
    const auto key = TuntermKey(interface, index, bind);
    pbhCalls.push_back(key);
    auto failure = pbhFailures.find(key);
    if (failure != pbhFailures.end()) {
        if (--failure->second == 0) pbhFailures.erase(failure);
        return -1;
    }
    if (bind) {
        // The real PBH plugin has one table slot: attaching replaces it.
        actualPbh[interface] = {index};
    } else if (!actualPbh[interface].erase(index)) {
        return -1;
    }
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
        listFailures.clear();
        tuntermFailures.clear();
        actualTunterm.clear();
        tuntermCalls.clear();
        actualPbh.clear();
        pbhCalls.clear();
        pbhFailures.clear();
        readFailures.clear();
        readCalls.clear();
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

    void markPbh(const Group &group)
    {
        // As for regular ACLs, supply an already compiled table index. The
        // real PBH classification and bind/unbind policy remain in use.
        m_switch->m_acl_swindex_map.erase(group.table);
        m_switch->m_pbh.m_tables.insert(group.table);
        m_switch->m_pbh.m_table_index_map[group.table] = group.index;
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

    void addBridgedChild(const std::string &interface, uint32_t vlan = 100)
    {
        auto parent = m_switch->m_ifaceRegistry.findByHwif(interface);
        ASSERT_NE(nullptr, parent);
        ASSERT_NE(nullptr, m_switch->m_ifaceRegistry.addSubInterface(parent, vlan,
                    static_cast<uint16_t>(vlan)));
        ASSERT_TRUE(m_switch->m_ifaceRegistry.setBdId(interface + "." + std::to_string(vlan), vlan));
    }

    void failList(const std::string &interface, const std::vector<uint32_t> &policy,
            unsigned int count = 1, bool commitBeforeFailure = false)
    {
        listFailures.push_back({interface, m_input, policy, count, commitBeforeFailure});
    }

    void failTunterm(const std::string &interface, uint32_t index, bool bind,
            unsigned int count = 1)
    {
        tuntermFailures[TuntermKey(interface, index, bind)] = count;
    }

    void setOppositeGroup(sai_object_id_t group)
    {
        ASSERT_EQ(SAI_STATUS_SUCCESS, setOppositeGroupStatus(group));
    }

    sai_status_t setOppositeGroupStatus(sai_object_id_t group)
    {
        sai_attribute_t attr = {};
        if (m_type == SAI_OBJECT_TYPE_PORT) {
            attr.id = m_input ? SAI_PORT_ATTR_EGRESS_ACL : SAI_PORT_ATTR_INGRESS_ACL;
        } else {
            attr.id = m_input ? SAI_LAG_ATTR_EGRESS_ACL : SAI_LAG_ATTR_INGRESS_ACL;
        }
        attr.value.oid = group;
        return m_switch->set(m_type, m_port, &attr);
    }

    const std::vector<uint32_t> &policy(const BindingCall &call) const
    {
        return m_input ? call.ingress : call.egress;
    }

    void expectNoEmptyListUpdate()
    {
        for (const auto &call : bindingCalls) {
            EXPECT_FALSE(policy(call).empty()) << call.interface;
            EXPECT_EQ(defaultAclIndex, policy(call).empty() ? 0 : policy(call).back());
        }
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

    void expectCall(size_t offset, const std::vector<uint32_t> &expected,
            const std::string &interface)
    {
        ASSERT_LT(offset, bindingCalls.size());
        EXPECT_EQ(interface, bindingCalls[offset].interface);
        EXPECT_EQ(expected, policy(bindingCalls[offset]));
    }

    void expectCall(size_t offset, const std::vector<uint32_t> &expected)
    {
        expectCall(offset, expected, m_interface);
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
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    expectStoredGroup(group.oid);
    expectAssociation(group.oid, true);
    EXPECT_TRUE(bindingCalls.empty());

    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(1u, bindingCalls.size());
    expectCall(0, {100, defaultAclIndex});
    expectPolicy(m_interface, {100, defaultAclIndex});
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    EXPECT_EQ(1u, bindingCalls.size());
}

TEST_P(SwitchVppAclBindingTest, ExplicitNullDefaultBindsAndClearsCompleteList)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    expectStoredGroup(SAI_NULL_OBJECT_ID);
    EXPECT_TRUE(bindingCalls.empty());
    auto group = createGroup(100);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    ASSERT_EQ(2u, bindingCalls.size());
    expectCall(0, {100, defaultAclIndex});
    expectCall(1, {});
    expectPolicy(m_interface, {});
    expectStoredGroup(SAI_NULL_OBJECT_ID);
    expectAssociation(group.oid, false);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    EXPECT_EQ(2u, bindingCalls.size());
    EXPECT_TRUE(m_switch->m_port_acl_tables.empty());
}

TEST_P(SwitchVppAclBindingTest, FirstBindingIncludesBridgedChild)
{
    addBridgedChild(m_interface);
    const auto child = m_interface + ".100";
    auto group = createGroup(100);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(2u, bindingCalls.size());
    expectCall(0, {100, defaultAclIndex});
    expectCall(1, {100, defaultAclIndex}, child);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    ASSERT_EQ(4u, bindingCalls.size());
    expectCall(2, {});
    expectCall(3, {}, child);
    EXPECT_TRUE(m_switch->m_port_acl_tables.empty());
}

TEST_P(SwitchVppAclBindingTest, ReplacementSubmitsOneOrderedList)
{
    auto oldGroup = createGroup(100);
    auto newGroup = createGroup(200);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(oldGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(newGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(oldGroup.oid));
    bindingCalls.clear();
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(newGroup.oid));
    ASSERT_EQ(1u, bindingCalls.size());
    expectCall(0, {200, defaultAclIndex});
    expectStoredGroup(newGroup.oid);
    expectAssociation(oldGroup.oid, false);
    expectAssociation(newGroup.oid, true);
    expectNoEmptyListUpdate();
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

TEST_P(SwitchVppAclBindingTest, FailedInitialListDoesNotStoreAttribute)
{
    auto group = createGroup(100);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    failList(m_interface, {100, defaultAclIndex});
    ASSERT_EQ(SAI_STATUS_FAILURE, setGroup(group.oid));
    expectPolicy(m_interface, {});
    expectAssociation(group.oid, false);
    sai_attribute_t attr = {};
    attr.id = m_attr;
    EXPECT_EQ(SAI_STATUS_ITEM_NOT_FOUND, m_switch->get(m_type, m_port, 1, &attr));
    ASSERT_FALSE(bindingCalls.empty());
    EXPECT_FALSE(bindingCalls.front().succeeded);
    EXPECT_FALSE(readCalls.empty());
    EXPECT_TRUE(m_switch->m_port_acl_tables.empty());
}

TEST_P(SwitchVppAclBindingTest, FailedReplacementKeepsOldPolicyAndOwner)
{
    auto oldGroup = createGroup(100);
    auto newGroup = createGroup(200);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(oldGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(newGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(oldGroup.oid));
    bindingCalls.clear();
    failList(m_interface, {200, defaultAclIndex});
    ASSERT_EQ(SAI_STATUS_FAILURE, setGroup(newGroup.oid));
    expectPolicy(m_interface, {100, defaultAclIndex});
    expectStoredGroup(oldGroup.oid);
    expectAssociation(oldGroup.oid, true);
    expectAssociation(newGroup.oid, false);
    expectNoEmptyListUpdate();
}

TEST_P(SwitchVppAclBindingTest, MemberInsertionPreservesPriorityDefaultAndOppositeDirection)
{
    auto members = createBoundThreeTableGroup();
    auto opposite = createGroup(500, !m_input);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(opposite));
    setOppositeGroup(opposite.oid);
    auto added = createTableMember(members[0].oid, 400);
    bindingCalls.clear();
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(added, 25));
    ASSERT_EQ(1u, bindingCalls.size());
    expectCall(0, {100, 400, 200, 300, defaultAclIndex});
    const auto &oppositeList = m_input ? bindingCalls[0].egress : bindingCalls[0].ingress;
    EXPECT_EQ((std::vector<uint32_t>{500, defaultAclIndex}), oppositeList);
    expectPolicy(m_interface, {500, defaultAclIndex}, !m_input);
    expectNoEmptyListUpdate();
}

TEST_P(SwitchVppAclBindingTest, MemberRemovalDoesNotClearPolicyFirst)
{
    auto members = createBoundThreeTableGroup();
    bindingCalls.clear();
    ASSERT_EQ(SAI_STATUS_SUCCESS, removeMember(members[1]));
    ASSERT_EQ(1u, bindingCalls.size());
    expectCall(0, {100, 300, defaultAclIndex});
    expectMembers(members[0].oid, {members[0], members[2]});
    expectMemberObject(members[1], false);
    expectNoEmptyListUpdate();
}

TEST_P(SwitchVppAclBindingTest, FailedMemberRemovalPreservesPolicyAndObject)
{
    auto members = createBoundThreeTableGroup();
    failList(m_interface, {100, 300, defaultAclIndex});
    ASSERT_EQ(SAI_STATUS_FAILURE, removeMember(members[1]));
    expectPolicy(m_interface, {100, 200, 300, defaultAclIndex});
    expectMembers(members[0].oid, members);
    expectMemberObject(members[1], true);
    expectAssociation(members[0].oid, true);
    expectStoredGroup(members[0].oid);
    expectNoEmptyListUpdate();
    ASSERT_EQ(SAI_STATUS_SUCCESS, removeMember(members[1]));
    expectPolicy(m_interface, {100, 300, defaultAclIndex});
    expectMemberObject(members[1], false);
}

TEST_P(SwitchVppAclBindingTest, FailedMemberAdditionRemovesTentativeObject)
{
    auto members = createBoundThreeTableGroup();
    auto added = createTableMember(members[0].oid, 400);
    failList(m_interface, {400, 100, 200, 300, defaultAclIndex});
    ASSERT_EQ(SAI_STATUS_FAILURE, addMember(added, 40));
    expectPolicy(m_interface, {100, 200, 300, defaultAclIndex});
    expectMembers(members[0].oid, members);
    expectMemberObject(added, false);
    expectAssociation(members[0].oid, true);
    expectNoEmptyListUpdate();
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(added, 40));
    expectPolicy(m_interface, {400, 100, 200, 300, defaultAclIndex});
    expectMemberObject(added, true);
}

TEST_P(SwitchVppAclBindingTest, FailedChildUpdateRestoresAllPorts)
{
    auto members = createBoundThreeTableGroup();
    const std::string second = m_type == SAI_OBJECT_TYPE_PORT ? "Ethernet4" : "BondEthernet2";
    auto secondPort = createAdditionalPort(second);
    addBridgedChild(second);
    const auto child = second + ".100";
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(members[0].oid, secondPort));
    bindingCalls.clear();
    failList(child, {100, 300, defaultAclIndex});
    ASSERT_EQ(SAI_STATUS_FAILURE, removeMember(members[1]));
    for (const auto &interface : {m_interface, second, child}) {
        expectPolicy(interface, {100, 200, 300, defaultAclIndex});
    }
    expectMembers(members[0].oid, members);
    expectMemberObject(members[1], true);
    expectAssociation(members[0].oid, true, secondPort);
    expectNoEmptyListUpdate();
    ASSERT_EQ(SAI_STATUS_SUCCESS, removeMember(members[1]));
    for (const auto &interface : {m_interface, second, child}) {
        expectPolicy(interface, {100, 300, defaultAclIndex});
    }
    expectMemberObject(members[1], false);
}

TEST_P(SwitchVppAclBindingTest, FailedRecoveryKeepsTruthfulPolicyAndRetries)
{
    auto members = createBoundThreeTableGroup();
    const std::string second = m_type == SAI_OBJECT_TYPE_PORT ? "Ethernet4" : "BondEthernet2";
    auto secondPort = createAdditionalPort(second);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(members[0].oid, secondPort));
    failList(second, {100, 300, defaultAclIndex});
    failList(m_interface, {100, 200, 300, defaultAclIndex});
    ASSERT_EQ(SAI_STATUS_FAILURE, removeMember(members[1]));
    // The first target accepted the change, then rejected recovery. Do not
    // claim its old list was restored just because logical membership was.
    expectPolicy(m_interface, {100, 300, defaultAclIndex});
    expectPolicy(second, {100, 200, 300, defaultAclIndex});
    expectMembers(members[0].oid, members);
    expectMemberObject(members[1], true);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(members[0].oid));
    expectPolicy(m_interface, {100, 200, 300, defaultAclIndex});
    expectAssociation(members[0].oid, true);
}

TEST_P(SwitchVppAclBindingTest, RejectedReplacementCannotAcquirePolicyOnLaterMemberChange)
{
    auto oldGroup = createGroup(100);
    auto rejected = createGroup(200);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(oldGroup));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(rejected));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(oldGroup.oid));
    failList(m_interface, {200, defaultAclIndex}, 1, true);
    ASSERT_EQ(SAI_STATUS_FAILURE, setGroup(rejected.oid));
    expectPolicy(m_interface, {100, defaultAclIndex});
    expectStoredGroup(oldGroup.oid);
    expectAssociation(oldGroup.oid, true);
    expectAssociation(rejected.oid, false);
    auto added = createTableMember(rejected.oid, 300);
    bindingCalls.clear();
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(added));
    EXPECT_TRUE(bindingCalls.empty());
    expectPolicy(m_interface, {100, defaultAclIndex});
}

TEST_P(SwitchVppAclBindingTest, FailedClearIsAtomicAndPreservesOppositeDirection)
{
    auto group = createGroup(100);
    auto opposite = createGroup(500, !m_input);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(opposite));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    setOppositeGroup(opposite.oid);
    failList(m_interface, {});
    ASSERT_EQ(SAI_STATUS_FAILURE, setGroup(SAI_NULL_OBJECT_ID));
    expectPolicy(m_interface, {100, defaultAclIndex});
    expectPolicy(m_interface, {500, defaultAclIndex}, !m_input);
    expectStoredGroup(group.oid);
    expectAssociation(group.oid, true);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    expectPolicy(m_interface, {});
    expectPolicy(m_interface, {500, defaultAclIndex}, !m_input);
}

TEST_P(SwitchVppAclBindingTest, ReadbackRepairsSetErrorAfterPolicyWasApplied)
{
    auto group = createGroup(100);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    failList(m_interface, {100, defaultAclIndex}, 1, true);
    ASSERT_EQ(SAI_STATUS_FAILURE, setGroup(group.oid));
    EXPECT_FALSE(readCalls.empty());
    expectPolicy(m_interface, {});
    expectAssociation(group.oid, false);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    expectPolicy(m_interface, {100, defaultAclIndex});
    expectStoredGroup(group.oid);
}

TEST_P(SwitchVppAclBindingTest, FailedReadbackRequiresRefreshBeforePreservingOtherDirection)
{
    auto group = createGroup(100);
    auto opposite = createGroup(500, !m_input);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(opposite));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    // Model a lost error reply after VPP applied the requested list. Both
    // readback and immediate recovery refresh fail; later retry must refresh.
    auto replacement = createGroup(200);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(replacement));
    failList(m_interface, {200, defaultAclIndex}, 1, true);
    readFailures[m_interface] = 100;
    ASSERT_EQ(SAI_STATUS_FAILURE, setGroup(replacement.oid));
    EXPECT_FALSE(readCalls.empty());
    expectStoredGroup(group.oid);
    expectAssociation(replacement.oid, false);
    expectPolicy(m_interface, {200, defaultAclIndex});
    readFailures.clear();
    const auto beforeOppositeUpdate = actualBindings[InterfaceKey(m_interface, m_input)];
    const auto readsBefore = readCalls.size();
    setOppositeGroup(opposite.oid);
    // Setting the opposite direction must preserve the actual list returned
    // by the refreshed VPP read, rather than a stale pre-error snapshot.
    expectPolicy(m_interface, beforeOppositeUpdate);
    EXPECT_GT(readCalls.size(), readsBefore);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    expectPolicy(m_interface, {100, defaultAclIndex});
    expectPolicy(m_interface, {500, defaultAclIndex}, !m_input);
}

TEST_P(SwitchVppAclBindingTest, AncillaryUnbindFailureIsRetainedAndRetried)
{
    auto group = createGroup(100);
    m_switch->m_tunterm_acl_swindex_map[group.table] = 700;
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    ASSERT_TRUE(actualTunterm[std::make_pair(m_interface, 700)]);
    failTunterm(m_interface, 700, false);
    ASSERT_EQ(SAI_STATUS_FAILURE,
            m_switch->aclBindUnbindPort(m_port, group.oid, m_input, false));
    expectPolicy(m_interface, {});
    EXPECT_TRUE(actualTunterm[std::make_pair(m_interface, 700)]);
    EXPECT_FALSE(m_switch->m_acl_tunterm_pending.empty());
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    EXPECT_FALSE(actualTunterm[std::make_pair(m_interface, 700)]);
    EXPECT_TRUE(m_switch->m_acl_tunterm_pending.empty());
    expectStoredGroup(SAI_NULL_OBJECT_ID);
    expectAssociation(group.oid, false);
}

TEST_P(SwitchVppAclBindingTest, AncillaryBindFailureRollsBackMemberAndFullList)
{
    auto members = createBoundThreeTableGroup();
    auto added = createTableMember(members[0].oid, 400);
    m_switch->m_tunterm_acl_swindex_map[added.table] = 700;
    failTunterm(m_interface, 700, true);
    ASSERT_EQ(SAI_STATUS_FAILURE, addMember(added, 25));
    expectPolicy(m_interface, {100, 200, 300, defaultAclIndex});
    expectMembers(members[0].oid, members);
    expectMemberObject(added, false);
    EXPECT_TRUE(m_switch->m_acl_tunterm_pending.empty());
    EXPECT_FALSE(actualTunterm[std::make_pair(m_interface, 700)]);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(added, 25));
    expectPolicy(m_interface, {100, 400, 200, 300, defaultAclIndex});
    EXPECT_TRUE(actualTunterm[std::make_pair(m_interface, 700)]);
}

TEST_P(SwitchVppAclBindingTest, NewChildReceivesCompleteOrderedPolicy)
{
    createBoundThreeTableGroup();
    auto opposite = createGroup(500, !m_input);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(opposite));
    setOppositeGroup(opposite.oid);
    addBridgedChild(m_interface);
    const auto child = m_interface + ".100";
    bindingCalls.clear();
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_switch->aclInheritInterfacePolicy(m_interface, child));
    ASSERT_EQ(1u, bindingCalls.size());
    expectCall(0, {100, 200, 300, defaultAclIndex}, child);
    expectPolicy(child, {100, 200, 300, defaultAclIndex});
    expectPolicy(child, {500, defaultAclIndex}, !m_input);
    expectNoEmptyListUpdate();
}

TEST_P(SwitchVppAclBindingTest, FailedChildInheritanceClearsUnverifiedLists)
{
    createBoundThreeTableGroup();
    addBridgedChild(m_interface);
    const auto child = m_interface + ".100";
    failList(child, {100, 200, 300, defaultAclIndex}, 1, true);
    readFailures[child] = 100;
    ASSERT_EQ(SAI_STATUS_FAILURE, m_switch->aclInheritInterfacePolicy(m_interface, child));
    // Full child teardown can replace both directions with empty lists even
    // when a failed dump prevents a safe direction-only update.
    expectPolicy(child, {});
    expectPolicy(child, {}, !m_input);
    EXPECT_EQ(0u, m_switch->m_acl_interface_state_unknown.count(child));
    expectPolicy(m_interface, {100, 200, 300, defaultAclIndex});
    EXPECT_FALSE(readCalls.empty());
}

TEST_P(SwitchVppAclBindingTest, CombinedListLimitIncludesBothDefaultPermits)
{
    auto group = createGroup(1000);
    auto opposite = createGroup(2000, !m_input);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(opposite));
    for (uint32_t i = 1; i < 126; ++i) {
        ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(createTableMember(group.oid, 1000 + i)));
    }
    for (uint32_t i = 1; i < 127; ++i) {
        ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(createTableMember(opposite.oid, 2000 + i)));
    }
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    setOppositeGroup(opposite.oid);
    const auto before = actualBindings;
    ASSERT_FALSE(bindingCalls.empty());
    EXPECT_EQ(255u, bindingCalls.back().ingress.size() + bindingCalls.back().egress.size());
    auto excess = createTableMember(group.oid, 3000);
    const auto callsBefore = bindingCalls.size();
    ASSERT_NE(SAI_STATUS_SUCCESS, addMember(excess));
    EXPECT_EQ(callsBefore, bindingCalls.size());
    EXPECT_EQ(before, actualBindings);
    expectMemberObject(excess, false);
    expectStoredGroup(group.oid);
    expectAssociation(group.oid, true);
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    expectPolicy(m_interface, {});
    EXPECT_EQ(128u, actualBindings[InterfaceKey(m_interface, !m_input)].size());
}

TEST_P(SwitchVppAclBindingTest, ReplacementRemovesOldRegularAndPbhAttachments)
{
    auto regular = createGroup(100);
    auto pbh = createGroup(700);
    markPbh(pbh);
    auto opposite = createGroup(500, !m_input);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(regular));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(pbh));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(opposite));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(regular.oid));
    setOppositeGroup(opposite.oid);

    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(pbh.oid));
    expectPolicy(m_interface, {});
    expectPolicy(m_interface, {500, defaultAclIndex}, !m_input);
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[m_interface]);
    expectStoredGroup(pbh.oid);
    expectAssociation(regular.oid, false);
    expectAssociation(pbh.oid, true);

    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(regular.oid));
    expectPolicy(m_interface, {100, defaultAclIndex});
    expectPolicy(m_interface, {500, defaultAclIndex}, !m_input);
    EXPECT_TRUE(actualPbh[m_interface].empty());
    expectAssociation(pbh.oid, false);
    expectAssociation(regular.oid, true);
}

TEST_P(SwitchVppAclBindingTest, PbhMemberChangesLeaveRegularPolicyAttached)
{
    auto regular = createGroup(100);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(regular));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(regular.oid));
    auto pbh = createTableMember(regular.oid, 700);
    markPbh(pbh);
    bindingCalls.clear();
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(pbh));
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[m_interface]);
    expectPolicy(m_interface, {100, defaultAclIndex});
    ASSERT_EQ(SAI_STATUS_SUCCESS, removeMember(pbh));
    EXPECT_TRUE(actualPbh[m_interface].empty());
    expectPolicy(m_interface, {100, defaultAclIndex});
    expectMemberObject(pbh, false);
    expectNoEmptyListUpdate();
}

TEST_P(SwitchVppAclBindingTest, PbhOnlyOwnerRetriesIndependentRegularSnapshot)
{
    auto independent = createGroup(100);
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_switch->aclBindInterfaceTable(m_interface, independent.table, m_input, true));
    auto pbh = createGroup(700);
    markPbh(pbh);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(pbh));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(pbh.oid));
    auto opposite = createGroup(500, !m_input);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(opposite));
    setOppositeGroup(opposite.oid);
    auto replacement = createGroup(200);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(replacement));

    for (bool clear : {false, true}) {
        SCOPED_TRACE(clear);
        failList(m_interface, {200, defaultAclIndex}, 1, true);
        // Preserve the failed snapshot across the whole unsuccessful setter,
        // rather than letting an incidental second recovery attempt repair it.
        failList(m_interface, {100, defaultAclIndex}, 100);
        ASSERT_EQ(SAI_STATUS_FAILURE, setGroup(replacement.oid));
        expectPolicy(m_interface, {200, defaultAclIndex});
        expectPolicy(m_interface, {500, defaultAclIndex}, !m_input);
        expectStoredGroup(pbh.oid);
        expectAssociation(pbh.oid, true);
        expectAssociation(replacement.oid, false);
        EXPECT_FALSE(m_switch->m_acl_pbh_regular_restore.empty());

        listFailures.clear();
        ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(clear ? SAI_NULL_OBJECT_ID : pbh.oid));
        // The PBH-only owner did not own this regular list. Both retry and
        // explicit PBH clear must restore the saved list, not leave table 200.
        expectPolicy(m_interface, {100, defaultAclIndex});
        expectPolicy(m_interface, {500, defaultAclIndex}, !m_input);
        EXPECT_TRUE(m_switch->m_acl_pbh_regular_restore.empty());
        expectStoredGroup(clear ? SAI_NULL_OBJECT_ID : pbh.oid);
        expectAssociation(pbh.oid, !clear);
        EXPECT_EQ(!clear, actualPbh[m_interface].count(700) != 0);
    }
}

TEST_P(SwitchVppAclBindingTest, PbhGroupIncludesBridgedChildrenAndExcludesRoutedChildren)
{
    auto group = createGroup(700);
    markPbh(group);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    addBridgedChild(m_interface);
    addBridgedChild(m_interface, 200);
    addBridgedChild(m_interface, 300);
    const auto child = m_interface + ".100";
    const auto second = m_interface + ".200";
    const auto routed = m_interface + ".300";
    ASSERT_TRUE(m_switch->m_ifaceRegistry.setRifOid(routed, 42));

    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    for (const auto &interface : {m_interface, child, second}) {
        EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[interface]);
    }
    EXPECT_TRUE(actualPbh[routed].empty());
    EXPECT_EQ(3u, pbhCalls.size());
    const auto key = std::make_pair(m_port, group.oid);
    EXPECT_EQ(3u, m_switch->m_acl_group_pbh_tables.at(key).size());
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    EXPECT_EQ(3u, pbhCalls.size());

    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    for (const auto &interface : {m_interface, child, second}) {
        EXPECT_TRUE(actualPbh[interface].empty());
    }
    EXPECT_TRUE(m_switch->m_acl_group_pbh_tables.empty());
}

TEST_P(SwitchVppAclBindingTest, NewChildInheritsPbhAndCleansFeaturesSeparately)
{
    auto group = createGroup(100);
    auto pbh = createTableMember(group.oid, 700);
    markPbh(pbh);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(pbh));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    addBridgedChild(m_interface);
    const auto child = m_interface + ".100";
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_switch->aclInheritInterfacePolicy(m_interface, child));
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[child]);
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[m_interface]);
    expectPolicy(child, {100, defaultAclIndex});

    // Regular ACL rollback must not unexpectedly detach independent PBH.
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_switch->aclRemoveInterfacePolicy(child));
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[child]);
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_switch->aclInheritInterfacePolicy(m_interface, child));
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_switch->aclRemoveInterfacePbh(child));
    EXPECT_TRUE(actualPbh[child].empty());
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[m_interface]);
    expectPolicy(child, {100, defaultAclIndex});

    const auto key = std::make_pair(m_port, group.oid);
    EXPECT_EQ(0u, m_switch->m_acl_group_pbh_tables.at(key).count(child));
    EXPECT_EQ(1u, m_switch->m_acl_group_pbh_tables.at(key).count(m_interface));
}

TEST_P(SwitchVppAclBindingTest, PartialPbhFanoutAndChildDetachFailuresRemainRetryable)
{
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    auto group = createGroup(700);
    markPbh(group);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    addBridgedChild(m_interface);
    const auto child = m_interface + ".100";
    pbhFailures[TuntermKey(child, 700, true)] = 1;
    // The parent attached successfully, but rollback also fails. Keep that
    // exact attachment while the rejected owner loses its SAI association.
    pbhFailures[TuntermKey(m_interface, 700, false)] = 100;
    ASSERT_NE(SAI_STATUS_SUCCESS, setGroup(group.oid));
    expectStoredGroup(SAI_NULL_OBJECT_ID);
    expectAssociation(group.oid, false);
    const auto key = std::make_pair(m_port, group.oid);
    ASSERT_EQ(1u, m_switch->m_acl_group_pbh_tables.count(key));
    EXPECT_EQ(1u, m_switch->m_acl_group_pbh_tables.at(key).count(m_interface));
    EXPECT_EQ(0u, m_switch->m_acl_group_pbh_tables.at(key).count(child));
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[m_interface]);
    EXPECT_TRUE(actualPbh[child].empty());

    pbhFailures.clear();
    const auto calls_before = pbhCalls.size();
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    ASSERT_EQ(calls_before + 1, pbhCalls.size());
    EXPECT_EQ(TuntermKey(child, 700, true), pbhCalls.back());
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[child]);

    pbhFailures[TuntermKey(child, 700, false)] = 1;
    ASSERT_EQ(SAI_STATUS_FAILURE, m_switch->aclRemoveInterfacePbh(child));
    EXPECT_EQ(1u, m_switch->m_acl_group_pbh_tables.at(key).count(child));
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[child]);
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_switch->aclRemoveInterfacePbh(child));
    EXPECT_EQ(0u, m_switch->m_acl_group_pbh_tables.at(key).count(child));
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[m_interface]);
}

TEST_P(SwitchVppAclBindingTest, PbhChildInheritanceFailureClearsPartialPolicy)
{
    auto group = createGroup(100);
    auto pbh = createTableMember(group.oid, 700);
    markPbh(pbh);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(pbh));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    addBridgedChild(m_interface);
    const auto child = m_interface + ".100";
    pbhFailures[TuntermKey(child, 700, true)] = 1;
    ASSERT_EQ(SAI_STATUS_FAILURE, m_switch->aclInheritInterfacePolicy(m_interface, child));
    expectPolicy(child, {});
    EXPECT_TRUE(actualPbh[child].empty());
    expectPolicy(m_interface, {100, defaultAclIndex});
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[m_interface]);
    ASSERT_EQ(SAI_STATUS_SUCCESS, m_switch->aclInheritInterfacePolicy(m_interface, child));
    expectPolicy(child, {100, defaultAclIndex});
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[child]);
}

TEST_P(SwitchVppAclBindingTest, SharedPbhOwnershipIsScopedToEachInterface)
{
    auto group = createGroup(700);
    markPbh(group);
    auto opposite = createGroup(800, !m_input);
    opposite.table = group.table;
    opposite.index = group.index;
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(opposite));
    addBridgedChild(m_interface);
    const auto child = m_interface + ".100";
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    auto excess = createTableMember(group.oid, 701);
    markPbh(excess);
    ASSERT_NE(SAI_STATUS_SUCCESS, addMember(excess));
    expectMemberObject(excess, false);
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[m_interface]);
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[child]);
    EXPECT_EQ(2u, pbhCalls.size());
    setOppositeGroup(opposite.oid);
    EXPECT_EQ(2u, pbhCalls.size());

    // A different table cannot displace the still accepted other owner of
    // the plugin's single PBH slot, even during a failed group replacement.
    auto conflicting = createGroup(900, !m_input);
    markPbh(conflicting);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(conflicting));
    ASSERT_NE(SAI_STATUS_SUCCESS, setOppositeGroupStatus(conflicting.oid));
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[m_interface]);
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[child]);
    const auto opposite_key = std::make_pair(m_port, opposite.oid);
    EXPECT_EQ(2u, m_switch->m_acl_group_pbh_tables.at(opposite_key).size());
    expectAssociation(conflicting.oid, false);

    const auto calls_before = pbhCalls.size();
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    EXPECT_EQ(calls_before, pbhCalls.size());
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[child]);
    setOppositeGroup(SAI_NULL_OBJECT_ID);
    EXPECT_TRUE(actualPbh[m_interface].empty());
    EXPECT_TRUE(actualPbh[child].empty());
    EXPECT_TRUE(m_switch->m_acl_group_pbh_tables.empty());
}

TEST_P(SwitchVppAclBindingTest, PbhReconcileRetiresFormerChildrenAndForgetsDeletedOnes)
{
    auto group = createGroup(700);
    markPbh(group);
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    addBridgedChild(m_interface);
    const auto child = m_interface + ".100";
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    ASSERT_TRUE(m_switch->m_ifaceRegistry.clearBdId(child));
    pbhFailures[TuntermKey(child, 700, false)] = 1;
    ASSERT_EQ(SAI_STATUS_FAILURE, setGroup(group.oid));
    const auto key = std::make_pair(m_port, group.oid);
    EXPECT_EQ(1u, m_switch->m_acl_group_pbh_tables.at(key).count(child));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    EXPECT_TRUE(actualPbh[child].empty());
    EXPECT_EQ(0u, m_switch->m_acl_group_pbh_tables.at(key).count(child));

    ASSERT_TRUE(m_switch->m_ifaceRegistry.setBdId(child, 100));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    // VPP automatically drops PBH on successful interface deletion. Forget
    // that child's bookkeeping without detaching the surviving parent.
    actualPbh[child].clear();
    m_switch->m_ifaceRegistry.remove(child);
    m_switch->aclForgetInterfacePolicy(child);
    const auto calls_before = pbhCalls.size();
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    EXPECT_EQ(calls_before, pbhCalls.size());
    EXPECT_EQ((std::set<uint32_t>{700}), actualPbh[m_interface]);
    EXPECT_EQ(0u, m_switch->m_acl_group_pbh_tables.at(key).count(child));
}

TEST_P(SwitchVppAclBindingTest, TuntermRemainsWhileOtherDirectionUsesSameTable)
{
    auto group = createGroup(100);
    m_switch->m_tunterm_acl_swindex_map[group.table] = 700;
    ASSERT_EQ(SAI_STATUS_SUCCESS, addMember(group));
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(group.oid));
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_switch->aclBindInterfaceTable(m_interface, group.table, !m_input, true));
    tuntermCalls.clear();
    ASSERT_EQ(SAI_STATUS_SUCCESS, setGroup(SAI_NULL_OBJECT_ID));
    EXPECT_TRUE(actualTunterm[std::make_pair(m_interface, 700)]);
    for (const auto &call : tuntermCalls) EXPECT_TRUE(std::get<2>(call));
    expectPolicy(m_interface, {100, defaultAclIndex}, !m_input);
    ASSERT_EQ(SAI_STATUS_SUCCESS,
            m_switch->aclBindInterfaceTable(m_interface, group.table, !m_input, false));
    EXPECT_FALSE(actualTunterm[std::make_pair(m_interface, 700)]);
    expectPolicy(m_interface, {}, !m_input);
}

INSTANTIATE_TEST_SUITE_P(PortAndLagDirections, SwitchVppAclBindingTest,
        ::testing::Values(std::make_tuple(SAI_OBJECT_TYPE_PORT, true),
                          std::make_tuple(SAI_OBJECT_TYPE_PORT, false),
                          std::make_tuple(SAI_OBJECT_TYPE_LAG, true),
                          std::make_tuple(SAI_OBJECT_TYPE_LAG, false)));
