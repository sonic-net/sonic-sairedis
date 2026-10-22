#pragma once

#include "vpp/SwitchVpp.h"
#include "vpp/vppxlate/SaiIntfStats.h"

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

    void reset();
    bool isAdminUp(const std::string& hwif) const;
    std::vector<VppCall> callsTo(const std::string& api, const std::string& name) const;
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
