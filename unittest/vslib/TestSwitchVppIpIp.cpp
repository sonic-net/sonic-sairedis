#include "SwitchVppTestHarness.h"

#include "meta/sai_serialize.h"

#include <gtest/gtest.h>

#include <cstdio>
#include <string>
#include <vector>
#include <unistd.h>

using namespace saivs;
using namespace saivs_test;

/*
 * SAI_TUNNEL_ATTR_LOOPBACK_PACKET_ACTION of IP-in-IP tunnels.
 *
 * Each switch of a dual top-of-rack pair has a P2P tunnel to its peer, which
 * orchagent creates with the action DROP and uses for encap only: the P2MP
 * terms of other SAI tunnels, the decap tunnels, match the packets of the
 * peer. The encap next hop of the P2P tunnel creates a VPP P2P tunnel, which
 * VPP takes the encapsulated packets of the peer in on rather than the MP2P
 * tunnel of the terms, so a packet routed back to the peer has that VPP tunnel
 * as both its ingress and its egress interface.
 *
 * sonicExtFeatureEnabled() keeps the iface-loopback state process-wide, so
 * each case runs as a death test, in a process that runs the test anew
 * (gtest_death_test_style), and exits 0 if it passes.
 *
 * The tunnel to the peer also has the pipe TTL model (SAI_TUNNEL_ATTR_ENCAP_TTL_MODE),
 * with the default SAI_TUNNEL_ATTR_ENCAP_TTL_VAL, 255: VPP needs the sonic_ext
 * tunnel-encap-ttl feature to give its VPP tunnel that outer TTL.
 */

#define EXIT_UNLESS(condition) \
    do \
    { \
        if (!(condition)) \
        { \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
            _exit(1); \
        } \
    } while (0)

namespace
{
    const char LOCAL_IP[] = "10.1.0.32";
    const char PEER_IP[] = "10.1.0.33";
    const char OTHER_PEER_IP[] = "10.1.0.34";

    class SwitchVppIpIpTest : public SwitchVppTest
    {
        protected:

            void SetUp() override
            {
                SwitchVppTest::SetUp();

                ::testing::FLAGS_gtest_death_test_style = "threadsafe";

                std::vector<sai_attribute_t> attrs(2);
                attrs[0].id = SAI_ROUTER_INTERFACE_ATTR_TYPE;
                attrs[0].value.s32 = SAI_ROUTER_INTERFACE_TYPE_LOOPBACK;
                attrs[1].id = SAI_ROUTER_INTERFACE_ATTR_VIRTUAL_ROUTER_ID;
                attrs[1].value.oid = m_defaultVr;
                ASSERT_EQ(SAI_STATUS_SUCCESS, create(SAI_OBJECT_TYPE_ROUTER_INTERFACE, m_overlayRif, attrs));
            }

            sai_status_t create(
                    sai_object_type_t type,
                    sai_object_id_t& oid,
                    std::vector<sai_attribute_t> attrs)
            {
                SWSS_LOG_ENTER();

                oid = m_mgr->allocateNewObjectId(type, m_switchId);

                return m_sw->create(type, sai_serialize_object_id(oid), m_switchId,
                        static_cast<uint32_t>(attrs.size()), attrs.data());
            }

            sai_status_t remove(
                    sai_object_type_t type,
                    sai_object_id_t oid)
            {
                SWSS_LOG_ENTER();

                return m_sw->remove(type, sai_serialize_object_id(oid));
            }

            sai_attribute_t loopbackAction(
                    int32_t action)
            {
                SWSS_LOG_ENTER();

                sai_attribute_t attr{};
                attr.id = SAI_TUNNEL_ATTR_LOOPBACK_PACKET_ACTION;
                attr.value.s32 = action;

                return attr;
            }

            /* An IP-in-IP P2P tunnel from this switch, with the extra attributes given */
            sai_status_t createTunnel(
                    sai_object_id_t& tunnel,
                    std::vector<sai_attribute_t> extra = {})
            {
                SWSS_LOG_ENTER();

                std::vector<sai_attribute_t> attrs(4);
                attrs[0].id = SAI_TUNNEL_ATTR_TYPE;
                attrs[0].value.s32 = SAI_TUNNEL_TYPE_IPINIP;
                attrs[1].id = SAI_TUNNEL_ATTR_OVERLAY_INTERFACE;
                attrs[1].value.oid = m_overlayRif;
                attrs[2].id = SAI_TUNNEL_ATTR_PEER_MODE;
                attrs[2].value.s32 = SAI_TUNNEL_PEER_MODE_P2P;
                attrs[3].id = SAI_TUNNEL_ATTR_ENCAP_SRC_IP;
                sai_deserialize_ip_address(LOCAL_IP, attrs[3].value.ipaddr);
                attrs.insert(attrs.end(), extra.begin(), extra.end());

                return create(SAI_OBJECT_TYPE_TUNNEL, tunnel, attrs);
            }

            /* An encap next hop to the peer switch, or to another */
            sai_status_t createNexthop(
                    sai_object_id_t& nexthop,
                    sai_object_id_t tunnel,
                    const char *peer = PEER_IP)
            {
                SWSS_LOG_ENTER();

                std::vector<sai_attribute_t> attrs(3);
                attrs[0].id = SAI_NEXT_HOP_ATTR_TYPE;
                attrs[0].value.s32 = SAI_NEXT_HOP_TYPE_TUNNEL_ENCAP;
                attrs[1].id = SAI_NEXT_HOP_ATTR_IP;
                sai_deserialize_ip_address(peer, attrs[1].value.ipaddr);
                attrs[2].id = SAI_NEXT_HOP_ATTR_TUNNEL_ID;
                attrs[2].value.oid = tunnel;

                return create(SAI_OBJECT_TYPE_NEXT_HOP, nexthop, attrs);
            }

            /* A decap term for the packets of the peer switch: P2P, or P2MP as dual top-of-rack decap tunnels have */
            sai_status_t createTerm(
                    sai_object_id_t& term,
                    sai_object_id_t tunnel,
                    sai_tunnel_term_table_entry_type_t type = SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_P2P)
            {
                SWSS_LOG_ENTER();

                std::vector<sai_attribute_t> attrs(5);
                attrs[0].id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_VR_ID;
                attrs[0].value.oid = m_defaultVr;
                attrs[1].id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TYPE;
                attrs[1].value.s32 = type;
                attrs[2].id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_DST_IP;
                sai_deserialize_ip_address(LOCAL_IP, attrs[2].value.ipaddr);
                attrs[3].id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_TUNNEL_TYPE;
                attrs[3].value.s32 = SAI_TUNNEL_TYPE_IPINIP;
                attrs[4].id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_ACTION_TUNNEL_ID;
                attrs[4].value.oid = tunnel;

                if (type == SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_P2P)
                {
                    sai_attribute_t src{};
                    src.id = SAI_TUNNEL_TERM_TABLE_ENTRY_ATTR_SRC_IP;
                    sai_deserialize_ip_address(PEER_IP, src.value.ipaddr);
                    attrs.push_back(src);
                }

                return create(SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY, term, attrs);
            }

            sai_status_t setLoopbackAction(
                    sai_object_id_t tunnel,
                    int32_t action)
            {
                SWSS_LOG_ENTER();

                auto attr = loopbackAction(action);

                return m_sw->set(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(tunnel), &attr);
            }

            /* The loopback packet action GET returns for a tunnel, or -1 if GET fails */
            int32_t storedLoopbackAction(
                    sai_object_id_t tunnel)
            {
                SWSS_LOG_ENTER();

                sai_attribute_t attr{};
                attr.id = SAI_TUNNEL_ATTR_LOOPBACK_PACKET_ACTION;

                return m_sw->get(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(tunnel), 1, &attr) ==
                    SAI_STATUS_SUCCESS ? attr.value.s32 : -1;
            }

            /* The loopback packet action VPP has for an interface: 1 drop, 0 forward, -1 none set */
            int vppLoopbackAction(
                    const std::string& ifname)
            {
                SWSS_LOG_ENTER();

                auto it = vppMock().actions.find(ifname);

                return it == vppMock().actions.end() ? -1 : it->second;
            }

            sai_attribute_t encapTtlMode(
                    int32_t mode)
            {
                SWSS_LOG_ENTER();

                sai_attribute_t attr{};
                attr.id = SAI_TUNNEL_ATTR_ENCAP_TTL_MODE;
                attr.value.s32 = mode;

                return attr;
            }

            sai_attribute_t encapTtlVal(
                    uint8_t ttl)
            {
                SWSS_LOG_ENTER();

                sai_attribute_t attr{};
                attr.id = SAI_TUNNEL_ATTR_ENCAP_TTL_VAL;
                attr.value.u8 = ttl;

                return attr;
            }

            sai_status_t setEncapTtl(
                    sai_object_id_t tunnel,
                    uint8_t ttl)
            {
                SWSS_LOG_ENTER();

                auto attr = encapTtlVal(ttl);

                return m_sw->set(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(tunnel), &attr);
            }

            /* The encap TTL GET returns for a tunnel, or -1 if GET fails */
            int storedEncapTtl(
                    sai_object_id_t tunnel)
            {
                SWSS_LOG_ENTER();

                auto attr = encapTtlVal(0);

                return m_sw->get(SAI_OBJECT_TYPE_TUNNEL, sai_serialize_object_id(tunnel), 1, &attr) ==
                    SAI_STATUS_SUCCESS ? attr.value.u8 : -1;
            }

            /* The encap TTL VPP has for an interface, 0 if none */
            int vppEncapTtl(
                    const std::string& ifname)
            {
                SWSS_LOG_ENTER();

                auto it = vppMock().encapTtls.find(ifname);

                return it == vppMock().encapTtls.end() ? 0 : it->second;
            }

            sai_object_id_t m_overlayRif = SAI_NULL_OBJECT_ID;
    };
}

TEST_F(SwitchVppIpIpTest, encapNexthopTakesDropOfItsTunnel)
{
    ASSERT_EXIT({
        vppMock().loopbackEnabled = true;

        /* As on a dual top-of-rack pair, a decap tunnel without the action has the P2MP term of the peer's packets */
        sai_object_id_t decapTunnel;
        sai_object_id_t term;
        EXIT_UNLESS(createTunnel(decapTunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createTerm(term, decapTunnel, SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_P2MP) == SAI_STATUS_SUCCESS);

        sai_object_id_t tunnel;
        sai_object_id_t nexthop;
        EXIT_UNLESS(createTunnel(tunnel, {loopbackAction(SAI_PACKET_ACTION_DROP)}) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppMock().loopbackCalls == 0);

        /* The P2P VPP tunnel of the next hop, which VPP takes the peer's packets in on, gets DROP */
        EXIT_UNLESS(createNexthop(nexthop, tunnel) == SAI_STATUS_SUCCESS);
        auto adds = vppCallsTo("vpp_ipip_tunnel_add");
        EXIT_UNLESS(adds.size() == 2);
        EXIT_UNLESS(adds[0].name == "ipip0" && adds[0].sub_id == static_cast<uint32_t>(IpIpTunnelVPPData::TUNNEL_API_MODE_MP2P));
        EXIT_UNLESS(adds[1].name == "ipip1" && adds[1].sub_id == static_cast<uint32_t>(IpIpTunnelVPPData::TUNNEL_API_MODE_P2P));
        EXIT_UNLESS(vppLoopbackAction("ipip1") == 1);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == -1);

        /* VPP resets the action of the tunnel it deletes */
        EXIT_UNLESS(remove(SAI_OBJECT_TYPE_NEXT_HOP, nexthop) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallIndex("vpp_ipip_tunnel_del", "ipip1") >= 0);
        EXIT_UNLESS(vppMock().loopbackCalls == 1);

        /* so the next tunnel, which takes the same sw_if_index, needs DROP again */
        EXIT_UNLESS(createNexthop(nexthop, tunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallsTo("vpp_ipip_tunnel_add").size() == 3);
        EXIT_UNLESS(vppLoopbackAction("ipip1") == 1);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == -1);
        EXIT_UNLESS(vppMock().loopbackCalls == 2);

        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppIpIpTest, vppTunnelOfDecapTermTakesActionOfEncapNexthops)
{
    ASSERT_EXIT({
        vppMock().loopbackEnabled = true;

        sai_object_id_t decapTunnel;
        sai_object_id_t term;
        EXIT_UNLESS(createTunnel(decapTunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createTerm(term, decapTunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallsTo("vpp_ipip_tunnel_add").size() == 1);

        sai_object_id_t tunnel;
        sai_object_id_t nexthop;
        EXIT_UNLESS(createTunnel(tunnel, {loopbackAction(SAI_PACKET_ACTION_DROP)}) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createNexthop(nexthop, tunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallsTo("vpp_ipip_tunnel_add").size() == 1);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 1);

        /* The decap term keeps the VPP tunnel, which no packet is routed back into now */
        EXIT_UNLESS(remove(SAI_OBJECT_TYPE_NEXT_HOP, nexthop) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallsTo("vpp_ipip_tunnel_del").empty());
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 0);

        EXIT_UNLESS(remove(SAI_OBJECT_TYPE_TUNNEL_TERM_TABLE_ENTRY, term) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallIndex("vpp_ipip_tunnel_del", "ipip0") >= 0);
        EXIT_UNLESS(vppMock().loopbackCalls == 2);

        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppIpIpTest, setAppliesToVppTunnelsAndKeepsPreviousActionOnFailure)
{
    ASSERT_EXIT({
        vppMock().loopbackEnabled = true;

        sai_object_id_t tunnel;
        sai_object_id_t nexthop;
        EXIT_UNLESS(createTunnel(tunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(storedLoopbackAction(tunnel) == SAI_PACKET_ACTION_FORWARD);
        EXIT_UNLESS(createNexthop(nexthop, tunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppMock().loopbackCalls == 0);

        vppMock().loopbackResult = -1;
        EXIT_UNLESS(setLoopbackAction(tunnel, SAI_PACKET_ACTION_DROP) == SAI_STATUS_FAILURE);
        EXIT_UNLESS(storedLoopbackAction(tunnel) == SAI_PACKET_ACTION_FORWARD);
        vppMock().loopbackResult = 0;

        EXIT_UNLESS(setLoopbackAction(tunnel, SAI_PACKET_ACTION_DROP) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(storedLoopbackAction(tunnel) == SAI_PACKET_ACTION_DROP);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 1);

        EXIT_UNLESS(setLoopbackAction(tunnel, SAI_PACKET_ACTION_TRAP) == SAI_STATUS_NOT_SUPPORTED);
        EXIT_UNLESS(storedLoopbackAction(tunnel) == SAI_PACKET_ACTION_DROP);

        EXIT_UNLESS(setLoopbackAction(tunnel, SAI_PACKET_ACTION_FORWARD) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(storedLoopbackAction(tunnel) == SAI_PACKET_ACTION_FORWARD);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 0);

        vppMock().loopbackResult = -1;
        EXIT_UNLESS(setLoopbackAction(tunnel, SAI_PACKET_ACTION_DROP) == SAI_STATUS_FAILURE);
        EXIT_UNLESS(storedLoopbackAction(tunnel) == SAI_PACKET_ACTION_FORWARD);
        vppMock().loopbackResult = 0;

        /* VPP still has FORWARD, so DROP is applied again */
        EXIT_UNLESS(setLoopbackAction(tunnel, SAI_PACKET_ACTION_DROP) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 1);

        sai_object_id_t trapTunnel;
        EXIT_UNLESS(createTunnel(trapTunnel, {loopbackAction(SAI_PACKET_ACTION_TRAP)}) == SAI_STATUS_NOT_SUPPORTED);

        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppIpIpTest, failedSetReturnsVppTunnelsToPreviousAction)
{
    ASSERT_EXIT({
        vppMock().loopbackEnabled = true;

        /* Next hops to two peers, so two VPP tunnels */
        sai_object_id_t tunnel;
        sai_object_id_t nexthop;
        sai_object_id_t otherNexthop;
        EXIT_UNLESS(createTunnel(tunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createNexthop(nexthop, tunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createNexthop(otherNexthop, tunnel, OTHER_PEER_IP) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallsTo("vpp_ipip_tunnel_add").size() == 2);

        /* The second VPP tunnel doesn't take DROP, so the first, which did, goes back to FORWARD */
        vppMock().loopbackFailures.emplace("ipip1", 1);
        EXIT_UNLESS(setLoopbackAction(tunnel, SAI_PACKET_ACTION_DROP) == SAI_STATUS_FAILURE);
        EXIT_UNLESS(storedLoopbackAction(tunnel) == SAI_PACKET_ACTION_FORWARD);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 0);
        EXIT_UNLESS(vppLoopbackAction("ipip1") == -1);

        /* If the first doesn't go back either, its next sync returns it to the stored action */
        vppMock().loopbackFailures.emplace("ipip0", 0);
        EXIT_UNLESS(setLoopbackAction(tunnel, SAI_PACKET_ACTION_DROP) == SAI_STATUS_FAILURE);
        EXIT_UNLESS(storedLoopbackAction(tunnel) == SAI_PACKET_ACTION_FORWARD);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 1);

        vppMock().loopbackFailures.clear();
        EXIT_UNLESS(setLoopbackAction(tunnel, SAI_PACKET_ACTION_FORWARD) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 0);
        EXIT_UNLESS(vppLoopbackAction("ipip1") == -1);

        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppIpIpTest, nexthopRemoveIsBestEffortAndNextSyncRetries)
{
    ASSERT_EXIT({
        vppMock().loopbackEnabled = true;

        /* Next hops of a DROP and a FORWARD tunnel to the same peer share a VPP tunnel */
        sai_object_id_t dropTunnel;
        sai_object_id_t forwardTunnel;
        sai_object_id_t dropNexthop;
        sai_object_id_t forwardNexthop;
        EXIT_UNLESS(createTunnel(dropTunnel, {loopbackAction(SAI_PACKET_ACTION_DROP)}) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createTunnel(forwardTunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createNexthop(dropNexthop, dropTunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createNexthop(forwardNexthop, forwardTunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallsTo("vpp_ipip_tunnel_add").size() == 1);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 1);

        /* The DROP tunnel's next hop is removed even if VPP doesn't take FORWARD */
        vppMock().loopbackFailures.emplace("ipip0", 0);
        EXIT_UNLESS(remove(SAI_OBJECT_TYPE_NEXT_HOP, dropNexthop) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallsTo("vpp_ipip_tunnel_del").empty());
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 1);

        /* and the next sync of the VPP tunnel applies FORWARD */
        vppMock().loopbackFailures.clear();
        EXIT_UNLESS(setLoopbackAction(forwardTunnel, SAI_PACKET_ACTION_FORWARD) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 0);

        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppIpIpTest, vppTunnelVppFailsToDeleteKeepsItsAction)
{
    ASSERT_EXIT({
        vppMock().loopbackEnabled = true;

        sai_object_id_t tunnel;
        sai_object_id_t nexthop;
        EXIT_UNLESS(createTunnel(tunnel, {loopbackAction(SAI_PACKET_ACTION_DROP)}) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createNexthop(nexthop, tunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 1);

        /* The next hop and the VPP tunnel, up and with DROP, stay */
        vppMock().ipipDelResult = -1;
        EXIT_UNLESS(remove(SAI_OBJECT_TYPE_NEXT_HOP, nexthop) == SAI_STATUS_FAILURE);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 1);
        EXIT_UNLESS(vppMock().isAdminUp("ipip0"));

        /* so a SET still reaches the VPP tunnel, and a new remove deletes it */
        EXIT_UNLESS(setLoopbackAction(tunnel, SAI_PACKET_ACTION_FORWARD) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppLoopbackAction("ipip0") == 0);

        vppMock().ipipDelResult = 0;
        EXIT_UNLESS(remove(SAI_OBJECT_TYPE_NEXT_HOP, nexthop) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallsTo("vpp_ipip_tunnel_del").size() == 2);
        EXIT_UNLESS(vppMock().swifNames.empty());

        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppIpIpTest, dropIsBestEffortWithoutIfaceLoopback)
{
    ASSERT_EXIT({
        vppMock().loopbackEnabled = false;

        /* Rejected even with no VPP tunnel to apply it to yet, as for a RIF */
        sai_object_id_t tunnel;
        EXIT_UNLESS(createTunnel(tunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(setLoopbackAction(tunnel, SAI_PACKET_ACTION_DROP) == SAI_STATUS_NOT_SUPPORTED);
        EXIT_UNLESS(storedLoopbackAction(tunnel) == SAI_PACKET_ACTION_FORWARD);

        /* orchagent creates the tunnel to the peer with DROP whatever the platform supports: it falls back to FORWARD */
        sai_object_id_t peerTunnel;
        sai_object_id_t nexthop;
        EXIT_UNLESS(createTunnel(peerTunnel, {loopbackAction(SAI_PACKET_ACTION_DROP)}) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(storedLoopbackAction(peerTunnel) == SAI_PACKET_ACTION_FORWARD);
        EXIT_UNLESS(createNexthop(nexthop, peerTunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallsTo("vpp_ipip_tunnel_add").size() == 1);

        EXIT_UNLESS(setLoopbackAction(peerTunnel, SAI_PACKET_ACTION_FORWARD) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(remove(SAI_OBJECT_TYPE_NEXT_HOP, nexthop) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppMock().loopbackCalls == 0);

        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppIpIpTest, loopbackCapabilityFollowsIfaceLoopback)
{
    for (bool enabled : {false, true})
    {
        ASSERT_EXIT({
            vppMock().loopbackEnabled = enabled;

            sai_attr_capability_t capability{};
            EXIT_UNLESS(m_sw->queryAttributeCapability(m_switchId, SAI_OBJECT_TYPE_TUNNEL,
                        SAI_TUNNEL_ATTR_LOOPBACK_PACKET_ACTION, &capability) == SAI_STATUS_SUCCESS);
            EXIT_UNLESS(capability.create_implemented == enabled);
            EXIT_UNLESS(capability.set_implemented == enabled);
            EXIT_UNLESS(capability.get_implemented == enabled);

            _exit(0);
        }, ::testing::ExitedWithCode(0), "");
    }
}

TEST_F(SwitchVppIpIpTest, pipeTunnelNexthopSetsEncapTtlOfItsVppTunnel)
{
    ASSERT_EXIT({
        vppMock().enabledFeatures.insert("tunnel-encap-ttl");

        /* As on a dual top-of-rack pair: a decap tunnel with the P2MP term, and the pipe tunnel to the peer */
        sai_object_id_t decapTunnel;
        sai_object_id_t term;
        EXIT_UNLESS(createTunnel(decapTunnel, {encapTtlMode(SAI_TUNNEL_TTL_MODE_PIPE_MODEL)}) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createTerm(term, decapTunnel, SAI_TUNNEL_TERM_TABLE_ENTRY_TYPE_P2MP) == SAI_STATUS_SUCCESS);

        sai_object_id_t tunnel;
        sai_object_id_t nexthop;
        EXIT_UNLESS(createTunnel(tunnel, {encapTtlMode(SAI_TUNNEL_TTL_MODE_PIPE_MODEL)}) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallsTo("vpp_sonic_ext_tunnel_encap_ttl_set").empty());

        /* The VPP tunnel of the next hop gets the default pipe TTL; the decap term's, which doesn't encap, none */
        EXIT_UNLESS(createNexthop(nexthop, tunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallsTo("vpp_ipip_tunnel_add").size() == 2);
        EXIT_UNLESS(vppEncapTtl("ipip1") == 255);
        EXIT_UNLESS(vppEncapTtl("ipip0") == 0);

        /* VPP resets the TTL of the tunnel it deletes */
        EXIT_UNLESS(remove(SAI_OBJECT_TYPE_NEXT_HOP, nexthop) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallIndex("vpp_ipip_tunnel_del", "ipip1") >= 0);
        EXIT_UNLESS(vppCallsTo("vpp_sonic_ext_tunnel_encap_ttl_set").size() == 1);

        /* so the next tunnel, which takes the same sw_if_index, needs it again */
        EXIT_UNLESS(createNexthop(nexthop, tunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppEncapTtl("ipip1") == 255);
        EXIT_UNLESS(vppCallsTo("vpp_sonic_ext_tunnel_encap_ttl_set").size() == 2);

        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppIpIpTest, encapTtlFollowsTtlModeAndValue)
{
    ASSERT_EXIT({
        vppMock().enabledFeatures.insert("tunnel-encap-ttl");

        /* Pipe model with a TTL */
        sai_object_id_t tunnel;
        sai_object_id_t nexthop;
        EXIT_UNLESS(createTunnel(tunnel, {encapTtlMode(SAI_TUNNEL_TTL_MODE_PIPE_MODEL), encapTtlVal(64)}) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createNexthop(nexthop, tunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppEncapTtl("ipip0") == 64);

        /* The uniform model, set or the SAI default, copies the inner TTL with the VPP tunnel flags instead */
        sai_object_id_t uniformTunnel;
        sai_object_id_t uniformNexthop;
        EXIT_UNLESS(createTunnel(uniformTunnel, {encapTtlMode(SAI_TUNNEL_TTL_MODE_UNIFORM_MODEL)}) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createNexthop(uniformNexthop, uniformTunnel, OTHER_PEER_IP) == SAI_STATUS_SUCCESS);

        sai_object_id_t defaultTunnel;
        sai_object_id_t defaultNexthop;
        EXIT_UNLESS(createTunnel(defaultTunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createNexthop(defaultNexthop, defaultTunnel, "10.1.0.35") == SAI_STATUS_SUCCESS);

        EXIT_UNLESS(vppCallsTo("vpp_ipip_tunnel_add").size() == 3);
        EXIT_UNLESS(vppCallsTo("vpp_sonic_ext_tunnel_encap_ttl_set").size() == 1);
        EXIT_UNLESS(vppMock().encapTtls.size() == 1);

        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppIpIpTest, sharedVppTunnelTakesHighestEncapTtlOfItsNexthops)
{
    ASSERT_EXIT({
        vppMock().enabledFeatures.insert("tunnel-encap-ttl");

        /* A P2P decap term creates the VPP tunnel to the peer, which next hops of two pipe tunnels then use */
        sai_object_id_t decapTunnel;
        sai_object_id_t term;
        EXIT_UNLESS(createTunnel(decapTunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createTerm(term, decapTunnel) == SAI_STATUS_SUCCESS);

        sai_object_id_t lowTunnel;
        sai_object_id_t highTunnel;
        sai_object_id_t lowNexthop;
        sai_object_id_t highNexthop;
        EXIT_UNLESS(createTunnel(lowTunnel, {encapTtlMode(SAI_TUNNEL_TTL_MODE_PIPE_MODEL), encapTtlVal(64)}) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createTunnel(highTunnel, {encapTtlMode(SAI_TUNNEL_TTL_MODE_PIPE_MODEL), encapTtlVal(128)}) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createNexthop(lowNexthop, lowTunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppEncapTtl("ipip0") == 64);
        EXIT_UNLESS(createNexthop(highNexthop, highTunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallsTo("vpp_ipip_tunnel_add").size() == 1);
        EXIT_UNLESS(vppEncapTtl("ipip0") == 128);

        EXIT_UNLESS(remove(SAI_OBJECT_TYPE_NEXT_HOP, highNexthop) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppEncapTtl("ipip0") == 64);

        /* The decap term keeps the VPP tunnel, which encapsulates nothing now: it loses the TTL */
        EXIT_UNLESS(remove(SAI_OBJECT_TYPE_NEXT_HOP, lowNexthop) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppCallsTo("vpp_ipip_tunnel_del").empty());
        EXIT_UNLESS(vppMock().callsTo("vpp_sonic_ext_tunnel_encap_ttl_set", "ipip0").back().sub_id == 0);
        EXIT_UNLESS(vppEncapTtl("ipip0") == 0);

        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppIpIpTest, setEncapTtlAppliesAndKeepsPreviousOnFailure)
{
    ASSERT_EXIT({
        vppMock().enabledFeatures.insert("tunnel-encap-ttl");

        /* The VPP tunnel doesn't take the TTL, but the next hop is created: it's best effort */
        vppMock().encapTtlFailures.emplace("ipip0", 255);
        sai_object_id_t tunnel;
        sai_object_id_t nexthop;
        EXIT_UNLESS(createTunnel(tunnel, {encapTtlMode(SAI_TUNNEL_TTL_MODE_PIPE_MODEL)}) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(createNexthop(nexthop, tunnel) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppEncapTtl("ipip0") == 0);

        /* and the next sync of the VPP tunnel applies it */
        vppMock().encapTtlFailures.clear();
        EXIT_UNLESS(setEncapTtl(tunnel, 255) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(vppEncapTtl("ipip0") == 255);

        EXIT_UNLESS(setEncapTtl(tunnel, 128) == SAI_STATUS_SUCCESS);
        EXIT_UNLESS(storedEncapTtl(tunnel) == 128);
        EXIT_UNLESS(vppEncapTtl("ipip0") == 128);

        /* A TTL VPP fails to take is not stored */
        vppMock().encapTtlFailures.emplace("ipip0", 200);
        EXIT_UNLESS(setEncapTtl(tunnel, 200) == SAI_STATUS_FAILURE);
        EXIT_UNLESS(storedEncapTtl(tunnel) == 128);
        EXIT_UNLESS(vppEncapTtl("ipip0") == 128);

        _exit(0);
    }, ::testing::ExitedWithCode(0), "");
}

TEST_F(SwitchVppIpIpTest, encapTtlNeedsTunnelEncapTtlFeature)
{
    for (bool enabled : {false, true})
    {
        ASSERT_EXIT({
            if (enabled)
            {
                vppMock().enabledFeatures.insert("tunnel-encap-ttl");
            }

            sai_attr_capability_t capability{};
            EXIT_UNLESS(m_sw->queryAttributeCapability(m_switchId, SAI_OBJECT_TYPE_TUNNEL,
                        SAI_TUNNEL_ATTR_ENCAP_TTL_VAL, &capability) == SAI_STATUS_SUCCESS);
            EXIT_UNLESS(capability.create_implemented == enabled);
            EXIT_UNLESS(capability.set_implemented == enabled);
            EXIT_UNLESS(capability.get_implemented == enabled);

            /* orchagent creates the tunnel to the peer with the pipe model whatever the platform supports */
            sai_object_id_t tunnel;
            sai_object_id_t nexthop;
            EXIT_UNLESS(createTunnel(tunnel, {encapTtlMode(SAI_TUNNEL_TTL_MODE_PIPE_MODEL)}) == SAI_STATUS_SUCCESS);
            EXIT_UNLESS(createNexthop(nexthop, tunnel) == SAI_STATUS_SUCCESS);
            EXIT_UNLESS(vppEncapTtl("ipip0") == (enabled ? 255 : 0));
            EXIT_UNLESS(vppCallsTo("vpp_sonic_ext_tunnel_encap_ttl_set").size() == (enabled ? 1u : 0u));

            EXIT_UNLESS(setEncapTtl(tunnel, 64) == (enabled ? SAI_STATUS_SUCCESS : SAI_STATUS_NOT_SUPPORTED));
            EXIT_UNLESS(storedEncapTtl(tunnel) == (enabled ? 64 : -1));

            _exit(0);
        }, ::testing::ExitedWithCode(0), "");
    }
}
