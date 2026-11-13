#pragma once

#include "vpp/SwitchVpp.h"
#include "vpp/vppxlate/SaiIntfStats.h"
#include "vpp/vppxlate/SaiAclStats.h"

#include <array>
#include <chrono>
#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <vector>

struct VppCall
{
    std::string api;
    std::string name;
    uint32_t id;
    uint32_t sub_id;
    bool flag;
};

struct VppRouteCall
{
    bool isAdd;
    bool isMultipath;
    uint32_t nextHopCount;
    std::string nextHop;
};

/**
 * @brief One vpp_pbh_profile_add_del() call, carrying the field vector exactly
 * as the SAI translation produced it.
 */
struct VppPbhProfileCall
{
    bool is_add;
    uint32_t profile_index;
    std::vector<vpp_pbh_hash_field_t> fields;
};

/**
 * @brief One vpp_pbh_table_add_replace() call. A name is only passed when a
 * table is being created, so named tells a create from a replace.
 */
struct VppPbhTableCall
{
    bool named;
    std::string name;
    uint32_t table_index;
    std::vector<vpp_pbh_rule_t> rules;
};

/**
 * @brief One vpp_pbh_interface_attach_detach() call.
 */
struct VppPbhAttachCall
{
    std::string hwif_name;
    uint32_t table_index;
    bool attach;
};

/**
 * @brief One vpp_rule_stats_query() call.
 */
struct VppRuleStatsCall
{
    vpp_rule_stats_type_t type;
    uint32_t table_index;
    uint32_t rule_index;
};

/**
 * @brief One vpp_classify_session_add() call: the node a frame the session
 * matches goes to, and its match, which tells what it is for.
 */
struct VppClassifySession
{
    uint32_t table_index;
    uint32_t hit_next_index;
    std::vector<uint8_t> match;
};

struct VppMock
{
    bool loopbackEnabled = false;
    std::string queriedFeature;
    unsigned int loopbackCalls = 0;
    int loopbackAction = -1;
    int loopbackResult = 0;
    // The interfaces VPP fails to set a loopback packet action on, with that action
    std::set<std::pair<std::string, int>> loopbackFailures;
    int featureResult = 0;
    unsigned int featureCalls = 0;
    int statsResult = 0;
    int lcpResult = 0;
    unsigned int statsCalls = 0;
    unsigned int interfaceCreates = 0;
    unsigned int interfaceLookups = 0;
    uint32_t kernelVrfId = 0;
    int kernelVrfResult = 0;
    std::string kernelLinkNames;
    int kernelLinkResult = 0;
    std::vector<std::string> kernelCommands;
    std::set<std::string> kernelInterfaces;
    int getVrfResult = 0;
    int setVrfResult = 0;
    unsigned int vrfSetCalls = 0;
    uint32_t nextSwIfIndex = 200;
    int settingsResult = 0;
    int mplsQueryResult = 0;
    unsigned int mplsQueryCalls = 0;
    unsigned int mplsQueryFailOnCall = 0;
    int mtuResult = 0;
    int mplsResult = 0;
    int adminResult = 0;
    int deleteResult = 0;
    // sw_interface_ip6_enable_disable() result, and the per-interface lock count
    // VPP keeps for IPv6 (ip6_link_t.il_locks), so a test can check that the
    // enables a RIF takes are released again.
    int ip6EnableResult = 0;
    std::map<std::string, int> ip6Locks;
    // sw_interface_set_mac() result, and the MAC each interface carries.
    int macResult = 0;
    std::map<std::string, std::string> macs;
    // sw_interface_set_ip_directed_broadcast() result, for the ref-counted
    // directed-broadcast flag of the sub-net broadcast neighbors.
    int directedBcastResult = 0;
    uint32_t routeStatsIndex = 42;
    std::vector<int> routeResults;
    std::vector<VppRouteCall> routeCalls;

    // sonic-ext features reported as enabled. "iface-loopback" keeps its own
    // flag because the loopback tests predate this set.
    std::set<std::string> enabledFeatures;

    // The nodes vpp_add_node_next() adds as nexts, with the next index each
    // gets; it fails for any other. None by default, so the one-time setup of
    // the L2 punt classify tables fails, which the VLAN member path tolerates.
    std::map<std::string, uint32_t> nodeNexts;
    uint32_t nextClassifyTable = 0;
    std::vector<VppClassifySession> classifySessions;

    // Policy Based Hashing.
    int pbhProfileResult = 0;
    int pbhTableResult = 0;
    int pbhTableDelResult = 0;
    int pbhAttachResult = 0;
    int pbhStatsResult = 0;
    uint32_t nextPbhProfileIndex = 10;
    uint32_t nextPbhTableIndex = 20;
    vpp_ace_stats_t pbhStats = {};
    std::set<uint32_t> pbhProfiles;
    std::set<uint32_t> pbhTables;
    std::vector<VppPbhProfileCall> pbhProfileCalls;
    std::vector<VppPbhTableCall> pbhTableCalls;
    std::vector<uint32_t> pbhTableDels;
    std::vector<VppPbhAttachCall> pbhAttachCalls;
    std::vector<VppRuleStatsCall> ruleStatsCalls;

    std::map<std::string, uint32_t> interfaceVrfs;
    std::set<uint32_t> vrfTables;
    std::map<std::string, vpp_interface_stats_t> interfaceStats;
    std::map<std::string, int> actions;
    std::set<std::string> interfaces;
    std::set<std::string> lcpPairs;
    std::set<std::string> bridgePorts;
    // VPP's bridge domains: the interfaces in each, a VLAN's bridge virtual
    // interface included, and the MAC age set on each (none: 0, VPP's default).
    // A bridge domain exists from its first interface until it is deleted.
    std::map<uint32_t, std::set<std::string>> bdMembers;
    std::map<uint32_t, uint8_t> bdMacAges;
    // the bridge domains VPP fails to set the MAC age of
    std::set<uint32_t> macAgeFailures;
    std::vector<VppCall> calls;
    std::map<std::string, bool> adminUp;
    std::map<std::string, std::array<uint32_t, 4>> mtus;
    std::map<std::string, unsigned int> mplsReferences;
    int l2fibDumpResult = 0;
    // the next dumps that fail, before l2fibDumpResult applies
    unsigned int l2fibDumpFailures = 0;
    unsigned int l2fibDumpCalls = 0;
    std::vector<vpp_l2fib_entry_t> l2fib;
    // Link state VPP reports, by interface name. Missing is down.
    std::map<std::string, bool> linkUp;
    int bfdDumpResult = 0;
    unsigned int bfdDumpCalls = 0;
    std::vector<vpp_bfd_state_notif_t> bfdSessions;
    bool natSupported = true;
    // NAT sessions VPP reports, by inside address (network order)
    std::map<uint32_t, std::vector<vpp_nat44_session_t>> natSessions;
    // sw_if_index vpp_get_swif_idx_by_name() returns for these names; any
    // other name gets the next nextSwIfIndex
    std::map<std::string, uint32_t> swIfIndexByName;
    // /sonic-ext/drop-reason/<reason> counters: reason -> sw_if_index ->
    // one value per VPP thread
    std::map<std::string, std::map<uint32_t, std::vector<uint64_t>>> dropReasonStats;
    int dropReasonDumpResult = 0;
    unsigned int dropReasonDumpCalls = 0;
    // Names of the interfaces the mock creates, by sw_if_index: IP-in-IP tunnels
    std::map<uint32_t, std::string> swifNames;
    // vpp_ipip_tunnel_del() result: on failure, VPP keeps the tunnel
    int ipipDelResult = 0;

    void reset();
    bool isAdminUp(const std::string& hwif) const;
    std::vector<VppCall> callsTo(const std::string& api, const std::string& name) const;

    /**
     * @brief Rule set of the last vpp_pbh_table_add_replace() call, which is
     * what VPP holds for the table the caller last programmed.
     */
    const std::vector<vpp_pbh_rule_t>& lastPbhRules() const;
};

VppMock& vppMock();

/*
 * The VPP event stream a switch's event thread reads: it waits once per pass, and
 * takes the events a test delivers, in order. Kept out of VppMock, which tests
 * reset, as that thread uses it.
 */
void deliverVppEvents(std::initializer_list<vpp_event_info_t *> events);

// The event thread has taken the delivered events, and posted them once it
// waits for the next ones
bool waitForVppEventsTaken();

// A call the event thread made on the event socket
struct VppEventCall
{
    std::string api;
    int result;
    std::chrono::steady_clock::time_point time;
};

// Forgets the calls made so far; the next pingFailures pings fail
void resetVppEventCalls(int pingFailures = 0);

std::vector<VppEventCall> vppEventCalls();

class TestableSwitchVpp : public saivs::SwitchVpp
{
public:
    using SwitchVpp::SwitchVpp;

protected:
    int vs_set_dev_admin_up(const char *dev, bool up) override;
    int exec(const std::string& command, std::string& output) override;
};
