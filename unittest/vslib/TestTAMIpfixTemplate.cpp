#include "SwitchStateBase.h"

#include "meta/sai_serialize.h"
#include "swss/logger.h"

#include <gtest/gtest.h>

#include <memory>
#include <vector>

using namespace saivs;

namespace
{
    uint16_t readU16Be(const uint8_t *data)
    {
        // SWSS_LOG_ENTER() omitted - test decoding helper.
        return static_cast<uint16_t>((data[0] << 8) | data[1]);
    }

    uint32_t readU32Be(const uint8_t *data)
    {
        // SWSS_LOG_ENTER() omitted - test decoding helper.
        return (static_cast<uint32_t>(data[0]) << 24) |
               (static_cast<uint32_t>(data[1]) << 16) |
               (static_cast<uint32_t>(data[2]) << 8) |
               static_cast<uint32_t>(data[3]);
    }
}

class TamIpfixTemplateTest : public ::testing::Test
{
    protected:
        void SetUp() override
        {
            auto signal = std::make_shared<Signal>();
            auto queue = std::make_shared<EventQueue>(signal);
            auto config = std::make_shared<SwitchConfig>(0, "");
            config->m_eventQueue = queue;
            auto configs = std::make_shared<SwitchConfigContainer>();
            m_manager = std::make_shared<RealObjectIdManager>(0, configs);
            m_switch = std::make_shared<SwitchStateBase>(
                    m_switchId,
                    m_manager,
                    config);

            sai_attribute_t notify{};
            notify.id = SAI_SWITCH_ATTR_TAM_TEL_TYPE_CONFIG_CHANGE_NOTIFY;
            ASSERT_EQ(
                    SAI_STATUS_SUCCESS,
                    m_switch->create(
                            SAI_OBJECT_TYPE_SWITCH,
                            sai_serialize_object_id(m_switchId),
                            SAI_NULL_OBJECT_ID,
                            1,
                            &notify));

            m_telType = createObject(SAI_OBJECT_TYPE_TAM_TEL_TYPE);
        }

        sai_object_id_t createObject(sai_object_type_t type)
        {
            SWSS_LOG_ENTER();

            sai_object_id_t object = m_manager->allocateNewObjectId(
                    type,
                    m_switchId);
            EXPECT_EQ(
                    SAI_STATUS_SUCCESS,
                    m_switch->create_internal(
                            type,
                            sai_serialize_object_id(object),
                            m_switchId,
                            0,
                            nullptr));
            return object;
        }

        void createSubscription(
                sai_object_id_t telType,
                sai_object_id_t object,
                uint16_t label,
                sai_stat_id_t stat)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attrs[4]{};
            attrs[0].id = SAI_TAM_COUNTER_SUBSCRIPTION_ATTR_TEL_TYPE;
            attrs[0].value.oid = telType;
            attrs[1].id = SAI_TAM_COUNTER_SUBSCRIPTION_ATTR_OBJECT_ID;
            attrs[1].value.oid = object;
            attrs[2].id = SAI_TAM_COUNTER_SUBSCRIPTION_ATTR_STAT_ID;
            attrs[2].value.u32 = static_cast<uint32_t>(stat);
            attrs[3].id = SAI_TAM_COUNTER_SUBSCRIPTION_ATTR_LABEL;
            attrs[3].value.u64 = label;

            sai_object_id_t subscription = m_manager->allocateNewObjectId(
                    SAI_OBJECT_TYPE_TAM_COUNTER_SUBSCRIPTION,
                    m_switchId);
            ASSERT_EQ(
                    SAI_STATUS_SUCCESS,
                    m_switch->create_internal(
                            SAI_OBJECT_TYPE_TAM_COUNTER_SUBSCRIPTION,
                            sai_serialize_object_id(subscription),
                            m_switchId,
                            4,
                            attrs));
        }

        std::vector<uint8_t> getTemplate()
        {
            SWSS_LOG_ENTER();

            sai_attribute_t state{};
            state.id = SAI_TAM_TEL_TYPE_ATTR_STATE;
            state.value.s32 = SAI_TAM_TEL_TYPE_STATE_CREATE_CONFIG;
            EXPECT_EQ(
                    SAI_STATUS_SUCCESS,
                    m_switch->set(
                            SAI_OBJECT_TYPE_TAM_TEL_TYPE,
                            sai_serialize_object_id(m_telType),
                            &state));

            std::vector<uint8_t> bytes(65535);
            sai_attribute_t value{};
            value.id = SAI_TAM_TEL_TYPE_ATTR_IPFIX_TEMPLATES;
            value.value.u8list.count = static_cast<uint32_t>(bytes.size());
            value.value.u8list.list = bytes.data();
            EXPECT_EQ(
                    SAI_STATUS_SUCCESS,
                    m_switch->get(
                            SAI_OBJECT_TYPE_TAM_TEL_TYPE,
                            sai_serialize_object_id(m_telType),
                            1,
                            &value));
            bytes.resize(value.value.u8list.count);
            return bytes;
        }

        const sai_object_id_t m_switchId = 0x2100000000;
        sai_object_id_t m_telType = SAI_NULL_OBJECT_ID;
        std::shared_ptr<RealObjectIdManager> m_manager;
        std::shared_ptr<SwitchStateBase> m_switch;
};

TEST_F(TamIpfixTemplateTest, EmitsACompleteVersionedPortTemplate)
{
    auto port = createObject(SAI_OBJECT_TYPE_PORT);
    createSubscription(
            m_telType,
            port,
            1,
            SAI_PORT_STAT_IF_IN_OCTETS);

    auto first = getTemplate();
    ASSERT_EQ(36u, first.size());
    EXPECT_EQ(10u, readU16Be(first.data()));
    EXPECT_EQ(first.size(), readU16Be(first.data() + 2));
    EXPECT_NE(0u, readU32Be(first.data() + 12));
    EXPECT_EQ(2u, readU16Be(first.data() + 16));
    EXPECT_EQ(20u, readU16Be(first.data() + 18));
    EXPECT_EQ(2u, readU16Be(first.data() + 22));
    EXPECT_EQ(325u, readU16Be(first.data() + 24));
    EXPECT_EQ(8u, readU16Be(first.data() + 26));
    EXPECT_EQ(0x8001u, readU16Be(first.data() + 28));
    EXPECT_EQ(8u, readU16Be(first.data() + 30));
    EXPECT_EQ(
            (static_cast<uint32_t>(SAI_OBJECT_TYPE_PORT) << 16) |
                    SAI_PORT_STAT_IF_IN_OCTETS,
            readU32Be(first.data() + 32));

    auto second = getTemplate();
    EXPECT_EQ(readU32Be(first.data() + 12), readU32Be(second.data() + 12));
    EXPECT_LT(readU16Be(first.data() + 20), readU16Be(second.data() + 20));
}

TEST_F(TamIpfixTemplateTest, FiltersSubscriptionsForOtherTelemetryTypes)
{
    auto port = createObject(SAI_OBJECT_TYPE_PORT);
    auto otherTelType = createObject(SAI_OBJECT_TYPE_TAM_TEL_TYPE);
    createSubscription(m_telType, port, 1, SAI_PORT_STAT_IF_IN_OCTETS);
    createSubscription(otherTelType, port, 2, SAI_PORT_STAT_IF_IN_UCAST_PKTS);
    createSubscription(m_telType, port, 3, SAI_PORT_STAT_IF_OUT_OCTETS);

    auto value = getTemplate();
    ASSERT_EQ(44u, value.size());
    EXPECT_EQ(3u, readU16Be(value.data() + 22));
    EXPECT_EQ(0x8001u, readU16Be(value.data() + 28));
    EXPECT_EQ(0x8003u, readU16Be(value.data() + 36));
}
