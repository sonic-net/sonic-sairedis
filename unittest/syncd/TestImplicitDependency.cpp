/*
 * SPDX-FileCopyrightText: Nexthop Systems Inc.
 * Copyright (c) 2026 Nexthop Systems Inc. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

#include "AsicView.h"
#include "BreakConfig.h"
#include "ComparisonLogic.h"
#include "ImplicitDependency.h"
#include "MockableSaiInterface.h"
#include "MockableSaiSwitchInterface.h"
#include "NotificationHandler.h"
#include "NotificationProcessor.h"
#include "RedisClient.h"
#include "SaiObj.h"
#include "ServiceMethodTable.h"
#include "SingleReiniter.h"
#include "VendorSaiOptions.h"
#include "VirtualOidTranslator.h"

#include "lib/RedisVidIndexGenerator.h"
#include "lib/sairediscommon.h"
#include "meta/sai_serialize.h"
#include "meta/SaiAttributeList.h"
#include "vslib/Sai.h"

#include "swss/dbconnector.h"
#include "swss/logger.h"
#include "swss/redisreply.h"
#include "swss/table.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <set>

using namespace syncd;
using namespace unittests;
using namespace std::placeholders;

namespace
{
    constexpr sai_object_id_t SWITCH_VID = 0x21000000000000;
    constexpr sai_object_id_t SWITCH_RID = 0x21000000000000;

    const std::string P2MP = "oid:0x2a000000000001";
    const std::string P2P = "oid:0x2a000000000002";
    const std::string TERM = "oid:0x2b000000000001";
    const std::string BRIDGE_PORT = "oid:0x3a000000000001";

    typedef std::map<std::string, std::string> Attrs;

    Attrs vxlanTunnel(
            _In_ const char* peerMode,
            _In_ const char* srcIp,
            _In_ const char* dstIp = nullptr)
    {
        SWSS_LOG_ENTER();

        Attrs attrs = {
            {"SAI_TUNNEL_ATTR_TYPE", "SAI_TUNNEL_TYPE_VXLAN"},
            {"SAI_TUNNEL_ATTR_ENCAP_SRC_IP", srcIp},
        };

        if (peerMode)
        {
            attrs["SAI_TUNNEL_ATTR_PEER_MODE"] = peerMode;
        }

        if (dstIp)
        {
            attrs["SAI_TUNNEL_ATTR_ENCAP_DST_IP"] = dstIp;
        }

        return attrs;
    }

    Attrs p2mpTunnel(
            _In_ const char* srcIp)
    {
        SWSS_LOG_ENTER();

        return vxlanTunnel("SAI_TUNNEL_PEER_MODE_P2MP", srcIp);
    }

    Attrs p2pTunnel(
            _In_ const char* srcIp,
            _In_ const char* dstIp)
    {
        SWSS_LOG_ENTER();

        return vxlanTunnel("SAI_TUNNEL_PEER_MODE_P2P", srcIp, dstIp);
    }

    /*
     * P2MP VXLAN tunnel termination entry of the local endpoint dstIp, for
     * the tunnel with VID tunnelVid.
     */
    Attrs p2mpTerm(
            _In_ const char* dstIp,
            _In_ const std::string& tunnelVid)
    {
        SWSS_LOG_ENTER();

        return {
            {"SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE", "SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_P2MP"},
            {"SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE", "SAI_TUNNEL_TYPE_VXLAN"},
            {"SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP", dstIp},
            {"SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID", tunnelVid},
        };
    }

    /*
     * Keys of an object given as ASIC_DB strings: {provider key, dependant key}.
     */
    std::pair<std::string, std::string> keysOf(
            _In_ sai_object_type_t objectType,
            _In_ const Attrs& attrs)
    {
        SWSS_LOG_ENTER();

        std::vector<swss::FieldValueTuple> values(attrs.begin(), attrs.end());

        saimeta::SaiAttributeList list(objectType, values, false);

        return {
            ImplicitDependency::getProviderKey(objectType, list.get_attr_count(), list.get_attr_list()),
            ImplicitDependency::getDependantKey(objectType, list.get_attr_count(), list.get_attr_list())
        };
    }

    std::string tunnelKey(
            _In_ const std::string& vid)
    {
        SWSS_LOG_ENTER();

        return "SAI_OBJECT_TYPE_TUNNEL:" + vid;
    }

    std::string termKey(
            _In_ const std::string& vid)
    {
        SWSS_LOG_ENTER();

        return "SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY:" + vid;
    }

    class TestSwitch: public MockableSaiSwitchInterface
    {
        public:

            TestSwitch(): MockableSaiSwitchInterface(SWITCH_VID, SWITCH_RID) { }

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getVidToRidMap() const override { return {}; }

            virtual std::unordered_map<sai_object_id_t, sai_object_id_t> getRidToVidMap() const override { return {}; }

            virtual bool isDiscoveredRid(_In_ sai_object_id_t) const override { return false; }

            virtual bool isNonRemovableRid(_In_ sai_object_id_t) const override { return false; }

            virtual sai_object_id_t getSwitchDefaultAttrOid(_In_ sai_attr_id_t) const override { return SAI_NULL_OBJECT_ID; }

            virtual std::set<sai_object_id_t> getDiscoveredRids() const override { return {}; }

            virtual std::set<sai_object_id_t> getColdBootDiscoveredVids() const override { return {}; }

            virtual std::set<sai_object_id_t> getWarmBootDiscoveredVids() const override { return {}; }
    };

    struct Views
    {
        std::shared_ptr<AsicView> current = std::make_shared<AsicView>();
        std::shared_ptr<AsicView> temp = std::make_shared<AsicView>();
        std::shared_ptr<ComparisonLogic> cl;

        Views(
                _In_ const swss::TableDump &currentDump,
                _In_ const swss::TableDump &tempDump)
        {
            SWSS_LOG_ENTER();

            current->fromDump(currentDump);
            temp->fromDump(tempDump);

            /*
             * The comparison logic takes the current view's RID maps from the
             * switch, so they are set after it.
             */
            cl = std::make_shared<ComparisonLogic>(
                    std::make_shared<MockableSaiInterface>(),
                    std::make_shared<TestSwitch>(),
                    nullptr, std::set<sai_object_id_t>{}, current, temp, std::make_shared<BreakConfig>());

            /*
             * Objects of the current view exist on the ASIC; the switch is
             * the same object in both views, and it stands in as the default
             * trap group (which is not under test).
             */
            sai_object_id_t rid = 0x100;

            for (auto& kv: current->m_soAll)
            {
                sai_object_id_t vid = kv.second->getVid();
                sai_object_id_t r = (vid == SWITCH_VID) ? SWITCH_RID : rid++;

                current->m_vidToRid[vid] = r;
                current->m_ridToVid[r] = vid;
            }

            temp->m_vidToRid[SWITCH_VID] = SWITCH_RID;
            temp->m_ridToVid[SWITCH_RID] = SWITCH_VID;

            current->m_defaultTrapGroupRid = SWITCH_RID;

        }
    };

    swss::TableDump switchOnly()
    {
        SWSS_LOG_ENTER();

        return {
            {"SAI_OBJECT_TYPE_SWITCH:oid:0x21000000000000", {
                {"SAI_SWITCH_ATTR_INIT_SWITCH", "true"},
            }},
        };
    }

    /*
     * "op key" of each generated ASIC operation, in order.
     */
    std::vector<std::string> opsOf(
            _In_ const std::vector<AsicOperation>& ops)
    {
        SWSS_LOG_ENTER();

        std::vector<std::string> out;

        for (const auto& op: ops)
        {
            out.push_back(kfvOp(*op.m_op) + " " + kfvKey(*op.m_op));
        }

        return out;
    }

    size_t indexOf(
            _In_ const std::vector<std::string>& v,
            _In_ const std::string& s)
    {
        SWSS_LOG_ENTER();

        return std::find(v.begin(), v.end(), s) - v.begin();
    }
}

TEST(ImplicitDependency, p2p_tunnel_needs_p2mp_tunnel_of_its_source_ip)
{
    auto p2mp = keysOf(SAI_OBJECT_TYPE_TUNNEL, p2mpTunnel("10.1.0.32"));
    auto p2p = keysOf(SAI_OBJECT_TYPE_TUNNEL, p2pTunnel("10.1.0.32", "10.255.91.1"));

    EXPECT_NE(p2mp.first, "");
    EXPECT_EQ(p2mp.second, "");

    EXPECT_EQ(p2p.first, "");
    EXPECT_EQ(p2p.second, p2mp.first);
}

TEST(ImplicitDependency, p2p_tunnel_needs_p2mp_termination_of_its_source_ip)
{
    auto term = keysOf(SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY, p2mpTerm("10.1.0.32", P2MP));
    auto p2p = keysOf(SAI_OBJECT_TYPE_TUNNEL, p2pTunnel("10.1.0.32", "10.255.91.1"));

    EXPECT_EQ(term.first, p2p.second);
    EXPECT_EQ(term.second, "");

    auto p2pTerm = p2mpTerm("10.1.0.32", P2MP);
    p2pTerm["SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE"] = "SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_P2P";

    EXPECT_EQ(keysOf(SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY, p2pTerm), std::make_pair(std::string(), std::string()));

    auto ipinipTerm = p2mpTerm("10.1.0.32", P2MP);
    ipinipTerm["SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE"] = "SAI_TUNNEL_TYPE_IPINIP";

    EXPECT_EQ(keysOf(SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY, ipinipTerm), std::make_pair(std::string(), std::string()));

    EXPECT_NE(keysOf(SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY, p2mpTerm("10.1.0.33", P2MP)).first, p2p.second);
}

TEST(ImplicitDependency, tunnel_peer_mode_defaults_to_p2mp)
{
    auto p2mp = keysOf(SAI_OBJECT_TYPE_TUNNEL, vxlanTunnel(nullptr, "10.1.0.32"));
    auto p2p = keysOf(SAI_OBJECT_TYPE_TUNNEL, p2pTunnel("10.1.0.32", "10.255.91.1"));

    EXPECT_EQ(p2mp.first, p2p.second);
    EXPECT_EQ(p2mp.second, "");
}

TEST(ImplicitDependency, other_source_ip_is_another_endpoint_and_only_vxlan_has_one)
{
    auto p2p = keysOf(SAI_OBJECT_TYPE_TUNNEL, p2pTunnel("10.1.0.32", "10.255.91.1"));

    EXPECT_NE(keysOf(SAI_OBJECT_TYPE_TUNNEL, p2mpTunnel("10.1.0.33")).first, p2p.second);

    auto ipinip = p2mpTunnel("10.1.0.32");
    ipinip["SAI_TUNNEL_ATTR_TYPE"] = "SAI_TUNNEL_TYPE_IPINIP";

    EXPECT_EQ(keysOf(SAI_OBJECT_TYPE_TUNNEL, ipinip), std::make_pair(std::string(), std::string()));
}

TEST(ImplicitDependency, no_source_ip_or_other_object_has_no_key)
{
    Attrs noSrcIp = {
        {"SAI_TUNNEL_ATTR_TYPE", "SAI_TUNNEL_TYPE_IPINIP"},
    };

    EXPECT_EQ(keysOf(SAI_OBJECT_TYPE_TUNNEL, noSrcIp), std::make_pair(std::string(), std::string()));

    Attrs vlan = {
        {"SAI_VLAN_ATTR_VLAN_ID", "100"},
    };

    EXPECT_EQ(keysOf(SAI_OBJECT_TYPE_VLAN, vlan), std::make_pair(std::string(), std::string()));
}

/*
 * Apply view (warm boot) reaching a new P2P tunnel first creates the new P2MP
 * tunnel and P2MP termination entry of its source IP before it.
 */
TEST(ImplicitDependency, apply_view_creates_p2mp_tunnel_before_p2p_tunnel)
{
    auto tempDump = switchOnly();

    tempDump[tunnelKey(P2MP)] = p2mpTunnel("10.1.0.32");
    tempDump[termKey(TERM)] = p2mpTerm("10.1.0.32", P2MP);
    tempDump[tunnelKey(P2P)] = p2pTunnel("10.1.0.32", "10.255.91.1");

    Views v(switchOnly(), tempDump);

    v.cl->processObjectForViewTransition(*v.current, *v.temp, v.temp->m_soAll.at(P2P));

    auto ops = opsOf(v.current->asicGetOperations());

    ASSERT_EQ(ops.size(), 3u);
    EXPECT_EQ(ops[0], "create SAI_OBJECT_TYPE_TUNNEL:" + P2MP);
    EXPECT_EQ(ops[1], "create SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY:" + TERM);
    EXPECT_EQ(ops[2], "create SAI_OBJECT_TYPE_TUNNEL:" + P2P);
}

/*
 * Apply view removing the endpoint removes the P2P tunnel first, also after
 * the remove operations are optimized: the P2P tunnel's remove waits for its
 * bridge port's, and the P2MP termination entry's and tunnel's must stay
 * after it.
 */
TEST(ImplicitDependency, apply_view_removes_p2p_tunnel_before_p2mp_tunnel)
{
    auto currentDump = switchOnly();

    currentDump[tunnelKey(P2MP)] = p2mpTunnel("10.1.0.32");
    currentDump[termKey(TERM)] = p2mpTerm("10.1.0.32", P2MP);
    currentDump[tunnelKey(P2P)] = p2pTunnel("10.1.0.32", "10.255.91.1");
    currentDump["SAI_OBJECT_TYPE_BRIDGE_PORT:" + BRIDGE_PORT] = {
        {"SAI_BRIDGE_PORT_ATTR_TYPE", "SAI_BRIDGE_PORT_TYPE_TUNNEL"},
        {"SAI_BRIDGE_PORT_ATTR_TUNNEL_ID", P2P},
    };

    Views v(currentDump, switchOnly());

    v.cl->matchOids(*v.current, *v.temp);
    v.cl->applyViewTransition(*v.current, *v.temp);

    const std::string removeP2mp = "remove SAI_OBJECT_TYPE_TUNNEL:" + P2MP;
    const std::string removeTerm = "remove SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY:" + TERM;
    const std::string removeP2p = "remove SAI_OBJECT_TYPE_TUNNEL:" + P2P;
    const std::string removeBp = "remove SAI_OBJECT_TYPE_BRIDGE_PORT:" + BRIDGE_PORT;

    for (auto ops: {opsOf(v.current->asicGetOperations()), opsOf(v.current->asicGetWithOptimizedRemoveOperations())})
    {
        ASSERT_EQ(ops.size(), 4u);
        EXPECT_LT(indexOf(ops, removeBp), indexOf(ops, removeP2p));
        EXPECT_LT(indexOf(ops, removeP2p), indexOf(ops, removeTerm));
        EXPECT_LT(indexOf(ops, removeP2p), indexOf(ops, removeP2mp));
        EXPECT_LT(indexOf(ops, removeTerm), indexOf(ops, removeP2mp));
        EXPECT_LT(indexOf(ops, removeP2mp), ops.size());
    }
}

namespace
{
    static std::map<std::string, std::string> g_profile = {
        {"SAI_VS_SWITCH_TYPE", "SAI_VS_SWITCH_TYPE_BCM56850"},
    };

    static std::map<std::string, std::string>::iterator g_profileIter;

    const char* profileGetValue(
            _In_ sai_switch_profile_id_t profile_id,
            _In_ const char* variable)
    {
        SWSS_LOG_ENTER();

        auto it = variable ? g_profile.find(variable) : g_profile.end();

        return it == g_profile.end() ? nullptr : it->second.c_str();
    }

    int profileGetNextValue(
            _In_ sai_switch_profile_id_t profile_id,
            _Out_ const char** variable,
            _Out_ const char** value)
    {
        SWSS_LOG_ENTER();

        if (value == nullptr)
        {
            g_profileIter = g_profile.begin();
            return 0;
        }

        if (variable == nullptr || g_profileIter == g_profile.end())
        {
            return -1;
        }

        *variable = g_profileIter->first.c_str();
        *value = g_profileIter->second.c_str();

        g_profileIter++;

        return 0;
    }

    /*
     * The virtual switch, failing a P2P VXLAN tunnel create while no P2MP
     * termination entry has set up its local endpoint, as some vendor SAI implementations do.
     */
    class TunnelOrderSai: public saivs::Sai
    {
        public:

            virtual sai_status_t create(
                    _In_ sai_object_type_t objectType,
                    _Out_ sai_object_id_t* objectId,
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override
            {
                SWSS_LOG_ENTER();

                if (objectType == SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY)
                {
                    std::string dstIp;

                    for (uint32_t i = 0; i < attr_count; i++)
                    {
                        if (attr_list[i].id == SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP)
                        {
                            dstIp = sai_serialize_ip_address(attr_list[i].value.ipaddr);
                        }
                    }

                    m_endpoints.insert(dstIp);

                    return made(objectType, objectId, "TERM " + dstIp);
                }

                if (objectType != SAI_OBJECT_TYPE_TUNNEL)
                {
                    return saivs::Sai::create(objectType, objectId, switchId, attr_count, attr_list);
                }

                int32_t peerMode = SAI_TUNNEL_PEER_MODE_P2MP;
                std::string srcIp;
                std::string dstIp;

                for (uint32_t i = 0; i < attr_count; i++)
                {
                    switch (attr_list[i].id)
                    {
                        case SAI_TUNNEL_ATTR_PEER_MODE:
                            peerMode = attr_list[i].value.s32;
                            break;
                        case SAI_TUNNEL_ATTR_ENCAP_SRC_IP:
                            srcIp = sai_serialize_ip_address(attr_list[i].value.ipaddr);
                            break;
                        case SAI_TUNNEL_ATTR_ENCAP_DST_IP:
                            dstIp = sai_serialize_ip_address(attr_list[i].value.ipaddr);
                            break;
                        default:
                            break;
                    }
                }

                if (peerMode == SAI_TUNNEL_PEER_MODE_P2P)
                {
                    if (m_endpoints.count(srcIp) == 0)
                    {
                        return SAI_STATUS_ITEM_NOT_FOUND;
                    }

                    return made(objectType, objectId, dstIp);
                }

                return made(objectType, objectId, "P2MP " + srcIp);
            }

            virtual sai_status_t remove(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId) override
            {
                SWSS_LOG_ENTER();

                sai_status_t status = saivs::Sai::remove(objectType, objectId);

                if (status == SAI_STATUS_SUCCESS && objectType == SAI_OBJECT_TYPE_BRIDGE_PORT)
                {
                    m_removedBridgePorts.push_back(objectId);
                }

                return status;
            }

        private:

            /*
             * The objects carry only the attributes under test, which the
             * virtual switch would reject, so they get a made up RID.
             */
            sai_status_t made(
                    _In_ sai_object_type_t objectType,
                    _Out_ sai_object_id_t* objectId,
                    _In_ const std::string& name)
            {
                SWSS_LOG_ENTER();

                *objectId = ((sai_object_id_t)objectType << 48) | (0xf000 + m_created.size());

                m_created.push_back(name);

                return SAI_STATUS_SUCCESS;
            }

        public:

            /*
             * Local endpoints set up by P2MP termination entries, which a P2P
             * tunnel create looks up.
             */
            std::set<std::string> m_endpoints;

            std::vector<std::string> m_created;

            std::vector<sai_object_id_t> m_removedBridgePorts;
    };

    /*
     * ASIC_DB holding the given objects next to the switch, as a stopped syncd
     * leaves it, and a SingleReiniter to replay it on a new virtual switch.
     */
    struct Replay
    {
        std::shared_ptr<TunnelOrderSai> sai = std::make_shared<TunnelOrderSai>();
        std::shared_ptr<RedisClient> client;
        std::shared_ptr<VirtualOidTranslator> translator;
        std::shared_ptr<SingleReiniter> sr;

        Replay(
                _In_ const std::map<std::string, Attrs>& objects)
        {
            SWSS_LOG_ENTER();

            auto db = std::make_shared<swss::DBConnector>("ASIC_DB", 0);

            swss::RedisReply r(db.get(), "FLUSHALL", REDIS_REPLY_STATUS);
            r.checkStatusOK();

            ServiceMethodTable smt;

            smt.profileGetValue = std::bind(&profileGetValue, _1, _2);
            smt.profileGetNextValue = std::bind(&profileGetNextValue, _1, _2, _3);

            sai_service_method_table_t services = smt.getServiceMethodTable();

            EXPECT_EQ(SAI_STATUS_SUCCESS, sai->apiInitialize(0, &services));

            sai->setOptions(VendorSaiOptions::OPTIONS_KEY, std::make_shared<VendorSaiOptions>());

            client = std::make_shared<RedisClient>(db);

            translator = std::make_shared<VirtualOidTranslator>(
                    client,
                    std::make_shared<sairedis::VirtualObjectIdManager>(
                        0,
                        std::make_shared<sairedis::SwitchConfigContainer>(),
                        std::make_shared<sairedis::RedisVidIndexGenerator>(db, REDIS_KEY_VIDCOUNTER)),
                    sai);

            auto handler = std::make_shared<NotificationHandler>(
                    std::make_shared<NotificationProcessor>(nullptr, nullptr, nullptr));

            swss::Table asicState(db.get(), ASIC_STATE_TABLE);

            asicState.set("SAI_OBJECT_TYPE_SWITCH:oid:0x21000000000000", {{"SAI_SWITCH_ATTR_INIT_SWITCH", "true"}});

            std::vector<std::string> keys = {std::string(ASIC_STATE_TABLE) + ":SAI_OBJECT_TYPE_SWITCH:oid:0x21000000000000"};

            /*
             * The switch RID of the stopped syncd, which the new virtual
             * switch gets again.
             */
            const sai_object_id_t switchRid = 0x2100000000;

            client->insertVidAndRid(SWITCH_VID, switchRid);

            SingleReiniter::ObjectIdMap v2r = {{SWITCH_VID, switchRid}};

            sai_object_id_t oldRid = 0x2a0000000000f0;

            for (const auto& o: objects)
            {
                asicState.set(o.first, std::vector<swss::FieldValueTuple>(o.second.begin(), o.second.end()));

                keys.push_back(std::string(ASIC_STATE_TABLE) + ":" + o.first);

                sai_object_id_t vid;
                sai_deserialize_object_id(o.first.substr(o.first.find(':') + 1), vid);

                v2r[vid] = oldRid++;
            }

            SingleReiniter::ObjectIdMap r2v;

            for (const auto& kv: v2r)
            {
                r2v[kv.second] = kv.first;
            }

            sr = std::make_shared<SingleReiniter>(client, translator, sai, handler, v2r, r2v, keys);
        }

        ~Replay()
        {
            SWSS_LOG_ENTER();

            sai->apiUninitialize();
        }
    };
}

/*
 * Hard reinit reaching a P2P tunnel first (here directly) creates the P2MP
 * tunnel and P2MP termination entry of its source IP before it.
 */
TEST(ImplicitDependency, hard_reinit_creates_p2mp_tunnel_before_p2p_tunnel)
{
    Replay replay({
        {tunnelKey(P2MP), p2mpTunnel("10.1.0.32")},
        {termKey(TERM), p2mpTerm("10.1.0.32", P2MP)},
        {tunnelKey(P2P), p2pTunnel("10.1.0.32", "10.255.91.1")},
    });

    replay.sr->prepareAsicState();
    replay.sr->processSwitches();

    sai_object_id_t p2p;
    sai_deserialize_object_id(P2P, p2p);

    EXPECT_NO_THROW(replay.sr->processSingleVid(p2p));

    EXPECT_EQ(replay.sai->m_created, (std::vector<std::string>{"P2MP 10.1.0.32", "TERM 10.1.0.32", "10.255.91.1"}));
}

/*
 * A full hard reinit of P2P tunnels on two source IP addresses replays every
 * object, each P2P tunnel after the P2MP termination entry of its source IP.
 */
TEST(ImplicitDependency, hard_reinit_replays_tunnels_of_two_source_ips)
{
    std::map<std::string, Attrs> objects = {
        {tunnelKey("oid:0x2a000000000011"), p2pTunnel("10.1.0.32", "10.255.91.1")},
        {tunnelKey("oid:0x2a000000000012"), p2pTunnel("10.1.0.33", "10.255.91.2")},
        {tunnelKey("oid:0x2a000000000013"), p2pTunnel("10.1.0.32", "10.255.91.3")},
        {termKey("oid:0x2b000000000011"), p2mpTerm("10.1.0.33", "oid:0x2a000000000015")},
        {tunnelKey("oid:0x2a000000000014"), p2pTunnel("10.1.0.33", "10.255.91.4")},
        {tunnelKey("oid:0x2a000000000015"), p2mpTunnel("10.1.0.33")},
        {tunnelKey("oid:0x2a000000000016"), p2pTunnel("10.1.0.32", "10.255.91.5")},
        {tunnelKey("oid:0x2a000000000017"), vxlanTunnel(nullptr, "10.1.0.32")},
        {termKey("oid:0x2b000000000012"), p2mpTerm("10.1.0.32", "oid:0x2a000000000017")},
        {tunnelKey("oid:0x2a000000000018"), p2pTunnel("10.1.0.33", "10.255.91.6")},
    };

    Replay replay(objects);

    EXPECT_NO_THROW(replay.sr->hardReinit());

    EXPECT_EQ(replay.sai->m_created.size(), objects.size());
    EXPECT_EQ(replay.sr->getTranslatedVid2Rid().size(), objects.size() + 1);
}

/*
 * Hard reinit with a populated ASIC_DB gives the switch's default objects no
 * VIDs, and removes the ones the view does not have, like the default bridge
 * ports. A notification still carrying such a RID, like an FDB event learned
 * on a default bridge port before the remove, must find it removed rather
 * than unknown.
 */
TEST(SingleReiniter, hard_reinit_records_removed_default_objects)
{
    std::map<std::string, Attrs> objects;

    // more objects than the switch saves its default objects to the view with
    for (int i = 0; i < 40; i++)
    {
        objects["SAI_OBJECT_TYPE_VLAN:" + sai_serialize_object_id(0x26000000000100 + i)] = {
            {"SAI_VLAN_ATTR_VLAN_ID", std::to_string(100 + i)},
        };
    }

    Replay replay(objects);

    EXPECT_NO_THROW(replay.sr->hardReinit());

    replay.client->setVidAndRidMap(replay.sr->getTranslatedVid2Rid());

    EXPECT_NO_THROW(replay.sr->postRemoveActions());

    ASSERT_FALSE(replay.sai->m_removedBridgePorts.empty());

    for (auto rid: replay.sai->m_removedBridgePorts)
    {
        EXPECT_TRUE(replay.translator->checkRidExists(rid, true)) << sai_serialize_object_id(rid);
        EXPECT_FALSE(replay.translator->checkRidExists(rid, false)) << sai_serialize_object_id(rid);
    }
}
