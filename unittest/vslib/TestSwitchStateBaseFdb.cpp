#include "SwitchBCM56850.h"
#include "EventPayloadNotification.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <linux/if_ether.h>

#include <memory>
#include <vector>

using namespace saivs;

static void dummyFdbEventNotify(
        _In_ uint32_t count,
        _In_ const sai_fdb_event_notification_data_t *data)
{
    SWSS_LOG_ENTER();
}

class SwitchStateBaseFdbTest : public ::testing::Test
{
    public:

        void SetUp() override
        {
            m_eventQueue = std::make_shared<EventQueue>(std::make_shared<Signal>());

            auto sc = std::make_shared<SwitchConfig>(0, "");

            sc->m_saiSwitchType = SAI_SWITCH_TYPE_NPU;
            sc->m_switchType = SAI_VS_SWITCH_TYPE_BCM56850;
            sc->m_bootType = SAI_VS_BOOT_TYPE_COLD;
            sc->m_useTapDevice = false;
            sc->m_laneMap = LaneMap::getDefaultLaneMap(0);
            sc->m_eventQueue = m_eventQueue;

            auto scc = std::make_shared<SwitchConfigContainer>();

            scc->insert(sc);

            m_sw = std::make_shared<SwitchBCM56850>(
                    m_switchId,
                    std::make_shared<RealObjectIdManager>(0, scc),
                    sc);

            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_INIT_SWITCH;
            attr.value.booldata = true;

            ASSERT_EQ(m_sw->initialize_default_objects(1, &attr), SAI_STATUS_SUCCESS);

            attr.id = SAI_SWITCH_ATTR_FDB_EVENT_NOTIFY;
            attr.value.ptr = (void*)&dummyFdbEventNotify;

            ASSERT_EQ(m_sw->set(SAI_OBJECT_TYPE_SWITCH, m_switchId, &attr), SAI_STATUS_SUCCESS);

            attr.id = SAI_SWITCH_ATTR_DEFAULT_1Q_BRIDGE_ID;

            ASSERT_EQ(m_sw->get(SAI_OBJECT_TYPE_SWITCH, m_switchId, 1, &attr), SAI_STATUS_SUCCESS);

            m_bridgeId = attr.value.oid;

            m_vlan1 = findVlan(1);

            ASSERT_NE(m_vlan1, SAI_NULL_OBJECT_ID);
        }

        sai_object_id_t findVlan(
                _In_ uint16_t vlanId)
        {
            SWSS_LOG_ENTER();

            for (auto& v: m_sw->m_objectHash.at(SAI_OBJECT_TYPE_VLAN))
            {
                sai_object_id_t oid;
                sai_deserialize_object_id(v.first, oid);

                sai_attribute_t attr;
                attr.id = SAI_VLAN_ATTR_VLAN_ID;

                if (m_sw->get(SAI_OBJECT_TYPE_VLAN, oid, 1, &attr) == SAI_STATUS_SUCCESS && attr.value.u16 == vlanId)
                {
                    return oid;
                }
            }

            return SAI_NULL_OBJECT_ID;
        }

        sai_object_id_t createVlan(
                _In_ uint16_t vlanId)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_VLAN_ATTR_VLAN_ID;
            attr.value.u16 = vlanId;

            sai_object_id_t oid = SAI_NULL_OBJECT_ID;

            EXPECT_EQ(m_sw->create(SAI_OBJECT_TYPE_VLAN, &oid, m_switchId, 1, &attr), SAI_STATUS_SUCCESS);

            return oid;
        }

        sai_object_id_t portBridgePort(
                _In_ sai_object_id_t portId)
        {
            SWSS_LOG_ENTER();

            for (auto& bp: m_sw->m_objectHash.at(SAI_OBJECT_TYPE_BRIDGE_PORT))
            {
                sai_object_id_t oid;
                sai_deserialize_object_id(bp.first, oid);

                sai_attribute_t attr;
                attr.id = SAI_BRIDGE_PORT_ATTR_PORT_ID;

                if (m_sw->get(SAI_OBJECT_TYPE_BRIDGE_PORT, oid, 1, &attr) == SAI_STATUS_SUCCESS && attr.value.oid == portId)
                {
                    return oid;
                }
            }

            return SAI_NULL_OBJECT_ID;
        }

        sai_object_id_t createBridgePort(
                _In_ sai_object_id_t portId)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attrs[4];

            attrs[0].id = SAI_BRIDGE_PORT_ATTR_TYPE;
            attrs[0].value.s32 = SAI_BRIDGE_PORT_TYPE_PORT;
            attrs[1].id = SAI_BRIDGE_PORT_ATTR_PORT_ID;
            attrs[1].value.oid = portId;
            attrs[2].id = SAI_BRIDGE_PORT_ATTR_BRIDGE_ID;
            attrs[2].value.oid = m_bridgeId;
            attrs[3].id = SAI_BRIDGE_PORT_ATTR_FDB_LEARNING_MODE;
            attrs[3].value.s32 = SAI_BRIDGE_PORT_FDB_LEARNING_MODE_HW;

            sai_object_id_t oid = SAI_NULL_OBJECT_ID;

            EXPECT_EQ(m_sw->create(SAI_OBJECT_TYPE_BRIDGE_PORT, &oid, m_switchId, 4, attrs), SAI_STATUS_SUCCESS);

            return oid;
        }

        void sendFrame(
                _In_ sai_object_id_t portId,
                _In_ const sai_mac_t& srcMac,
                _In_ uint16_t vlanId = 0)
        {
            SWSS_LOG_ENTER();

            std::vector<uint8_t> frame(64, 0);

            ethhdr* eh = (ethhdr*)frame.data();

            memset(eh->h_dest, 0xff, ETH_ALEN);
            memcpy(eh->h_source, srcMac, ETH_ALEN);

            if (vlanId)
            {
                eh->h_proto = htons(ETH_P_8021Q);

                uint16_t tci = htons(vlanId);
                memcpy(frame.data() + sizeof(ethhdr), &tci, sizeof(tci));
            }
            else
            {
                eh->h_proto = htons(ETH_P_IP);
            }

            m_sw->process_packet_for_fdb_event(portId, "Ethernet", frame.data(), frame.size());
        }

        sai_fdb_entry_t fdbEntry(
                _In_ const sai_mac_t& mac,
                _In_ sai_object_id_t bvId)
        {
            SWSS_LOG_ENTER();

            sai_fdb_entry_t fe;

            memset(&fe, 0, sizeof(fe));

            fe.switch_id = m_switchId;
            fe.bv_id = bvId;
            memcpy(fe.mac_address, mac, sizeof(sai_mac_t));

            return fe;
        }

        bool fdbEntryExists(
                _In_ const sai_fdb_entry_t& fe)
        {
            SWSS_LOG_ENTER();

            auto& fdbs = m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY);

            return fdbs.find(sai_serialize_fdb_entry(fe)) != fdbs.end();
        }

        sai_attribute_t getFdbAttr(
                _In_ const sai_fdb_entry_t& fe,
                _In_ sai_attr_id_t id)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = id;

            EXPECT_EQ(m_sw->get(SAI_OBJECT_TYPE_FDB_ENTRY, sai_serialize_fdb_entry(fe), 1, &attr), SAI_STATUS_SUCCESS);

            return attr;
        }

        void setAgingTime(
                _In_ uint32_t seconds)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = SAI_SWITCH_ATTR_FDB_AGING_TIME;
            attr.value.u32 = seconds;

            ASSERT_EQ(m_sw->set(SAI_OBJECT_TYPE_SWITCH, m_switchId, &attr), SAI_STATUS_SUCCESS);
        }

        // moves the last time the MAC was seen into the past
        void backdate(
                _In_ const sai_mac_t& mac,
                _In_ uint16_t vlanId,
                _In_ uint32_t seconds)
        {
            SWSS_LOG_ENTER();

            FdbInfo key;

            key.setVlanId(vlanId);
            memcpy(key.m_fdbEntry.mac_address, mac, sizeof(sai_mac_t));

            auto it = m_sw->m_fdb_info_set.find(key);

            ASSERT_NE(it, m_sw->m_fdb_info_set.end());

            FdbInfo fi = *it;

            fi.setTimestamp(fi.getTimestamp() - seconds);

            m_sw->m_fdb_info_set.erase(it);
            m_sw->m_fdb_info_set.insert(fi);
        }

        uint32_t timestamp(
                _In_ const sai_mac_t& mac,
                _In_ uint16_t vlanId)
        {
            SWSS_LOG_ENTER();

            FdbInfo key;

            key.setVlanId(vlanId);
            memcpy(key.m_fdbEntry.mac_address, mac, sizeof(sai_mac_t));

            auto it = m_sw->m_fdb_info_set.find(key);

            return (it == m_sw->m_fdb_info_set.end()) ? 0 : it->getTimestamp();
        }

        // fdb events sent since the last call
        std::vector<sai_fdb_event_t> fdbEvents()
        {
            SWSS_LOG_ENTER();

            std::vector<sai_fdb_event_t> events;

            while (m_eventQueue->size())
            {
                auto event = m_eventQueue->dequeue();

                auto payload = std::dynamic_pointer_cast<EventPayloadNotification>(event->getPayload());

                if (!payload || payload->getNotification()->getNotificationType() != SAI_SWITCH_NOTIFICATION_TYPE_FDB_EVENT)
                {
                    continue;
                }

                uint32_t count;
                sai_fdb_event_notification_data_t* data = nullptr;

                sai_deserialize_fdb_event_ntf(payload->getNotification()->getSerializedNotification(), count, &data);

                for (uint32_t i = 0; i < count; i++)
                {
                    events.push_back(data[i].event_type);
                }

                sai_deserialize_free_fdb_event_ntf(count, data);
            }

            return events;
        }

    protected:

        const sai_object_id_t m_switchId = 0x2100000000;

        std::shared_ptr<EventQueue> m_eventQueue;

        std::shared_ptr<SwitchBCM56850> m_sw;

        sai_object_id_t m_bridgeId = SAI_NULL_OBJECT_ID;

        sai_object_id_t m_vlan1 = SAI_NULL_OBJECT_ID;
};

TEST_F(SwitchStateBaseFdbTest, untaggedFrameOnLagMemberLearnsInLagPortVlan)
{
    auto vlan10 = createVlan(10);

    auto port = m_sw->m_port_list.at(0);

    // a lag member has no bridge port of its own
    ASSERT_EQ(m_sw->remove(SAI_OBJECT_TYPE_BRIDGE_PORT, portBridgePort(port)), SAI_STATUS_SUCCESS);

    sai_attribute_t attr;

    attr.id = SAI_LAG_ATTR_PORT_VLAN_ID;
    attr.value.u16 = 10;

    sai_object_id_t lag;
    ASSERT_EQ(m_sw->create(SAI_OBJECT_TYPE_LAG, &lag, m_switchId, 1, &attr), SAI_STATUS_SUCCESS);

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_LAG_MEMBER_ATTR_LAG_ID;
    attrs[0].value.oid = lag;
    attrs[1].id = SAI_LAG_MEMBER_ATTR_PORT_ID;
    attrs[1].value.oid = port;

    sai_object_id_t member;
    ASSERT_EQ(m_sw->create(SAI_OBJECT_TYPE_LAG_MEMBER, &member, m_switchId, 2, attrs), SAI_STATUS_SUCCESS);

    auto lagBridgePort = createBridgePort(lag);

    sai_mac_t mac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x01 };

    sendFrame(port, mac);

    EXPECT_TRUE(fdbEntryExists(fdbEntry(mac, vlan10)));
    EXPECT_FALSE(fdbEntryExists(fdbEntry(mac, m_vlan1)));
    EXPECT_EQ(getFdbAttr(fdbEntry(mac, vlan10), SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID).value.oid, lagBridgePort);

    // the lag port VLAN defaults to 1 when it was never set
    attr.id = SAI_LAG_ATTR_PORT_VLAN_ID;
    ASSERT_EQ(m_sw->m_objectHash.at(SAI_OBJECT_TYPE_LAG).at(sai_serialize_object_id(lag)).erase(sai_metadata_get_attr_metadata(SAI_OBJECT_TYPE_LAG, attr.id)->attridname), 1u);

    sai_mac_t mac2 = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x02 };

    sendFrame(port, mac2);

    EXPECT_TRUE(fdbEntryExists(fdbEntry(mac2, m_vlan1)));
}

TEST_F(SwitchStateBaseFdbTest, trafficRefreshesLearnedEntryAge)
{
    auto port = m_sw->m_port_list.at(0);

    sai_mac_t mac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x03 };

    auto fe = fdbEntry(mac, m_vlan1);

    sendFrame(port, mac);

    ASSERT_TRUE(fdbEntryExists(fe));
    EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_LEARNED });

    ASSERT_NO_FATAL_FAILURE(setAgingTime(60));

    // first seen 100 s ago, seen again now: must not age out
    ASSERT_NO_FATAL_FAILURE(backdate(mac, 1, 100));

    sendFrame(port, mac);

    EXPECT_GE(timestamp(mac, 1) + 5, (uint32_t)time(NULL));

    m_sw->processFdbEntriesForAging();

    EXPECT_TRUE(fdbEntryExists(fe));
    EXPECT_TRUE(fdbEvents().empty());

    // no traffic for longer than the aging time: ages out
    ASSERT_NO_FATAL_FAILURE(backdate(mac, 1, 100));

    m_sw->processFdbEntriesForAging();

    EXPECT_FALSE(fdbEntryExists(fe));
    EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_AGED });
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
}

TEST_F(SwitchStateBaseFdbTest, learnedEntryRemovedByUserIsLearnedAgainAndNotAged)
{
    auto port = m_sw->m_port_list.at(0);

    sai_mac_t mac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x04 };

    auto fe = fdbEntry(mac, m_vlan1);

    sendFrame(port, mac);

    ASSERT_TRUE(fdbEntryExists(fe));
    fdbEvents();

    ASSERT_EQ(m_sw->remove(SAI_OBJECT_TYPE_FDB_ENTRY, sai_serialize_fdb_entry(fe)), SAI_STATUS_SUCCESS);

    sendFrame(port, mac);

    EXPECT_TRUE(fdbEntryExists(fe));
    EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_LEARNED });

    ASSERT_EQ(m_sw->remove(SAI_OBJECT_TYPE_FDB_ENTRY, sai_serialize_fdb_entry(fe)), SAI_STATUS_SUCCESS);

    ASSERT_NO_FATAL_FAILURE(setAgingTime(60));

    ASSERT_NO_FATAL_FAILURE(backdate(mac, 1, 100));

    m_sw->processFdbEntriesForAging();

    EXPECT_TRUE(fdbEvents().empty());
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
}
