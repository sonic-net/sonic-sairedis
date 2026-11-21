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

        struct FdbEvent
        {
            sai_fdb_event_t type;

            sai_object_id_t bridgePortId;
        };

        // fdb events sent since the last call, with the bridge port they name
        std::vector<FdbEvent> fdbEventsWithBridgePort()
        {
            SWSS_LOG_ENTER();

            std::vector<FdbEvent> events;

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
                    FdbEvent e = { data[i].event_type, SAI_NULL_OBJECT_ID };

                    for (uint32_t a = 0; a < data[i].attr_count; a++)
                    {
                        if (data[i].attr[a].id == SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID)
                        {
                            e.bridgePortId = data[i].attr[a].value.oid;
                        }
                    }

                    events.push_back(e);
                }

                sai_deserialize_free_fdb_event_ntf(count, data);
            }

            return events;
        }

        // a lag on the given member port with a bridge port, which is an EVPN Ethernet segment (ES) when it has a protection group
        sai_object_id_t createLagBridgePort(
                _In_ sai_object_id_t port,
                _In_ bool withProtectionGroup)
        {
            SWSS_LOG_ENTER();

            EXPECT_EQ(m_sw->remove(SAI_OBJECT_TYPE_BRIDGE_PORT, portBridgePort(port)), SAI_STATUS_SUCCESS);

            sai_object_id_t lag = SAI_NULL_OBJECT_ID;

            EXPECT_EQ(m_sw->create(SAI_OBJECT_TYPE_LAG, &lag, m_switchId, 0, nullptr), SAI_STATUS_SUCCESS);

            sai_attribute_t attrs[2];

            attrs[0].id = SAI_LAG_MEMBER_ATTR_LAG_ID;
            attrs[0].value.oid = lag;
            attrs[1].id = SAI_LAG_MEMBER_ATTR_PORT_ID;
            attrs[1].value.oid = port;

            sai_object_id_t member = SAI_NULL_OBJECT_ID;

            EXPECT_EQ(m_sw->create(SAI_OBJECT_TYPE_LAG_MEMBER, &member, m_switchId, 2, attrs), SAI_STATUS_SUCCESS);

            auto bp = createBridgePort(lag);

            if (withProtectionGroup)
            {
                sai_attribute_t attr;

                attr.id = SAI_NEXT_HOP_GROUP_ATTR_TYPE;
                attr.value.s32 = SAI_NEXT_HOP_GROUP_TYPE_BRIDGE_PORT;

                sai_object_id_t nhg = SAI_NULL_OBJECT_ID;

                EXPECT_EQ(m_sw->create(SAI_OBJECT_TYPE_NEXT_HOP_GROUP, &nhg, m_switchId, 1, &attr), SAI_STATUS_SUCCESS);

                attr.id = SAI_BRIDGE_PORT_ATTR_BRIDGE_PORT_PROTECTION_NEXT_HOP_GROUP_ID;
                attr.value.oid = nhg;

                EXPECT_EQ(m_sw->set(SAI_OBJECT_TYPE_BRIDGE_PORT, bp, &attr), SAI_STATUS_SUCCESS);
            }

            return bp;
        }

        // the entry an ES peer's MAC gets on the local ES: static, allowing MAC move
        void createStaticEntry(
                _In_ const sai_fdb_entry_t& fe,
                _In_ sai_object_id_t bridgePortId,
                _In_ bool allowMacMove)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attrs[3];

            attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
            attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
            attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
            attrs[1].value.oid = bridgePortId;
            attrs[2].id = SAI_FDB_ENTRY_ATTR_ALLOW_MAC_MOVE;
            attrs[2].value.booldata = allowMacMove;

            ASSERT_EQ(m_sw->create(SAI_OBJECT_TYPE_FDB_ENTRY, sai_serialize_fdb_entry(fe), m_switchId, 3, attrs), SAI_STATUS_SUCCESS);
        }

        void setFdbAttr(
                _In_ const sai_fdb_entry_t& fe,
                _In_ sai_attr_id_t id,
                _In_ int32_t value)
        {
            SWSS_LOG_ENTER();

            sai_attribute_t attr;

            attr.id = id;

            if (id == SAI_FDB_ENTRY_ATTR_ALLOW_MAC_MOVE)
            {
                attr.value.booldata = value != 0;
            }
            else
            {
                attr.value.s32 = value;
            }

            ASSERT_EQ(m_sw->set(SAI_OBJECT_TYPE_FDB_ENTRY, sai_serialize_fdb_entry(fe), &attr), SAI_STATUS_SUCCESS);
        }

        // moves the last learn reported for an ES peer entry into the past
        void backdateEsPeerLearn(
                _In_ const sai_fdb_entry_t& fe,
                _In_ uint32_t seconds)
        {
            SWSS_LOG_ENTER();

            auto it = m_sw->m_es_peer_fdb_learns.find(sai_serialize_fdb_entry(fe));

            ASSERT_NE(it, m_sw->m_es_peer_fdb_learns.end());

            it->second.lastReport -= seconds;
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

TEST_F(SwitchStateBaseFdbTest, learnedMacOnAnotherPortIsMoved)
{
    auto port0 = m_sw->m_port_list.at(0);
    auto port1 = m_sw->m_port_list.at(1);

    sai_mac_t mac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x05 };

    auto fe = fdbEntry(mac, m_vlan1);

    sendFrame(port0, mac);

    ASSERT_TRUE(fdbEntryExists(fe));
    EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_LEARNED });

    sendFrame(port1, mac);

    EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_MOVE });
    EXPECT_EQ(getFdbAttr(fe, SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID).value.oid, portBridgePort(port1));
    ASSERT_EQ(m_sw->m_fdb_info_set.size(), 1u);
    EXPECT_EQ(m_sw->m_fdb_info_set.begin()->getPortId(), port1);

    // further traffic on the new port only refreshes the entry
    sendFrame(port1, mac);

    EXPECT_TRUE(fdbEvents().empty());

    // and moving back is reported again
    sendFrame(port0, mac);

    EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_MOVE });
    EXPECT_EQ(getFdbAttr(fe, SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID).value.oid, portBridgePort(port0));
}

TEST_F(SwitchStateBaseFdbTest, staticEntryIsNotLearnedOverOrMoved)
{
    auto port0 = m_sw->m_port_list.at(0);
    auto port1 = m_sw->m_port_list.at(1);

    sai_mac_t mac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x06 };

    auto fe = fdbEntry(mac, m_vlan1);

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = portBridgePort(port0);

    ASSERT_EQ(m_sw->create(SAI_OBJECT_TYPE_FDB_ENTRY, sai_serialize_fdb_entry(fe), m_switchId, 2, attrs), SAI_STATUS_SUCCESS);

    sendFrame(port0, mac);
    sendFrame(port1, mac);

    EXPECT_TRUE(fdbEvents().empty());
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
    EXPECT_EQ(getFdbAttr(fe, SAI_FDB_ENTRY_ATTR_TYPE).value.s32, SAI_FDB_ENTRY_TYPE_STATIC);
    EXPECT_EQ(getFdbAttr(fe, SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID).value.oid, portBridgePort(port0));
}

TEST_F(SwitchStateBaseFdbTest, staticEntryAllowingMacMoveIsMoved)
{
    auto port0 = m_sw->m_port_list.at(0);
    auto port1 = m_sw->m_port_list.at(1);

    sai_mac_t mac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x07 };

    auto fe = fdbEntry(mac, m_vlan1);

    sai_attribute_t attrs[3];

    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = portBridgePort(port0);
    attrs[2].id = SAI_FDB_ENTRY_ATTR_ALLOW_MAC_MOVE;
    attrs[2].value.booldata = true;

    ASSERT_EQ(m_sw->create(SAI_OBJECT_TYPE_FDB_ENTRY, sai_serialize_fdb_entry(fe), m_switchId, 3, attrs), SAI_STATUS_SUCCESS);

    sendFrame(port1, mac);

    EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_MOVE });
    EXPECT_EQ(getFdbAttr(fe, SAI_FDB_ENTRY_ATTR_TYPE).value.s32, SAI_FDB_ENTRY_TYPE_DYNAMIC);
    EXPECT_EQ(getFdbAttr(fe, SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID).value.oid, portBridgePort(port1));
    EXPECT_EQ(m_sw->m_objectHash.at(SAI_OBJECT_TYPE_FDB_ENTRY).at(sai_serialize_fdb_entry(fe)).count("SAI_FDB_ENTRY_ATTR_ALLOW_MAC_MOVE"), 0u);
    EXPECT_EQ(m_sw->m_fdb_info_set.size(), 1u);
}

TEST_F(SwitchStateBaseFdbTest, dynamicEntryCreatedByUserAgesOnTraffic)
{
    auto port0 = m_sw->m_port_list.at(0);

    sai_mac_t mac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x08 };

    auto fe = fdbEntry(mac, m_vlan1);

    sai_attribute_t attrs[2];

    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;
    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = portBridgePort(port0);

    ASSERT_EQ(m_sw->create(SAI_OBJECT_TYPE_FDB_ENTRY, sai_serialize_fdb_entry(fe), m_switchId, 2, attrs), SAI_STATUS_SUCCESS);

    sendFrame(port0, mac);

    EXPECT_TRUE(fdbEvents().empty());
    EXPECT_EQ(m_sw->m_fdb_info_set.size(), 1u);

    ASSERT_NO_FATAL_FAILURE(setAgingTime(60));

    ASSERT_NO_FATAL_FAILURE(backdate(mac, 1, 100));

    m_sw->processFdbEntriesForAging();

    EXPECT_FALSE(fdbEntryExists(fe));
    EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_AGED });
}

TEST_F(SwitchStateBaseFdbTest, entryChangedByUserToDynamicIsTracked)
{
    auto port0 = m_sw->m_port_list.at(0);
    auto port1 = m_sw->m_port_list.at(1);

    sai_mac_t mac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x09 };

    auto fe = fdbEntry(mac, m_vlan1);
    auto sid = sai_serialize_fdb_entry(fe);

    // static entry allowing MAC move, like a remote entry on a tunnel bridge port
    sai_attribute_t attrs[3];

    attrs[0].id = SAI_FDB_ENTRY_ATTR_TYPE;
    attrs[0].value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;
    attrs[1].id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attrs[1].value.oid = portBridgePort(port0);
    attrs[2].id = SAI_FDB_ENTRY_ATTR_ALLOW_MAC_MOVE;
    attrs[2].value.booldata = true;

    ASSERT_EQ(m_sw->create(SAI_OBJECT_TYPE_FDB_ENTRY, sid, m_switchId, 3, attrs), SAI_STATUS_SUCCESS);

    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());

    // the user makes it a local dynamic entry on another port
    sai_attribute_t attr;

    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;

    ASSERT_EQ(m_sw->set(SAI_OBJECT_TYPE_FDB_ENTRY, sid, &attr), SAI_STATUS_SUCCESS);

    attr.id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attr.value.oid = portBridgePort(port1);

    ASSERT_EQ(m_sw->set(SAI_OBJECT_TYPE_FDB_ENTRY, sid, &attr), SAI_STATUS_SUCCESS);

    ASSERT_EQ(m_sw->m_fdb_info_set.size(), 1u);
    EXPECT_EQ(m_sw->m_fdb_info_set.begin()->getBridgePortId(), portBridgePort(port1));
    EXPECT_EQ(m_sw->m_fdb_info_set.begin()->getPortId(), port1);

    // traffic on that port refreshes it, nothing is reported
    sendFrame(port1, mac);

    EXPECT_TRUE(fdbEvents().empty());
    EXPECT_EQ(m_sw->m_fdb_info_set.size(), 1u);

    // and it ages like a learned entry
    ASSERT_NO_FATAL_FAILURE(setAgingTime(60));
    ASSERT_NO_FATAL_FAILURE(backdate(mac, 1, 100));

    m_sw->processFdbEntriesForAging();

    EXPECT_FALSE(fdbEntryExists(fe));
    EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_AGED });

    // an entry made static again is not tracked
    attr.id = SAI_FDB_ENTRY_ATTR_TYPE;
    attr.value.s32 = SAI_FDB_ENTRY_TYPE_DYNAMIC;

    ASSERT_EQ(m_sw->create(SAI_OBJECT_TYPE_FDB_ENTRY, sid, m_switchId, 2, attrs), SAI_STATUS_SUCCESS);
    ASSERT_EQ(m_sw->set(SAI_OBJECT_TYPE_FDB_ENTRY, sid, &attr), SAI_STATUS_SUCCESS);

    EXPECT_EQ(m_sw->m_fdb_info_set.size(), 1u);

    attr.value.s32 = SAI_FDB_ENTRY_TYPE_STATIC;

    ASSERT_EQ(m_sw->set(SAI_OBJECT_TYPE_FDB_ENTRY, sid, &attr), SAI_STATUS_SUCCESS);

    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());
}

TEST_F(SwitchStateBaseFdbTest, esPeerEntryHitByLocalTrafficIsReportedLearned)
{
    auto port = m_sw->m_port_list.at(0);

    auto esBridgePort = createLagBridgePort(port, true);

    sai_mac_t mac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x0a };

    auto fe = fdbEntry(mac, m_vlan1);

    ASSERT_NO_FATAL_FAILURE(createStaticEntry(fe, esBridgePort, true));

    // the host's frame on the ES lag is reported as a learn on the ES bridge port
    sendFrame(port, mac);

    auto events = fdbEventsWithBridgePort();

    ASSERT_EQ(events.size(), 1u);
    EXPECT_EQ(events[0].type, SAI_FDB_EVENT_LEARNED);
    EXPECT_EQ(events[0].bridgePortId, esBridgePort);

    // the entry is left as it is, and not aged
    EXPECT_EQ(getFdbAttr(fe, SAI_FDB_ENTRY_ATTR_TYPE).value.s32, SAI_FDB_ENTRY_TYPE_STATIC);
    EXPECT_TRUE(getFdbAttr(fe, SAI_FDB_ENTRY_ATTR_ALLOW_MAC_MOVE).value.booldata);
    EXPECT_EQ(getFdbAttr(fe, SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID).value.oid, esBridgePort);
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());

    // more frames right after it raise nothing
    sendFrame(port, mac);

    EXPECT_TRUE(fdbEvents().empty());

    // the user makes it a local dynamic entry: traffic refreshes it and it ages
    ASSERT_NO_FATAL_FAILURE(setFdbAttr(fe, SAI_FDB_ENTRY_ATTR_ALLOW_MAC_MOVE, false));
    ASSERT_NO_FATAL_FAILURE(setFdbAttr(fe, SAI_FDB_ENTRY_ATTR_TYPE, SAI_FDB_ENTRY_TYPE_DYNAMIC));

    ASSERT_NO_FATAL_FAILURE(backdateEsPeerLearn(fe, 100));

    sendFrame(port, mac);

    EXPECT_TRUE(fdbEvents().empty());
    ASSERT_EQ(m_sw->m_fdb_info_set.size(), 1u);
    EXPECT_EQ(m_sw->m_fdb_info_set.begin()->getBridgePortId(), esBridgePort);

    ASSERT_NO_FATAL_FAILURE(setAgingTime(60));
    ASSERT_NO_FATAL_FAILURE(backdate(mac, 1, 100));

    m_sw->processFdbEntriesForAging();

    EXPECT_FALSE(fdbEntryExists(fe));
    EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_AGED });
    EXPECT_TRUE(m_sw->m_es_peer_fdb_learns.empty());
}

TEST_F(SwitchStateBaseFdbTest, esPeerEntryLearnIsSentAgainAFewTimes)
{
    auto port = m_sw->m_port_list.at(0);

    auto esBridgePort = createLagBridgePort(port, true);

    sai_mac_t mac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x0b };

    auto fe = fdbEntry(mac, m_vlan1);

    ASSERT_NO_FATAL_FAILURE(createStaticEntry(fe, esBridgePort, true));

    // the user drops the learn and keeps the static entry: traffic a minute later raises it again, 3 times in all
    for (int i = 0; i < 3; i++)
    {
        sendFrame(port, mac);

        EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_LEARNED }) << "learn " << i + 1;

        sendFrame(port, mac);

        EXPECT_TRUE(fdbEvents().empty()) << "learn " << i + 1;

        ASSERT_NO_FATAL_FAILURE(backdateEsPeerLearn(fe, 60));
    }

    sendFrame(port, mac);

    EXPECT_TRUE(fdbEvents().empty());
    EXPECT_TRUE(m_sw->m_es_peer_fdb_learns.at(sai_serialize_fdb_entry(fe)).warned);

    // the entry installed again on its bridge port is reported again
    sai_attribute_t attr;

    attr.id = SAI_FDB_ENTRY_ATTR_BRIDGE_PORT_ID;
    attr.value.oid = esBridgePort;

    ASSERT_EQ(m_sw->set(SAI_OBJECT_TYPE_FDB_ENTRY, sai_serialize_fdb_entry(fe), &attr), SAI_STATUS_SUCCESS);

    sendFrame(port, mac);

    EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_LEARNED });

    // so is a static entry made again
    ASSERT_EQ(m_sw->remove(SAI_OBJECT_TYPE_FDB_ENTRY, sai_serialize_fdb_entry(fe)), SAI_STATUS_SUCCESS);
    EXPECT_TRUE(m_sw->m_es_peer_fdb_learns.empty());

    ASSERT_NO_FATAL_FAILURE(createStaticEntry(fe, esBridgePort, true));

    sendFrame(port, mac);

    EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_LEARNED });
}

TEST_F(SwitchStateBaseFdbTest, staticEntryOutsideAnEsIsNotReportedOnTraffic)
{
    auto port0 = m_sw->m_port_list.at(0);
    auto port1 = m_sw->m_port_list.at(1);
    auto port2 = m_sw->m_port_list.at(2);

    auto esBridgePort = createLagBridgePort(port0, true);
    auto lagBridgePort = createLagBridgePort(port1, false);

    sai_mac_t esMac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x0c };
    sai_mac_t lagMac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x0d };
    sai_mac_t portMac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x0e };

    // a static entry not allowing MAC move on the ES, one allowing it on a lag that is no ES, and one on a port
    ASSERT_NO_FATAL_FAILURE(createStaticEntry(fdbEntry(esMac, m_vlan1), esBridgePort, false));
    ASSERT_NO_FATAL_FAILURE(createStaticEntry(fdbEntry(lagMac, m_vlan1), lagBridgePort, true));
    ASSERT_NO_FATAL_FAILURE(createStaticEntry(fdbEntry(portMac, m_vlan1), portBridgePort(port2), true));

    sendFrame(port0, esMac);
    sendFrame(port1, lagMac);
    sendFrame(port2, portMac);

    EXPECT_TRUE(fdbEvents().empty());
    EXPECT_TRUE(m_sw->m_es_peer_fdb_learns.empty());
    EXPECT_TRUE(m_sw->m_fdb_info_set.empty());

    // each of these bridge ports learns a new MAC, so the frames above did reach learning
    uint8_t last = 0x10;

    for (auto& pb: std::vector<std::pair<sai_object_id_t, sai_object_id_t>>{
            { port0, esBridgePort }, { port1, lagBridgePort }, { port2, portBridgePort(port2) } })
    {
        sai_mac_t newMac = { 0x00, 0x11, 0x22, 0x33, 0x44, last++ };

        sendFrame(pb.first, newMac);

        auto events = fdbEventsWithBridgePort();

        ASSERT_EQ(events.size(), 1u);
        EXPECT_EQ(events[0].type, SAI_FDB_EVENT_LEARNED);
        EXPECT_EQ(events[0].bridgePortId, pb.second);
    }
}

TEST_F(SwitchStateBaseFdbTest, esPeerEntryIsNotReportedWhileTheEsIsSwitchedOver)
{
    auto port = m_sw->m_port_list.at(0);

    auto esBridgePort = createLagBridgePort(port, true);

    sai_mac_t mac = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x0f };

    auto fe = fdbEntry(mac, m_vlan1);

    ASSERT_NO_FATAL_FAILURE(createStaticEntry(fe, esBridgePort, true));

    sai_attribute_t attr;

    attr.id = SAI_BRIDGE_PORT_ATTR_BRIDGE_PORT_SET_SWITCHOVER;
    attr.value.booldata = true;

    ASSERT_EQ(m_sw->set(SAI_OBJECT_TYPE_BRIDGE_PORT, esBridgePort, &attr), SAI_STATUS_SUCCESS);

    sendFrame(port, mac);

    EXPECT_TRUE(fdbEvents().empty());

    attr.value.booldata = false;

    ASSERT_EQ(m_sw->set(SAI_OBJECT_TYPE_BRIDGE_PORT, esBridgePort, &attr), SAI_STATUS_SUCCESS);

    sendFrame(port, mac);

    EXPECT_EQ(fdbEvents(), std::vector<sai_fdb_event_t>{ SAI_FDB_EVENT_LEARNED });
}
