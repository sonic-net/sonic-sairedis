#pragma once

#include "vpp/SwitchVpp.h"
#include "vpp/vppxlate/SaiIntfStats.h"
#include "vpp/vppxlate/SaiAclStats.h"

#include <array>
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

struct VppMock
{
    bool loopbackEnabled = false;
    std::string queriedFeature;
    unsigned int loopbackCalls = 0;
    int loopbackAction = -1;
    int loopbackResult = 0;
    int featureResult = 0;
    unsigned int featureCalls = 0;
    std::vector<std::string> queriedFeatures;
    // -1 keeps the l2 classify punt setup from running at all
    int addNodeNextResult = -1;
    uint32_t nextNodeIndex = 10;
    uint32_t nextClassifyTable = 0;
    int ip2meResult = 0;
    int statsResult = 0;
    int lcpResult = 0;
    unsigned int statsCalls = 0;
    unsigned int interfaceCreates = 0;
    uint32_t kernelVrfId = 0;
    int kernelVrfResult = 0;
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

class TestableSwitchVpp : public saivs::SwitchVpp
{
public:
    using SwitchVpp::SwitchVpp;

protected:
    int vs_set_dev_admin_up(const char *dev, bool up) override;
    int exec(const std::string& command, std::string& output) override;
};
