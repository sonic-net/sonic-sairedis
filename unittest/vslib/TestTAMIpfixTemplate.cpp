#include "swss/logger.h"
/**
 * @file TestTAMIpfixTemplate.cpp
 * @brief Unit tests for IPFIX template generation in refresh_tam_tel_ipfix_templates()
 *
 * Tests the Task 2 implementation: real IPFIX template generation based on
 * TAM_COUNTER_SUBSCRIPTION objects, replacing the old placeholder random data.
 *
 * Test cases:
 * 1. Empty subscription — no counter_subscription objects exist
 * 2. Single counter_subscription — verify binary format per HLD 7.2.2
 * 3. Multiple counter_subscriptions — verify field count and enterprise encoding
 * 4. Subscription not matching tel_type — verify filtering logic
 */

#include <SwitchStateBase.h>
#include <EventPayloadNotification.h>
#include <meta/sai_serialize.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <cstring>

using namespace saivs;
using namespace std;

// ---- Helper: decode big-endian values from buffer ----

static uint16_t read_u16_be(const uint8_t *buf)
{
    return static_cast<uint16_t>((buf[0] << 8) | buf[1]);
}

static uint32_t read_u32_be(const uint8_t *buf)
{
    return (static_cast<uint32_t>(buf[0]) << 24) |
           (static_cast<uint32_t>(buf[1]) << 16) |
           (static_cast<uint32_t>(buf[2]) << 8) |
           static_cast<uint32_t>(buf[3]);
}

// ---- Test fixture ----

class TAMIpfixTemplateTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        SWSS_LOG_ENTER();
        m_signal = std::make_shared<Signal>();
        m_eventQueue = std::make_shared<EventQueue>(m_signal);
        m_sc = std::make_shared<SwitchConfig>(0, "");
        m_sc->m_eventQueue = m_eventQueue;
        m_scc = std::make_shared<SwitchConfigContainer>();
        m_switch_id = 0x2100000000;

        m_ridmgr = std::make_shared<RealObjectIdManager>(0, m_scc);
        m_ss = std::make_shared<SwitchStateBase>(
            m_switch_id,
            m_ridmgr,
            m_sc);

        // Create switch
        vector<sai_attribute_t> attrs;
        sai_attribute_t attr;
        attr.id = SAI_SWITCH_ATTR_TAM_TEL_TYPE_CONFIG_CHANGE_NOTIFY;
        attr.value.ptr = nullptr; // no callback needed for template tests
        attrs.push_back(attr);

        ASSERT_EQ(SAI_STATUS_SUCCESS,
                  m_ss->create(SAI_OBJECT_TYPE_SWITCH,
                               sai_serialize_object_id(m_switch_id), 0,
                               static_cast<uint32_t>(attrs.size()), attrs.data()));

        // Create TAM hierarchy: TAM -> TAM_TELEMETRY -> TAM_TEL_TYPE
        m_tam_id = m_ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_TAM, m_switch_id);
        ASSERT_EQ(SAI_STATUS_SUCCESS,
                  m_ss->create(SAI_OBJECT_TYPE_TAM,
                               sai_serialize_object_id(m_tam_id), m_switch_id, 0, nullptr));

        m_tam_tel_id = m_ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_TAM_TELEMETRY, m_switch_id);
        ASSERT_EQ(SAI_STATUS_SUCCESS,
                  m_ss->create(SAI_OBJECT_TYPE_TAM_TELEMETRY,
                               sai_serialize_object_id(m_tam_tel_id), m_switch_id, 0, nullptr));

        m_tam_tel_type_id = m_ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_TAM_TEL_TYPE, m_switch_id);
        ASSERT_EQ(SAI_STATUS_SUCCESS,
                  m_ss->create(SAI_OBJECT_TYPE_TAM_TEL_TYPE,
                               sai_serialize_object_id(m_tam_tel_type_id), m_switch_id, 0, nullptr));
    }

    /**
     * @brief Trigger IPFIX template generation and retrieve the result.
     * Sets TAM_TEL_TYPE state to CREATE_CONFIG, then gets IPFIX_TEMPLATES.
     */
    std::vector<uint8_t> getIpfixTemplates()
    {
        SWSS_LOG_ENTER();
        sai_attribute_t attr;

        // Trigger template generation by setting state
        attr.id = SAI_TAM_TEL_TYPE_ATTR_STATE;
        attr.value.s32 = SAI_TAM_TEL_TYPE_STATE_CREATE_CONFIG;
        EXPECT_EQ(SAI_STATUS_SUCCESS,
                  m_ss->set(SAI_OBJECT_TYPE_TAM_TEL_TYPE,
                            sai_serialize_object_id(m_tam_tel_type_id), &attr));

        // Get the generated IPFIX templates
        std::vector<uint8_t> buffer(64 * 1024); // 64KB should be plenty
        attr.id = SAI_TAM_TEL_TYPE_ATTR_IPFIX_TEMPLATES;
        attr.value.u8list.count = static_cast<uint32_t>(buffer.size());
        attr.value.u8list.list = buffer.data();

        EXPECT_EQ(SAI_STATUS_SUCCESS,
                  m_ss->get(SAI_OBJECT_TYPE_TAM_TEL_TYPE,
                            sai_serialize_object_id(m_tam_tel_type_id), 1, &attr));

        buffer.resize(attr.value.u8list.count);
        return buffer;
    }

    /**
     * @brief Create a TAM_COUNTER_SUBSCRIPTION object with given attributes.
     */
    sai_object_id_t createCounterSubscription(
        sai_object_id_t tel_type_oid,
        uint64_t label,
        uint32_t stat_id,
        sai_object_id_t subscribed_object_id)
    {
        SWSS_LOG_ENTER();

        sai_object_id_t cs_id = m_ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_TAM_COUNTER_SUBSCRIPTION, m_switch_id);
        string cs_id_str = sai_serialize_object_id(cs_id);

        vector<sai_attribute_t> attrs;
        sai_attribute_t attr;

        // TEL_TYPE reference
        attr.id = SAI_TAM_COUNTER_SUBSCRIPTION_ATTR_TEL_TYPE;
        attr.value.oid = tel_type_oid;
        attrs.push_back(attr);

        // LABEL
        attr.id = SAI_TAM_COUNTER_SUBSCRIPTION_ATTR_LABEL;
        attr.value.u64 = label;
        attrs.push_back(attr);

        // STAT_ID
        attr.id = SAI_TAM_COUNTER_SUBSCRIPTION_ATTR_STAT_ID;
        attr.value.u32 = stat_id;
        attrs.push_back(attr);

        // OBJECT_ID (the object being monitored)
        attr.id = SAI_TAM_COUNTER_SUBSCRIPTION_ATTR_OBJECT_ID;
        attr.value.oid = subscribed_object_id;
        attrs.push_back(attr);

        EXPECT_EQ(SAI_STATUS_SUCCESS,
                  m_ss->create(SAI_OBJECT_TYPE_TAM_COUNTER_SUBSCRIPTION,
                               cs_id_str, m_switch_id,
                               static_cast<uint32_t>(attrs.size()), attrs.data()));
        return cs_id;
    }

    /**
     * @brief Validate the common IPFIX template header.
     */
    void validateTemplateHeader(const std::vector<uint8_t> &tmpl,
                                uint16_t expected_num_fields)
    {
        SWSS_LOG_ENTER();
        // Minimum size: 4 (set header) + 4 (template header) + 4 (timestamp field) = 12
        ASSERT_GE(tmpl.size(), 12u);

        const uint8_t *p = tmpl.data();

        // Set ID = 2 (IPFIX template set)
        EXPECT_EQ(read_u16_be(p + 0), 2u);

        // Set Length
        uint16_t expected_set_length = static_cast<uint16_t>(12 + (expected_num_fields - 1) * 8);
        EXPECT_EQ(read_u16_be(p + 2), expected_set_length);

        // Template ID >= 256
        EXPECT_GE(read_u16_be(p + 4), 256u);

        // Number of Fields
        EXPECT_EQ(read_u16_be(p + 6), expected_num_fields);

        // Field 0: observationTimeNanoseconds (Element ID = 325, Length = 8)
        EXPECT_EQ(read_u16_be(p + 8), 325u);
        EXPECT_EQ(read_u16_be(p + 10), 8u);
    }

    std::shared_ptr<Signal> m_signal;
    std::shared_ptr<EventQueue> m_eventQueue;
    std::shared_ptr<SwitchConfig> m_sc;
    std::shared_ptr<SwitchConfigContainer> m_scc;
    std::shared_ptr<RealObjectIdManager> m_ridmgr;
    std::shared_ptr<SwitchStateBase> m_ss;

    sai_object_id_t m_switch_id;
    sai_object_id_t m_tam_id;
    sai_object_id_t m_tam_tel_id;
    sai_object_id_t m_tam_tel_type_id;
};

// ---- Test Case 1: Empty subscription ----

TEST_F(TAMIpfixTemplateTest, EmptySubscription_GeneratesMinimalTemplate)
{
    // No counter subscriptions created — should not crash (Bug 1 regression)
    // and should produce a minimal template with only the timestamp field.
    auto tmpl = getIpfixTemplates();

    // Should have exactly 12 bytes: set header(4) + template header(4) + timestamp field(4)
    EXPECT_EQ(tmpl.size(), 12u);
    validateTemplateHeader(tmpl, 1); // 1 field = observationTimeNanoseconds only
}

// ---- Test Case 2: Single counter_subscription ----

TEST_F(TAMIpfixTemplateTest, SingleSubscription_CorrectBinaryFormat)
{
    // Create a port object to subscribe to
    sai_object_id_t port_id = m_ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_TAM, m_switch_id);
    ASSERT_EQ(SAI_STATUS_SUCCESS,
              m_ss->create(SAI_OBJECT_TYPE_TAM,
                           sai_serialize_object_id(port_id), m_switch_id, 0, nullptr));

    uint64_t label = 1;
    uint32_t stat_id = 0x0001; // e.g. SAI_PORT_STAT_IF_IN_OCTETS

    createCounterSubscription(m_tam_tel_type_id, label, stat_id, port_id);

    auto tmpl = getIpfixTemplates();

    // Expected size: 12 (header + timestamp field) + 8 (1 enterprise field) = 20
    EXPECT_EQ(tmpl.size(), 20u);
    validateTemplateHeader(tmpl, 2); // 2 fields: timestamp + 1 counter

    // Validate enterprise field at offset 12
    const uint8_t *field = tmpl.data() + 12;

    // Element ID = label | 0x8000 (enterprise bit set)
    uint16_t element_id = read_u16_be(field);
    EXPECT_EQ(element_id, static_cast<uint16_t>(label | 0x8000));

    // Field Length = 8
    EXPECT_EQ(read_u16_be(field + 2), 8u);

    // Enterprise Number = (stat_id << 16) | object_type
    uint32_t enterprise_num = read_u32_be(field + 4);
    uint32_t expected_obj_type = static_cast<uint32_t>(SAI_OBJECT_TYPE_TAM);
    uint32_t expected_enterprise = (stat_id << 16) | (expected_obj_type & 0xFFFF);
    EXPECT_EQ(enterprise_num, expected_enterprise);
}

// ---- Test Case 3: Multiple counter_subscriptions ----

TEST_F(TAMIpfixTemplateTest, MultipleSubscriptions_CorrectFieldCountAndEncoding)
{
    sai_object_id_t port_id = m_ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_TAM, m_switch_id);
    ASSERT_EQ(SAI_STATUS_SUCCESS,
              m_ss->create(SAI_OBJECT_TYPE_TAM,
                           sai_serialize_object_id(port_id), m_switch_id, 0, nullptr));

    // Create 3 counter subscriptions with different labels and stat IDs
    struct SubSpec {
        uint64_t label;
        uint32_t stat_id;
    };

    SubSpec specs[] = {
        {1, 0x0001}, // IF_IN_OCTETS
        {2, 0x0002}, // IF_IN_UCAST_PKTS
        {3, 0x0003}, // IF_OUT_OCTETS
    };

    for (auto &s : specs)
    {
        createCounterSubscription(m_tam_tel_type_id, s.label, s.stat_id, port_id);
    }

    auto tmpl = getIpfixTemplates();

    // Expected size: 12 + 3 * 8 = 36
    EXPECT_EQ(tmpl.size(), 36u);

    // 4 fields total: 1 timestamp + 3 counters
    validateTemplateHeader(tmpl, 4);

    // Validate each enterprise field
    for (size_t i = 0; i < 3; i++)
    {
        const uint8_t *field = tmpl.data() + 12 + i * 8;
        uint16_t element_id = read_u16_be(field);

        // Enterprise bit must be set
        EXPECT_NE(element_id & 0x8000, 0u)
            << "Enterprise bit not set for subscription " << i;

        // Field length = 8
        EXPECT_EQ(read_u16_be(field + 2), 8u);

        // Enterprise number should be non-zero
        uint32_t enterprise_num = read_u32_be(field + 4);
        EXPECT_NE(enterprise_num, 0u)
            << "Enterprise number is zero for subscription " << i;
    }
}

// ---- Test Case 4: Subscription not matching tel_type ----

TEST_F(TAMIpfixTemplateTest, UnmatchedSubscription_Filtered)
{
    sai_object_id_t port_id = m_ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_TAM, m_switch_id);
    ASSERT_EQ(SAI_STATUS_SUCCESS,
              m_ss->create(SAI_OBJECT_TYPE_TAM,
                           sai_serialize_object_id(port_id), m_switch_id, 0, nullptr));

    // Create a second tel_type that we'll reference from one subscription
    sai_object_id_t other_tel_type_id = m_ridmgr->allocateNewObjectId(SAI_OBJECT_TYPE_TAM_TEL_TYPE, m_switch_id);
    ASSERT_EQ(SAI_STATUS_SUCCESS,
              m_ss->create(SAI_OBJECT_TYPE_TAM_TEL_TYPE,
                           sai_serialize_object_id(other_tel_type_id), m_switch_id, 0, nullptr));

    // Subscription 1: references our tel_type — should be included
    createCounterSubscription(m_tam_tel_type_id, 1, 0x0001, port_id);

    // Subscription 2: references OTHER tel_type — should be filtered out
    createCounterSubscription(other_tel_type_id, 2, 0x0002, port_id);

    // Subscription 3: references our tel_type — should be included
    createCounterSubscription(m_tam_tel_type_id, 3, 0x0003, port_id);

    auto tmpl = getIpfixTemplates();

    // Only 2 matching subscriptions + 1 timestamp = 3 fields
    // Size: 12 + 2 * 8 = 28
    EXPECT_EQ(tmpl.size(), 28u);
    validateTemplateHeader(tmpl, 3);
}
