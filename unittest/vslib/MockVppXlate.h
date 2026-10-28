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
#include <utility>
#include <vector>

struct VppCall
{
    std::string api;
    std::string name;
    uint32_t id;
    uint32_t sub_id;
    bool flag;
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

struct VppCoppNeighborMissBindCall
{
    std::string policerName;
    bool isBind;
};

struct VppCoppDhcpBindCall
{
    std::string policerName;
    bool isIpv6;
    bool isBind;
};

struct VppCoppIfoutBindCall
{
    uint16_t ethertype;
    std::string policerName;
    bool isBind;
    bool isIp4TtlExpiring;
};

struct VppCoppIp2meBindCall
{
    std::string policerName;
    uint16_t tcpPort;
    bool conditional;
    bool isBind;
};

struct VppClassifyTable
{
    uint32_t skip = 0;
    uint32_t match = 0;
    uint32_t nextTable = ~0u;
    std::vector<uint8_t> mask;
    std::map<std::vector<uint8_t>, uint32_t> sessions;
};

struct VppMock
{
    bool loopbackEnabled = false;
    std::string queriedFeature;
    unsigned int loopbackCalls = 0;
    int loopbackAction = -1;
    int loopbackResult = 0;
    int featureResult = 0;
    unsigned int featureCalls = 0;
    int statsResult = 0;
    int lcpResult = 0;
    unsigned int statsCalls = 0;
    unsigned int interfaceCreates = 0;
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

    // sonic-ext features reported as enabled. "iface-loopback" keeps its own
    // flag because the loopback tests predate this set.
    std::set<std::string> enabledFeatures;

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

    // CoPP semantic NEIGHBOR_MISS binding.
    int coppNeighborMissBindResult = 0;
    unsigned int coppNeighborMissBindFailOnCall = 0;
    std::vector<VppCoppNeighborMissBindCall> coppNeighborMissBindCalls;

    // CoPP semantic DHCPv4/DHCPv6 bindings.
    int coppDhcpBindResult = 0;
    unsigned int coppDhcpBindFailOnCall = 0;
    std::set<unsigned int> coppDhcpBindFailOnCalls;
    std::vector<VppCoppDhcpBindCall> coppDhcpBindCalls;

    // CoPP interface-output and TTL punt bindings.
    int coppIfoutBindResult = 0;
    unsigned int coppIfoutBindFailOnCall = 0;
    int coppTtlPuntBindResult = 0;
    unsigned int coppTtlPuntBindFailOnCall = 0;
    std::vector<VppCoppIfoutBindCall> coppIfoutBindCalls;
    std::vector<bool> coppTtlPuntBindCalls;

    // CoPP name-keyed IP2ME and BGP bindings.
    int coppIp2meBindResult = 0;
    unsigned int coppIp2meBindFailOnCall = 0;
    std::set<unsigned int> coppIp2meBindFailOnCalls;
    std::vector<VppCoppIp2meBindCall> coppIp2meBindCalls;

    // Minimal VPP classify model for VLAN-member punt tests.
    bool classifyAvailable = false;
    uint32_t nextClassifyTable = 0;
    std::map<uint32_t, VppClassifyTable> classifyTables;
    std::map<std::pair<std::string, std::string>, uint32_t> nodeNexts;
    std::map<std::string, uint32_t> nodeNextCounts;
    std::map<std::string, std::array<uint32_t, 3>> l2ClassifyTables;

    std::map<std::string, uint32_t> interfaceVrfs;
    std::set<uint32_t> vrfTables;
    std::map<std::string, vpp_interface_stats_t> interfaceStats;
    std::map<std::string, int> actions;
    std::set<std::string> interfaces;
    std::set<std::string> lcpPairs;
    std::set<std::string> bridgePorts;
    std::vector<VppCall> calls;
    std::map<std::string, bool> adminUp;
    std::map<std::string, std::array<uint32_t, 4>> mtus;
    std::map<std::string, unsigned int> mplsReferences;
    int l2fibDumpResult = 0;
    unsigned int l2fibDumpCalls = 0;
    std::vector<vpp_l2fib_entry_t> l2fib;
    // Link state VPP reports, by interface name. Missing is down.
    std::map<std::string, bool> linkUp;
    int bfdDumpResult = 0;
    unsigned int bfdDumpCalls = 0;
    std::vector<vpp_bfd_state_notif_t> bfdSessions;

    void reset();
    bool isAdminUp(const std::string& hwif) const;
    std::vector<VppCall> callsTo(const std::string& api, const std::string& name) const;

    /**
     * @brief Rule set of the last vpp_pbh_table_add_replace() call, which is
     * what VPP holds for the table the caller last programmed.
     */
    const std::vector<vpp_pbh_rule_t>& lastPbhRules() const;
    uint32_t classifyFrame(
            const std::string& hwif,
            size_t slot,
            const std::vector<uint8_t>& frame) const;
    uint32_t nodeNext(
            const std::string& node,
            const std::string& next) const;
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
