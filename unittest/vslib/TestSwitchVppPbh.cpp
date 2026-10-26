/*
 * Policy Based Hashing (SAI PBH) translation tests.
 *
 * Every test drives a real SAI call and then reads back what reached the
 * vppxlate layer through MockVppXlate, because the whole point of
 * SwitchVppPbh is the translation in between: a rule that is programmed with
 * the wrong qualifier bit, the wrong profile index or at the wrong position
 * still forwards traffic, it just hashes it wrongly, so nothing short of
 * inspecting the VPP-facing structures catches it.
 */

#include "MockVppXlate.h"
#include "ContextConfigContainer.h"
#include "meta/sai_serialize.h"
#include "saivs.h"

#include <gtest/gtest.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>

using namespace saivs;

namespace
{
    constexpr uint32_t PBH_INVALID_INDEX = ~0u;

    struct PbhSwitch
    {
        std::shared_ptr<RealObjectIdManager> manager;
        std::shared_ptr<TestableSwitchVpp> sw;
        sai_object_id_t switchId;
    };

    PbhSwitch makeSwitch()
    {
        SWSS_LOG_ENTER();

        auto context = ContextConfigContainer::getDefault()->get(0);
        auto config = context->m_scc->getConfig(0);

        config->m_useTapDevice = false;
        config->m_profileMap[SAI_KEY_VS_PORT_CONFIG_FILE] = "/dev/null";

        PbhSwitch env;

        env.manager = std::make_shared<RealObjectIdManager>(context->m_guid, context->m_scc);
        env.switchId = env.manager->allocateNewSwitchObjectId("");
        env.sw = std::make_shared<TestableSwitchVpp>(env.switchId, env.manager, config);

        // The config is shared, so put back what the construction needed changed.
        config->m_useTapDevice = true;

        return env;
    }

    sai_attribute_t boolAttr(sai_attr_id_t id, bool value)
    {
        SWSS_LOG_ENTER();
        sai_attribute_t attr = {};
        attr.id = id;
        attr.value.booldata = value;
        return attr;
    }

    sai_attribute_t u32Attr(sai_attr_id_t id, uint32_t value)
    {
        SWSS_LOG_ENTER();
        sai_attribute_t attr = {};
        attr.id = id;
        attr.value.u32 = value;
        return attr;
    }

    sai_attribute_t oidAttr(sai_attr_id_t id, sai_object_id_t value)
    {
        SWSS_LOG_ENTER();
        sai_attribute_t attr = {};
        attr.id = id;
        attr.value.oid = value;
        return attr;
    }

    sai_attribute_t fieldU8(sai_attr_id_t id, uint8_t value, bool enable = true)
    {
        SWSS_LOG_ENTER();
        sai_attribute_t attr = {};
        attr.id = id;
        attr.value.aclfield.enable = enable;
        attr.value.aclfield.data.u8 = value;
        attr.value.aclfield.mask.u8 = 0xff;
        return attr;
    }

    sai_attribute_t fieldU16(sai_attr_id_t id, uint16_t value, bool enable = true)
    {
        SWSS_LOG_ENTER();
        sai_attribute_t attr = {};
        attr.id = id;
        attr.value.aclfield.enable = enable;
        attr.value.aclfield.data.u16 = value;
        attr.value.aclfield.mask.u16 = 0xffff;
        return attr;
    }

    sai_attribute_t fieldU32(sai_attr_id_t id, uint32_t value, uint32_t mask, bool enable = true)
    {
        SWSS_LOG_ENTER();
        sai_attribute_t attr = {};
        attr.id = id;
        attr.value.aclfield.enable = enable;
        attr.value.aclfield.data.u32 = value;
        attr.value.aclfield.mask.u32 = mask;
        return attr;
    }

    sai_attribute_t actionOid(sai_attr_id_t id, sai_object_id_t value, bool enable = true)
    {
        SWSS_LOG_ENTER();
        sai_attribute_t attr = {};
        attr.id = id;
        attr.value.aclaction.enable = enable;
        attr.value.aclaction.parameter.oid = value;
        return attr;
    }
}

/*
 * sonicExtFeatureEnabled() memorizes its answer in a process-wide cache, so the
 * first "pbh" query in this process decides what every later one sees.
 */
TEST(SwitchVppPbhFeature, disabledRefusesTableAndProfile)
{
    ::testing::FLAGS_gtest_death_test_style = "threadsafe";

    ASSERT_EXIT({
        vppMock().reset();

        auto env = makeSwitch();
        auto tbl = env.manager->allocateNewObjectId(SAI_OBJECT_TYPE_ACL_TABLE, env.switchId);

        if (env.sw->m_pbh.tableCreate(tbl) != SAI_STATUS_NOT_SUPPORTED ||
            !vppMock().pbhTableCalls.empty())
        {
            _exit(1);
        }

        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

class SwitchVppPbhTest : public ::testing::Test
{
protected:

    void SetUp() override
    {
        vppMock().reset();
        vppMock().enabledFeatures.insert("pbh");

        auto env = makeSwitch();

        manager = env.manager;
        switchId = env.switchId;
        sw = env.sw;
    }

    void TearDown() override
    {
        sw.reset();
    }

    sai_object_id_t allocate(sai_object_type_t type)
    {
        SWSS_LOG_ENTER();
        return manager->allocateNewObjectId(type, switchId);
    }

    sai_status_t create(sai_object_type_t type, sai_object_id_t oid,
                        const std::vector<sai_attribute_t>& attrs)
    {
        SWSS_LOG_ENTER();
        return sw->create(type, sai_serialize_object_id(oid), switchId,
                          (uint32_t) attrs.size(), attrs.data());
    }

    bool inStore(sai_object_type_t type, sai_object_id_t oid)
    {
        SWSS_LOG_ENTER();
        const auto& hash = sw->m_objectHash.at(type);
        return hash.find(sai_serialize_object_id(oid)) != hash.end();
    }

    /**
     * @brief Create a fine grained hash field, with an optional mask.
     */
    sai_object_id_t makeField(sai_native_hash_field_t field, uint32_t sequence,
                              const sai_attribute_t *mask = nullptr)
    {
        SWSS_LOG_ENTER();

        auto oid = allocate(SAI_OBJECT_TYPE_FINE_GRAINED_HASH_FIELD);

        std::vector<sai_attribute_t> attrs;
        sai_attribute_t attr = {};

        attr.id = SAI_FINE_GRAINED_HASH_FIELD_ATTR_NATIVE_HASH_FIELD;
        attr.value.s32 = field;
        attrs.push_back(attr);
        attrs.push_back(u32Attr(SAI_FINE_GRAINED_HASH_FIELD_ATTR_SEQUENCE_ID, sequence));

        if (mask != nullptr)
        {
            attrs.push_back(*mask);
        }

        EXPECT_EQ(SAI_STATUS_SUCCESS, sw->create_internal(
                SAI_OBJECT_TYPE_FINE_GRAINED_HASH_FIELD, sai_serialize_object_id(oid),
                switchId, (uint32_t) attrs.size(), attrs.data()));

        return oid;
    }

    /**
     * @brief Create a fine grained hash, i.e. a PBH profile.
     */
    sai_object_id_t makeHash(const std::vector<sai_object_id_t>& fields)
    {
        SWSS_LOG_ENTER();

        auto oid = allocate(SAI_OBJECT_TYPE_HASH);

        EXPECT_EQ(SAI_STATUS_SUCCESS, createHash(oid, fields));

        return oid;
    }

    sai_status_t createHash(sai_object_id_t oid, const std::vector<sai_object_id_t>& fields)
    {
        SWSS_LOG_ENTER();

        sai_attribute_t attr = {};

        attr.id = SAI_HASH_ATTR_FINE_GRAINED_HASH_FIELD_LIST;
        attr.value.objlist.count = (uint32_t) fields.size();
        attr.value.objlist.list = const_cast<sai_object_id_t *>(fields.data());

        return create(SAI_OBJECT_TYPE_HASH, oid, { attr });
    }

    /**
     * @brief Create an ACL table carrying the GRE_KEY + INNER_ETHER_TYPE
     * signature that marks it as PBH.
     */
    sai_object_id_t makePbhTable()
    {
        SWSS_LOG_ENTER();

        auto oid = allocate(SAI_OBJECT_TYPE_ACL_TABLE);

        EXPECT_EQ(SAI_STATUS_SUCCESS, create(SAI_OBJECT_TYPE_ACL_TABLE, oid, {
                boolAttr(SAI_ACL_TABLE_ATTR_FIELD_GRE_KEY, true),
                boolAttr(SAI_ACL_TABLE_ATTR_FIELD_INNER_ETHER_TYPE, true) }));

        return oid;
    }

    sai_object_id_t makeEntry(sai_object_id_t tbl, uint32_t priority,
                              std::vector<sai_attribute_t> attrs)
    {
        SWSS_LOG_ENTER();

        auto oid = allocate(SAI_OBJECT_TYPE_ACL_ENTRY);

        EXPECT_EQ(SAI_STATUS_SUCCESS, createEntry(oid, tbl, priority, attrs));

        return oid;
    }

    sai_status_t createEntry(sai_object_id_t oid, sai_object_id_t tbl, uint32_t priority,
                             std::vector<sai_attribute_t> attrs)
    {
        SWSS_LOG_ENTER();

        attrs.insert(attrs.begin(), u32Attr(SAI_ACL_ENTRY_ATTR_PRIORITY, priority));
        attrs.insert(attrs.begin(), oidAttr(SAI_ACL_ENTRY_ATTR_TABLE_ID, tbl));

        return create(SAI_OBJECT_TYPE_ACL_ENTRY, oid, attrs);
    }

    static uint32_t ruleId(sai_object_id_t entry_oid)
    {
        SWSS_LOG_ENTER();

        return (uint32_t) (entry_oid & 0xffffffff);
    }

    std::shared_ptr<RealObjectIdManager> manager;
    std::shared_ptr<TestableSwitchVpp> sw;
    sai_object_id_t switchId = SAI_NULL_OBJECT_ID;
};

//
// Hash objects and PBH profiles.
//

TEST_F(SwitchVppPbhTest, createHashTranslatesEveryFineGrainedField)
{
    sai_attribute_t v4_mask = {};
    sai_attribute_t v6_mask = {};

    v4_mask.id = SAI_FINE_GRAINED_HASH_FIELD_ATTR_IPV4_MASK;
    v4_mask.value.ip4 = 0x00ffffff;

    v6_mask.id = SAI_FINE_GRAINED_HASH_FIELD_ATTR_IPV6_MASK;
    memset(v6_mask.value.ip6, 0xff, sizeof(v6_mask.value.ip6));
    v6_mask.value.ip6[15] = 0x00;

    auto hash = makeHash({
            makeField(SAI_NATIVE_HASH_FIELD_INNER_SRC_IPV4, 1, &v4_mask),
            makeField(SAI_NATIVE_HASH_FIELD_INNER_DST_IPV6, 2, &v6_mask),
            makeField(SAI_NATIVE_HASH_FIELD_INNER_L4_SRC_PORT, 3),
            makeField(SAI_NATIVE_HASH_FIELD_INNER_IP_PROTOCOL, 4) });

    ASSERT_EQ(1u, vppMock().pbhProfileCalls.size());

    const auto& call = vppMock().pbhProfileCalls[0];

    EXPECT_TRUE(call.is_add);
    EXPECT_EQ(PBH_INVALID_INDEX, call.profile_index);
    ASSERT_EQ(4u, call.fields.size());

    EXPECT_EQ((uint8_t) VPP_PBH_HF_INNER_SRC_IPV4, call.fields[0].field);
    EXPECT_EQ(1u, call.fields[0].sequence_id);
    EXPECT_EQ(0, memcmp(call.fields[0].mask, &v4_mask.value.ip4, sizeof(v4_mask.value.ip4)));

    EXPECT_EQ((uint8_t) VPP_PBH_HF_INNER_DST_IPV6, call.fields[1].field);
    EXPECT_EQ(2u, call.fields[1].sequence_id);
    EXPECT_EQ(0, memcmp(call.fields[1].mask, v6_mask.value.ip6, sizeof(v6_mask.value.ip6)));

    // A non-address field hashes whole, so it carries no mask.
    EXPECT_EQ((uint8_t) VPP_PBH_HF_INNER_L4_SRC_PORT, call.fields[2].field);
    EXPECT_EQ(3u, call.fields[2].sequence_id);

    EXPECT_EQ((uint8_t) VPP_PBH_HF_INNER_IP_PROTOCOL, call.fields[3].field);
    EXPECT_EQ(4u, call.fields[3].sequence_id);

    EXPECT_EQ(vppMock().nextPbhProfileIndex - 1, sw->m_pbh.m_profile_map.at(hash));
    EXPECT_TRUE(inStore(SAI_OBJECT_TYPE_HASH, hash));

    // A hash carrying the native field list is the switch-global one, and is
    // stored without reaching PBH at all.
    auto global = allocate(SAI_OBJECT_TYPE_HASH);
    int32_t native[1] = { SAI_NATIVE_HASH_FIELD_SRC_IP };
    sai_attribute_t global_attr = {};

    global_attr.id = SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST;
    global_attr.value.s32list.count = 1;
    global_attr.value.s32list.list = native;

    ASSERT_EQ(SAI_STATUS_SUCCESS, create(SAI_OBJECT_TYPE_HASH, global, { global_attr }));

    EXPECT_EQ(1u, vppMock().pbhProfileCalls.size());
    EXPECT_EQ(0u, sw->m_pbh.m_profile_map.count(global));
    EXPECT_TRUE(inStore(SAI_OBJECT_TYPE_HASH, global));
}

TEST_F(SwitchVppPbhTest, createHashRejectsWhatItCannotProgram)
{
    // An outer field has no PBH equivalent.
    auto outer = allocate(SAI_OBJECT_TYPE_HASH);

    EXPECT_EQ(SAI_STATUS_NOT_SUPPORTED, createHash(outer,
            { makeField(SAI_NATIVE_HASH_FIELD_SRC_IP, 1) }));

    // A field named but never created would hash on a subset of what was asked.
    auto incomplete = allocate(SAI_OBJECT_TYPE_HASH);

    EXPECT_EQ(SAI_STATUS_FAILURE, createHash(incomplete,
            { makeField(SAI_NATIVE_HASH_FIELD_INNER_L4_DST_PORT, 1),
              allocate(SAI_OBJECT_TYPE_FINE_GRAINED_HASH_FIELD) }));

    // Neither got as far as VPP.
    EXPECT_TRUE(vppMock().pbhProfileCalls.empty());
    EXPECT_FALSE(inStore(SAI_OBJECT_TYPE_HASH, outer));
    EXPECT_FALSE(inStore(SAI_OBJECT_TYPE_HASH, incomplete));

    // A profile VPP refuses leaves no hash behind either.
    auto refused = allocate(SAI_OBJECT_TYPE_HASH);

    vppMock().pbhProfileResult = -1;

    EXPECT_EQ(SAI_STATUS_FAILURE, createHash(refused,
            { makeField(SAI_NATIVE_HASH_FIELD_INNER_DST_IPV4, 1) }));
    EXPECT_EQ(1u, vppMock().pbhProfileCalls.size());
    EXPECT_EQ(0u, sw->m_pbh.m_profile_map.count(refused));
    EXPECT_FALSE(inStore(SAI_OBJECT_TYPE_HASH, refused));
}

TEST_F(SwitchVppPbhTest, setHashReplacesTheProfileInPlace)
{
    auto hash = makeHash({ makeField(SAI_NATIVE_HASH_FIELD_INNER_SRC_IPV4, 1) });
    auto sid = sai_serialize_object_id(hash);
    uint32_t index = sw->m_pbh.m_profile_map.at(hash);

    // Nothing but the field list changes what VPP hashes on.
    int32_t native[1] = { SAI_NATIVE_HASH_FIELD_SRC_IP };
    sai_attribute_t other = {};

    other.id = SAI_HASH_ATTR_NATIVE_HASH_FIELD_LIST;
    other.value.s32list.count = 1;
    other.value.s32list.list = native;

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->set(SAI_OBJECT_TYPE_HASH, sid, &other));
    EXPECT_EQ(1u, vppMock().pbhProfileCalls.size());

    sai_object_id_t fields[2] = {
        makeField(SAI_NATIVE_HASH_FIELD_INNER_SRC_IPV6, 1),
        makeField(SAI_NATIVE_HASH_FIELD_INNER_DST_IPV6, 2) };

    sai_attribute_t attr = {};

    attr.id = SAI_HASH_ATTR_FINE_GRAINED_HASH_FIELD_LIST;
    attr.value.objlist.count = 2;
    attr.value.objlist.list = fields;

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->set(SAI_OBJECT_TYPE_HASH, sid, &attr));

    ASSERT_EQ(2u, vppMock().pbhProfileCalls.size());

    const auto& call = vppMock().pbhProfileCalls[1];

    // Named rather than reallocated: the rules carry the index.
    EXPECT_TRUE(call.is_add);
    EXPECT_EQ(index, call.profile_index);
    ASSERT_EQ(2u, call.fields.size());
    EXPECT_EQ((uint8_t) VPP_PBH_HF_INNER_SRC_IPV6, call.fields[0].field);
    EXPECT_EQ((uint8_t) VPP_PBH_HF_INNER_DST_IPV6, call.fields[1].field);
    EXPECT_EQ(index, sw->m_pbh.m_profile_map.at(hash));

    // A refused update leaves the profile where the rules expect it.
    sai_object_id_t replacement[1] = { makeField(SAI_NATIVE_HASH_FIELD_INNER_DST_IPV4, 1) };

    attr.value.objlist.count = 1;
    attr.value.objlist.list = replacement;

    vppMock().pbhProfileResult = -1;

    EXPECT_EQ(SAI_STATUS_FAILURE, sw->set(SAI_OBJECT_TYPE_HASH, sid, &attr));
    EXPECT_EQ(index, sw->m_pbh.m_profile_map.at(hash));
}

TEST_F(SwitchVppPbhTest, removeHashDeletesTheProfile)
{
    auto hash = makeHash({ makeField(SAI_NATIVE_HASH_FIELD_INNER_SRC_IPV4, 1) });
    auto sid = sai_serialize_object_id(hash);
    uint32_t index = sw->m_pbh.m_profile_map.at(hash);

    // A refused delete leaves the profile live in VPP, so the hash has to stay
    // reachable for the removal to be retried.
    vppMock().pbhProfileResult = -1;

    EXPECT_EQ(SAI_STATUS_FAILURE, sw->remove(SAI_OBJECT_TYPE_HASH, sid));
    EXPECT_EQ(index, sw->m_pbh.m_profile_map.at(hash));
    EXPECT_TRUE(inStore(SAI_OBJECT_TYPE_HASH, hash));

    vppMock().pbhProfileResult = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->remove(SAI_OBJECT_TYPE_HASH, sid));

    ASSERT_EQ(3u, vppMock().pbhProfileCalls.size());
    EXPECT_FALSE(vppMock().pbhProfileCalls[2].is_add);
    EXPECT_EQ(index, vppMock().pbhProfileCalls[2].profile_index);
    EXPECT_EQ(0u, vppMock().pbhProfiles.count(index));
    EXPECT_EQ(0u, sw->m_pbh.m_profile_map.count(hash));
    EXPECT_FALSE(inStore(SAI_OBJECT_TYPE_HASH, hash));
}

//
// Tables.
//

TEST_F(SwitchVppPbhTest, tableCreateProgramsAnEmptyTable)
{
    // INNER_ETHER_TYPE alone is a P4Orch table, not a PBH one. Stored rather
    // than created, because an ordinary ACL table would reach the ACL plugin.
    auto ordinary = allocate(SAI_OBJECT_TYPE_ACL_TABLE);
    auto ordinary_attr = boolAttr(SAI_ACL_TABLE_ATTR_FIELD_INNER_ETHER_TYPE, true);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->create_internal(SAI_OBJECT_TYPE_ACL_TABLE,
            sai_serialize_object_id(ordinary), switchId, 1, &ordinary_attr));

    EXPECT_FALSE(sw->m_pbh.isPbhTable(ordinary));
    EXPECT_TRUE(vppMock().pbhTableCalls.empty());

    auto tbl = makePbhTable();

    ASSERT_EQ(1u, vppMock().pbhTableCalls.size());

    const auto& call = vppMock().pbhTableCalls[0];

    EXPECT_TRUE(call.named);
    EXPECT_EQ("sonic_pbh_" + sai_serialize_object_id(tbl), call.name);
    EXPECT_EQ(PBH_INVALID_INDEX, call.table_index);
    EXPECT_TRUE(call.rules.empty());

    EXPECT_EQ(vppMock().nextPbhTableIndex - 1, sw->m_pbh.m_table_index_map.at(tbl));
    EXPECT_TRUE(sw->m_pbh.isPbhTable(tbl));

    // A refused create leaves neither an index nor a stored object.
    auto refused = allocate(SAI_OBJECT_TYPE_ACL_TABLE);

    vppMock().pbhTableResult = -1;

    EXPECT_EQ(SAI_STATUS_FAILURE, create(SAI_OBJECT_TYPE_ACL_TABLE, refused, {
            boolAttr(SAI_ACL_TABLE_ATTR_FIELD_GRE_KEY, true),
            boolAttr(SAI_ACL_TABLE_ATTR_FIELD_INNER_ETHER_TYPE, true) }));

    EXPECT_EQ(2u, vppMock().pbhTableCalls.size());
    EXPECT_EQ(0u, sw->m_pbh.m_table_index_map.count(refused));
    EXPECT_FALSE(inStore(SAI_OBJECT_TYPE_ACL_TABLE, refused));
}

TEST_F(SwitchVppPbhTest, tableRemoveDeletesTheTable)
{
    auto tbl = makePbhTable();
    uint32_t index = sw->m_pbh.m_table_index_map.at(tbl);

    vppMock().pbhTableDelResult = -1;

    EXPECT_EQ(SAI_STATUS_FAILURE, sw->remove(SAI_OBJECT_TYPE_ACL_TABLE,
            sai_serialize_object_id(tbl)));

    // The table is still live in VPP, so the handles needed to retry are kept.
    EXPECT_EQ(index, sw->m_pbh.m_table_index_map.at(tbl));
    EXPECT_EQ(1u, sw->m_pbh.m_tables.count(tbl));

    // The SAI object is already gone by now, so the retry goes straight at
    // SwitchVppPbh rather than back through remove().
    vppMock().pbhTableDelResult = 0;

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->m_pbh.tableRemove(tbl));

    ASSERT_EQ(2u, vppMock().pbhTableDels.size());
    EXPECT_EQ(index, vppMock().pbhTableDels[1]);
    EXPECT_EQ(0u, vppMock().pbhTables.count(index));
    EXPECT_EQ(0u, sw->m_pbh.m_table_index_map.count(tbl));
    EXPECT_EQ(0u, sw->m_pbh.m_tables.count(tbl));
}

//
// Rules.
//

TEST_F(SwitchVppPbhTest, entryTranslatesEveryQualifierAndAction)
{
    auto ecmp = makeHash({ makeField(SAI_NATIVE_HASH_FIELD_INNER_SRC_IPV4, 1) });
    auto lag = makeHash({ makeField(SAI_NATIVE_HASH_FIELD_INNER_DST_IPV4, 1) });
    auto tbl = makePbhTable();
    auto cntr = allocate(SAI_OBJECT_TYPE_ACL_COUNTER);

    auto entry = makeEntry(tbl, 999, {
            fieldU16(SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE, 0x0800),
            fieldU8(SAI_ACL_ENTRY_ATTR_FIELD_IP_PROTOCOL, 47),
            fieldU8(SAI_ACL_ENTRY_ATTR_FIELD_IPV6_NEXT_HEADER, 17),
            fieldU16(SAI_ACL_ENTRY_ATTR_FIELD_L4_DST_PORT, 4789),
            fieldU16(SAI_ACL_ENTRY_ATTR_FIELD_INNER_ETHER_TYPE, 0x86dd),
            fieldU32(SAI_ACL_ENTRY_ATTR_FIELD_GRE_KEY, 0x5a5a5a5a, 0xffffff00),
            actionOid(SAI_ACL_ENTRY_ATTR_ACTION_SET_ECMP_HASH_ID, ecmp),
            actionOid(SAI_ACL_ENTRY_ATTR_ACTION_SET_LAG_HASH_ID, lag),
            actionOid(SAI_ACL_ENTRY_ATTR_ACTION_COUNTER, cntr) });

    // The create of the table plus the replace driven by the entry.
    ASSERT_EQ(2u, vppMock().pbhTableCalls.size());

    const auto& replace = vppMock().pbhTableCalls[1];

    // A replace names an existing table rather than asking for a new one.
    EXPECT_FALSE(replace.named);
    EXPECT_EQ(sw->m_pbh.m_table_index_map.at(tbl), replace.table_index);
    ASSERT_EQ(1u, replace.rules.size());

    const auto& rule = replace.rules[0];

    EXPECT_EQ(ruleId(entry), rule.rule_id);
    EXPECT_EQ(999u, rule.priority);
    EXPECT_EQ((uint32_t) (VPP_PBH_Q_ETHER_TYPE | VPP_PBH_Q_IP_PROTOCOL |
                          VPP_PBH_Q_IPV6_NEXT_HEADER | VPP_PBH_Q_L4_DST_PORT |
                          VPP_PBH_Q_INNER_ETHER_TYPE | VPP_PBH_Q_GRE_KEY),
              rule.qualifiers);
    EXPECT_EQ(0x0800, rule.ether_type);
    EXPECT_EQ(47, rule.ip_protocol);
    EXPECT_EQ(17, rule.ipv6_next_header);
    EXPECT_EQ(4789, rule.l4_dst_port);
    EXPECT_EQ(0x86dd, rule.inner_ether_type);
    EXPECT_EQ(0x5a5a5a5au, rule.gre_key);
    EXPECT_EQ(0xffffff00u, rule.gre_key_mask);
    EXPECT_EQ(sw->m_pbh.m_profile_map.at(ecmp), rule.ecmp_profile);
    EXPECT_EQ(sw->m_pbh.m_profile_map.at(lag), rule.lag_profile);
    EXPECT_TRUE(rule.flow_counter);
}

TEST_F(SwitchVppPbhTest, disabledQualifiersAndActionsAreNotProgrammed)
{
    auto ecmp = makeHash({ makeField(SAI_NATIVE_HASH_FIELD_INNER_SRC_IPV4, 1) });
    auto tbl = makePbhTable();

    makeEntry(tbl, 1, {
            fieldU16(SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE, 0x0800, false),
            fieldU32(SAI_ACL_ENTRY_ATTR_FIELD_GRE_KEY, 1, 1, false),
            actionOid(SAI_ACL_ENTRY_ATTR_ACTION_SET_ECMP_HASH_ID, ecmp, false) });

    ASSERT_EQ(1u, vppMock().lastPbhRules().size());

    const auto& rule = vppMock().lastPbhRules()[0];

    EXPECT_EQ(0u, rule.qualifiers);
    EXPECT_EQ(PBH_INVALID_INDEX, rule.ecmp_profile);
    EXPECT_EQ(PBH_INVALID_INDEX, rule.lag_profile);
    EXPECT_FALSE(rule.flow_counter);
}

TEST_F(SwitchVppPbhTest, rulesAreSortedByPriorityThenRuleId)
{
    auto tbl = makePbhTable();

    auto low = makeEntry(tbl, 10, { fieldU16(SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE, 1) });
    auto first = makeEntry(tbl, 30, { fieldU16(SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE, 2) });
    auto second = makeEntry(tbl, 30, { fieldU16(SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE, 3) });

    ASSERT_LT(ruleId(first), ruleId(second));

    const auto& rules = vppMock().lastPbhRules();

    ASSERT_EQ(3u, rules.size());
    EXPECT_EQ(ruleId(first), rules[0].rule_id);
    EXPECT_EQ(ruleId(second), rules[1].rule_id);
    EXPECT_EQ(ruleId(low), rules[2].rule_id);

    for (auto entry: { low, first, second })
    {
        ASSERT_EQ(SAI_STATUS_SUCCESS, sw->remove(SAI_OBJECT_TYPE_ACL_ENTRY,
                sai_serialize_object_id(entry)));
    }

    // An emptied PBH table is reprogrammed empty, not left holding the old set.
    EXPECT_TRUE(vppMock().lastPbhRules().empty());
}

TEST_F(SwitchVppPbhTest, entryCreateRollsBackWhenItCannotBeProgrammed)
{
    auto tbl = makePbhTable();

    // A rule naming a profile that does not exist would hash on nothing, which
    // looks just like PBH working, so it is refused before VPP is asked.
    auto stranger = allocate(SAI_OBJECT_TYPE_HASH);
    auto unresolved = allocate(SAI_OBJECT_TYPE_ACL_ENTRY);

    EXPECT_NE(SAI_STATUS_SUCCESS, createEntry(unresolved, tbl, 10, {
            actionOid(SAI_ACL_ENTRY_ATTR_ACTION_SET_ECMP_HASH_ID, stranger) }));

    EXPECT_EQ(1u, vppMock().pbhTableCalls.size());
    EXPECT_TRUE(sw->m_acl_tbl_rules_map[tbl].empty());
    EXPECT_FALSE(inStore(SAI_OBJECT_TYPE_ACL_ENTRY, unresolved));

    // A refused reprogram means VPP never took the entry, so neither does the
    // state.
    auto refused = allocate(SAI_OBJECT_TYPE_ACL_ENTRY);

    vppMock().pbhTableResult = -1;

    EXPECT_NE(SAI_STATUS_SUCCESS, createEntry(refused, tbl, 10,
            { fieldU16(SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE, 1) }));

    EXPECT_EQ(2u, vppMock().pbhTableCalls.size());
    EXPECT_TRUE(sw->m_acl_tbl_rules_map[tbl].empty());
    EXPECT_FALSE(inStore(SAI_OBJECT_TYPE_ACL_ENTRY, refused));
}

TEST_F(SwitchVppPbhTest, entrySetReprogramsTheRule)
{
    auto tbl = makePbhTable();
    auto entry = makeEntry(tbl, 10, { fieldU16(SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE, 0x0800) });
    auto sid = sai_serialize_object_id(entry);

    auto attr = fieldU16(SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE, 0x86dd);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->set(SAI_OBJECT_TYPE_ACL_ENTRY, sid, &attr));

    ASSERT_EQ(1u, vppMock().lastPbhRules().size());
    EXPECT_EQ(0x86dd, vppMock().lastPbhRules()[0].ether_type);

    auto rejected = fieldU16(SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE, 0x0806);

    vppMock().pbhTableResult = -1;

    EXPECT_NE(SAI_STATUS_SUCCESS, sw->set(SAI_OBJECT_TYPE_ACL_ENTRY, sid, &rejected));

    // VPP is still enforcing the old value, so the state has to keep saying so.
    sai_attribute_t stored = {};

    stored.id = SAI_ACL_ENTRY_ATTR_FIELD_ETHER_TYPE;
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->get(SAI_OBJECT_TYPE_ACL_ENTRY, sid, 1, &stored));
    EXPECT_EQ(0x86dd, stored.value.aclfield.data.u16);
}

//
// Interface attachment.
//

TEST_F(SwitchVppPbhTest, bindAndUnbindAttachTheTable)
{
    auto unknown = allocate(SAI_OBJECT_TYPE_ACL_TABLE);

    EXPECT_EQ(SAI_STATUS_FAILURE, sw->m_pbh.bindUnbindPort("port0", unknown, true));
    EXPECT_TRUE(vppMock().pbhAttachCalls.empty());

    auto tbl = makePbhTable();
    uint32_t index = sw->m_pbh.m_table_index_map.at(tbl);

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->m_pbh.bindUnbindPort("port0", tbl, true));
    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->m_pbh.bindUnbindPort("port0", tbl, false));

    ASSERT_EQ(2u, vppMock().pbhAttachCalls.size());
    EXPECT_EQ("port0", vppMock().pbhAttachCalls[0].hwif_name);
    EXPECT_EQ(index, vppMock().pbhAttachCalls[0].table_index);
    EXPECT_TRUE(vppMock().pbhAttachCalls[0].attach);
    EXPECT_FALSE(vppMock().pbhAttachCalls[1].attach);

    vppMock().pbhAttachResult = -1;

    EXPECT_EQ(SAI_STATUS_FAILURE, sw->m_pbh.bindUnbindPort("port0", tbl, true));
    EXPECT_EQ(3u, vppMock().pbhAttachCalls.size());
}

//
// Counters.
//

TEST_F(SwitchVppPbhTest, countersReadThePbhSubtreeAtTheProgrammedPosition)
{
    auto tbl = makePbhTable();

    // Deliberately created low priority first, so the counter position and the
    // creation order disagree.
    auto low_cntr = allocate(SAI_OBJECT_TYPE_ACL_COUNTER);
    auto high_cntr = allocate(SAI_OBJECT_TYPE_ACL_COUNTER);

    makeEntry(tbl, 10, { actionOid(SAI_ACL_ENTRY_ATTR_ACTION_COUNTER, low_cntr) });
    makeEntry(tbl, 20, { actionOid(SAI_ACL_ENTRY_ATTR_ACTION_COUNTER, high_cntr) });

    uint32_t index = sw->m_pbh.m_table_index_map.at(tbl);

    EXPECT_TRUE(sw->m_pbh.isPbhCounterOid(low_cntr));
    EXPECT_EQ(0u, sw->m_pbh.m_cntr_info_map.at(high_cntr).rule_index);
    EXPECT_EQ(1u, sw->m_pbh.m_cntr_info_map.at(low_cntr).rule_index);

    vppMock().pbhStats.packets = 17;
    vppMock().pbhStats.bytes = 2048;

    std::vector<sai_attribute_t> attrs(2);

    attrs[0].id = SAI_ACL_COUNTER_ATTR_PACKETS;
    attrs[1].id = SAI_ACL_COUNTER_ATTR_BYTES;

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->get(SAI_OBJECT_TYPE_ACL_COUNTER,
            sai_serialize_object_id(low_cntr), 2, attrs.data()));

    EXPECT_EQ(17u, attrs[0].value.u64);
    EXPECT_EQ(2048u, attrs[1].value.u64);

    ASSERT_EQ(1u, vppMock().ruleStatsCalls.size());

    // PBH counters live in their own stats subtree, not the ACL plugin's.
    EXPECT_EQ(VPP_RULE_STATS_PBH, vppMock().ruleStatsCalls[0].type);
    EXPECT_EQ(index, vppMock().ruleStatsCalls[0].table_index);
    EXPECT_EQ(1u, vppMock().ruleStatsCalls[0].rule_index);

    vppMock().pbhStatsResult = -1;

    EXPECT_EQ(SAI_STATUS_FAILURE, sw->get(SAI_OBJECT_TYPE_ACL_COUNTER,
            sai_serialize_object_id(low_cntr), 1, attrs.data()));

    ASSERT_EQ(SAI_STATUS_SUCCESS, sw->remove(SAI_OBJECT_TYPE_ACL_TABLE,
            sai_serialize_object_id(tbl)));

    EXPECT_FALSE(sw->m_pbh.isPbhCounterOid(low_cntr));
    EXPECT_FALSE(sw->m_pbh.isPbhCounterOid(high_cntr));
}
