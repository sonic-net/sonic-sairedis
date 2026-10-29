#include "MockVppXlate.h"

#include <arpa/inet.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <deque>
#include <mutex>

#include <unistd.h>

namespace
{
    std::atomic<int> g_vppEventWaits{0};
    std::mutex g_vppEventsMutex;
    std::deque<vpp_event_info_t *> g_vppEvents;
    std::vector<VppEventCall> g_vppEventCalls;
    int g_vppPingFailures = 0;

    bool vppEventsPending()
    {
        SWSS_LOG_ENTER();
        std::lock_guard<std::mutex> lock(g_vppEventsMutex);
        return !g_vppEvents.empty();
    }

    int recordVppEventCall(const std::string& api, int result)
    {
        SWSS_LOG_ENTER();
        std::lock_guard<std::mutex> lock(g_vppEventsMutex);
        g_vppEventCalls.push_back({api, result, std::chrono::steady_clock::now()});
        return result;
    }
}

void resetVppEventCalls(int pingFailures)
{
    SWSS_LOG_ENTER();
    std::lock_guard<std::mutex> lock(g_vppEventsMutex);
    for (auto *ev : g_vppEvents)
    {
        free(ev);
    }
    g_vppEvents.clear();
    g_vppEventCalls.clear();
    g_vppPingFailures = pingFailures;
}

std::vector<VppEventCall> vppEventCalls()
{
    SWSS_LOG_ENTER();
    std::lock_guard<std::mutex> lock(g_vppEventsMutex);
    return g_vppEventCalls;
}

void deliverVppEvents(std::initializer_list<vpp_event_info_t *> events)
{
    SWSS_LOG_ENTER();
    std::lock_guard<std::mutex> lock(g_vppEventsMutex);
    g_vppEvents.insert(g_vppEvents.end(), events);
}

bool waitForVppEventsTaken()
{
    SWSS_LOG_ENTER();
    for (int i = 0; i < 500 && vppEventsPending(); i++)
    {
        usleep(10 * 1000);
    }
    if (vppEventsPending())
    {
        return false;
    }
    int waits = g_vppEventWaits.load();
    for (int i = 0; i < 500 && g_vppEventWaits.load() == waits; i++)
    {
        usleep(10 * 1000);
    }
    return g_vppEventWaits.load() != waits;
}

VppMock& vppMock()
{
    SWSS_LOG_ENTER();
    static VppMock mock;
    return mock;
}

void VppMock::reset()
{
    SWSS_LOG_ENTER();
    *this = VppMock{};
}

bool VppMock::isAdminUp(const std::string& hwif) const
{
    SWSS_LOG_ENTER();
    auto it = adminUp.find(hwif);
    return it != adminUp.end() && it->second;
}

std::vector<VppCall> VppMock::callsTo(const std::string& api, const std::string& name) const
{
    SWSS_LOG_ENTER();
    std::vector<VppCall> matching;
    for (const auto& call : calls)
    {
        if (call.api == api && call.name == name)
        {
            matching.push_back(call);
        }
    }
    return matching;
}

const std::vector<vpp_pbh_rule_t>& VppMock::lastPbhRules() const
{
    SWSS_LOG_ENTER();
    static const std::vector<vpp_pbh_rule_t> none;
    return pbhTableCalls.empty() ? none : pbhTableCalls.back().rules;
}

int TestableSwitchVpp::vs_set_dev_admin_up(const char *, bool)
{
    SWSS_LOG_ENTER();
    return 0;
}

int TestableSwitchVpp::exec(const std::string& command, std::string& output)
{
    SWSS_LOG_ENTER();
    vppMock().kernelCommands.push_back(command);
    if (command.find(" -o link show") != std::string::npos)
    {
        output = vppMock().kernelLinkNames;
        return vppMock().kernelLinkResult;
    }
    const std::string linkCommand = "/sbin/ip link show dev ";
    if (command.compare(0, linkCommand.size(), linkCommand) == 0 &&
            !vppMock().kernelInterfaces.empty() &&
            vppMock().kernelInterfaces.count(command.substr(linkCommand.size())) == 0)
    {
        output.clear();
        return 1;
    }
    output = std::to_string(vppMock().kernelVrfId) + "\n";
    return vppMock().kernelVrfResult;
}

extern "C" int __wrap_get_interface_state(const char *name, vpp_interface_state_t *state)
{
    SWSS_LOG_ENTER();
    const auto& mtu = vppMock().mtus[name];
    std::copy(mtu.begin(), mtu.end(), state->mtu);
    state->admin_up = vppMock().isAdminUp(name);
    return vppMock().settingsResult;
}

extern "C" int __wrap_get_interface_mpls_enabled(const char *name, bool *enabled)
{
    SWSS_LOG_ENTER();
    *enabled = vppMock().mplsReferences[name] != 0;
    if (++vppMock().mplsQueryCalls == vppMock().mplsQueryFailOnCall)
    {
        return -1;
    }
    return vppMock().mplsQueryResult;
}

extern "C" int __wrap_sw_interface_set_mtu(const char *name, uint32_t mtu)
{
    SWSS_LOG_ENTER();
    int ret = vppMock().mtuResult;
    vppMock().mtuResult = 0;
    if (ret == 0)
    {
        vppMock().mtus[name].fill(mtu);
    }
    return ret;
}

extern "C" int __wrap_sw_interface_set_mtu_all(const char *name, const uint32_t mtu[4])
{
    SWSS_LOG_ENTER();
    std::copy(mtu, mtu + 4, vppMock().mtus[name].begin());
    return 0;
}

extern "C" int __wrap_sw_interface_set_mpls_enable(const char *name, bool enabled)
{
    SWSS_LOG_ENTER();
    int ret = vppMock().mplsResult;
    vppMock().mplsResult = 0;
    if (ret == 0)
    {
        // VPP counts enable references; this API is not a boolean setter.
        if (enabled)
        {
            ++vppMock().mplsReferences[name];
        }
        else
        {
            --vppMock().mplsReferences[name];
        }
    }
    return ret;
}

extern "C" int __wrap_mpls_table_add_del(uint32_t, bool)
{
    SWSS_LOG_ENTER();
    return 0;
}

extern "C" int __wrap_vpp_sonic_ext_feature_get(const char *feature, bool *enabled)
{
    SWSS_LOG_ENTER();
    ++vppMock().featureCalls;
    vppMock().queriedFeature = feature;
    *enabled = vppMock().queriedFeature == "iface-loopback"
            ? vppMock().loopbackEnabled
            : vppMock().enabledFeatures.count(vppMock().queriedFeature) != 0;
    return vppMock().featureResult;
}

/*
 * PBH. The indices are handed out by VPP, so the mock allocates them the same
 * way: an in-index of ~0 asks for a new one, anything else names an existing
 * object to replace or drop.
 */
extern "C" int __wrap_vpp_pbh_profile_add_del(bool is_add, const vpp_pbh_hash_field_t *fields,
        uint32_t n_fields, uint32_t *profile_index)
{
    SWSS_LOG_ENTER();

    VppPbhProfileCall call;

    call.is_add = is_add;
    call.profile_index = *profile_index;

    if (fields != nullptr && n_fields != 0)
    {
        call.fields.assign(fields, fields + n_fields);
    }

    vppMock().pbhProfileCalls.push_back(call);

    if (vppMock().pbhProfileResult != 0)
    {
        return vppMock().pbhProfileResult;
    }

    if (is_add)
    {
        if (*profile_index == ~0u)
        {
            *profile_index = vppMock().nextPbhProfileIndex++;
        }
        vppMock().pbhProfiles.insert(*profile_index);
    }
    else
    {
        vppMock().pbhProfiles.erase(*profile_index);
    }

    return 0;
}

extern "C" int __wrap_vpp_pbh_table_add_replace(const char *name, const vpp_pbh_rule_t *rules,
        uint32_t n_rules, uint32_t *table_index)
{
    SWSS_LOG_ENTER();

    VppPbhTableCall call;

    call.named = name != nullptr;
    call.name = name ? name : "";
    call.table_index = *table_index;

    if (rules != nullptr && n_rules != 0)
    {
        call.rules.assign(rules, rules + n_rules);
    }

    vppMock().pbhTableCalls.push_back(call);

    if (vppMock().pbhTableResult != 0)
    {
        return vppMock().pbhTableResult;
    }

    if (*table_index == ~0u)
    {
        *table_index = vppMock().nextPbhTableIndex++;
    }

    vppMock().pbhTables.insert(*table_index);

    return 0;
}

extern "C" int __wrap_vpp_pbh_table_del(uint32_t table_index)
{
    SWSS_LOG_ENTER();

    vppMock().pbhTableDels.push_back(table_index);

    if (vppMock().pbhTableDelResult != 0)
    {
        return vppMock().pbhTableDelResult;
    }

    vppMock().pbhTables.erase(table_index);

    return 0;
}

extern "C" int __wrap_vpp_pbh_interface_attach_detach(const char *hwif_name,
        uint32_t table_index, bool attach)
{
    SWSS_LOG_ENTER();

    vppMock().pbhAttachCalls.push_back({hwif_name ? hwif_name : "", table_index, attach});

    return vppMock().pbhAttachResult;
}

extern "C" int __wrap_vpp_rule_stats_query(vpp_rule_stats_type_t type, uint32_t table_index,
        uint32_t rule_index, vpp_ace_stats_t *stats)
{
    SWSS_LOG_ENTER();

    vppMock().ruleStatsCalls.push_back({type, table_index, rule_index});

    if (vppMock().pbhStatsResult != 0)
    {
        return vppMock().pbhStatsResult;
    }

    *stats = vppMock().pbhStats;
    stats->ace_index = rule_index;

    return 0;
}

extern "C" int __wrap_vpp_iface_loopback_set_action(const char *name, int action)
{
    SWSS_LOG_ENTER();
    ++vppMock().loopbackCalls;
    vppMock().loopbackAction = action;
    if (vppMock().loopbackResult == 0)
    {
        vppMock().actions[name] = action;
    }
    return vppMock().loopbackResult;
}

extern "C" int __wrap_vpp_intf_stats_query(const char *name, vpp_interface_stats_t *stats)
{
    SWSS_LOG_ENTER();
    ++vppMock().statsCalls;
    *stats = vppMock().interfaceStats[name];
    return vppMock().statsResult;
}

extern "C" int __wrap_create_bvi_interface(uint8_t *, uint32_t instance)
{
    SWSS_LOG_ENTER();
    ++vppMock().interfaceCreates;
    return vppMock().interfaces.insert("bvi" + std::to_string(instance)).second ? 0 : -1;
}

extern "C" int __wrap_delete_bvi_interface(const char *name)
{
    SWSS_LOG_ENTER();
    if (vppMock().deleteResult != 0)
    {
        return vppMock().deleteResult;
    }
    vppMock().actions.erase(name);
    vppMock().adminUp.erase(name);
    vppMock().mtus.erase(name);
    vppMock().mplsReferences.erase(name);
    return vppMock().interfaces.erase(name) == 1 ? 0 : -1;
}

extern "C" int __wrap_create_sub_interface(const char *parent, uint32_t id, uint16_t vlan)
{
    SWSS_LOG_ENTER();
    ++vppMock().interfaceCreates;
    vppMock().calls.push_back({"create_sub_interface", parent, vlan, id, true});
    return vppMock().interfaces.insert(std::string(parent) + "." + std::to_string(id)).second ? 0 : -1;
}

extern "C" int __wrap_delete_sub_interface(const char *parent, uint32_t id)
{
    SWSS_LOG_ENTER();
    vppMock().calls.push_back({"delete_sub_interface", parent, id, id, false});
    return __wrap_delete_bvi_interface((std::string(parent) + "." + std::to_string(id)).c_str());
}

extern "C" int __wrap_configure_lcp_interface(const char *name, const char *, bool add)
{
    SWSS_LOG_ENTER();
    if (add)
    {
        if (vppMock().lcpResult != 0)
        {
            return vppMock().lcpResult;
        }
        return vppMock().lcpPairs.insert(name).second ? 0 : -1;
    }
    return vppMock().lcpPairs.erase(name) == 1 ? 0 : -1;
}

extern "C" int __wrap_set_sw_interface_l2_bridge(const char *name, uint32_t, bool add, uint32_t)
{
    SWSS_LOG_ENTER();
    if (add)
    {
        vppMock().bridgePorts.insert(name);
    }
    else
    {
        vppMock().bridgePorts.erase(name);
    }
    return 0;
}

extern "C" int __wrap_refresh_interfaces_list() {
    SWSS_LOG_ENTER();
    return 0;
}
extern "C" void __wrap_vpp_set_mac_event_cb(vpp_mac_event_cb_fn, void *) {
    SWSS_LOG_ENTER();
}
extern "C" int __wrap_vpp_l2fib_dump(uint32_t, vpp_l2fib_entry_cb_fn cb, void *ctx)
{
    SWSS_LOG_ENTER();
    ++vppMock().l2fibDumpCalls;
    if (vppMock().l2fibDumpResult == 0)
    {
        for (const auto& entry : vppMock().l2fib)
        {
            cb(&entry, ctx);
        }
    }
    return vppMock().l2fibDumpResult;
}
extern "C" int __wrap_vpp_bfd_session_dump(vpp_bfd_session_cb_fn cb, void *ctx)
{
    SWSS_LOG_ENTER();
    ++vppMock().bfdDumpCalls;
    if (vppMock().bfdDumpResult == 0)
    {
        for (const auto& session : vppMock().bfdSessions)
        {
            cb(&session, ctx);
        }
    }
    return vppMock().bfdDumpResult;
}
extern "C" int __wrap_vpp_ip_flow_hash_set(uint32_t, uint32_t, int) {
    SWSS_LOG_ENTER();
    return 0;
}
extern "C" int __wrap_set_interface_vrf(const char *name, uint32_t subId, uint32_t table, bool)
{
    SWSS_LOG_ENTER();
    ++vppMock().vrfSetCalls;
    if (vppMock().setVrfResult == 0)
    {
        const auto ifname = std::string(name) + (subId ? "." + std::to_string(subId) : "");
        vppMock().interfaceVrfs[ifname] = table;
    }
    return vppMock().setVrfResult;
}

extern "C" int __wrap_get_interface_vrf(const char *name, bool, uint32_t *table)
{
    SWSS_LOG_ENTER();
    *table = vppMock().interfaceVrfs[name];
    return vppMock().getVrfResult;
}

extern "C" int __wrap_ip_vrf_add(uint32_t table, const char *, bool)
{
    SWSS_LOG_ENTER();
    vppMock().vrfTables.insert(table);
    return 0;
}

extern "C" int __wrap_ip_vrf_del(uint32_t table, const char *, bool)
{
    SWSS_LOG_ENTER();
    vppMock().vrfTables.erase(table);
    return 0;
}

extern "C" int __wrap_interface_ip_address_del_all(const char *) {
    SWSS_LOG_ENTER();
    return 0;
}

extern "C" {

int __wrap_init_vpp_client() {
    SWSS_LOG_ENTER();
    return 0;
}
// the event thread waits for VPP to write to the event socket between passes
int __wrap_vpp_wait_for_events(int) {
    SWSS_LOG_ENTER();
    g_vppEventWaits++;
    // like poll(): returns once there is something to read, or after the interval
    if (!vppEventsPending())
    {
        usleep(10 * 1000);
    }
    return recordVppEventCall("vpp_wait_for_events", vppEventsPending() ? 1 : 0);
}
int __wrap_vpp_drain_events() {
    SWSS_LOG_ENTER();
    return recordVppEventCall("vpp_drain_events", 0);
}
int __wrap_vpp_sync_for_events() {
    SWSS_LOG_ENTER();
    int result = 0;
    {
        std::lock_guard<std::mutex> lock(g_vppEventsMutex);
        if (g_vppPingFailures > 0)
        {
            --g_vppPingFailures;
            result = -1;
        }
    }
    return recordVppEventCall("vpp_sync_for_events", result);
}
vpp_event_info_t * __wrap_vpp_ev_dequeue() {
    SWSS_LOG_ENTER();
    std::lock_guard<std::mutex> lock(g_vppEventsMutex);
    if (g_vppEvents.empty())
    {
        return NULL;
    }
    auto *ev = g_vppEvents.front();
    g_vppEvents.pop_front();
    return ev;
}

/*
 * VPP's vnet_sw_interface_set_flags() refuses admin up on a sub-interface whose
 * parent ("super-interface") is admin down, and sw_interface_set_flags returns
 * -1 for it. Admin down is always accepted.
 */
int __wrap_interface_set_state(const char *hwif_name, bool is_up)
{
    SWSS_LOG_ENTER();

    std::string name = hwif_name ? hwif_name : "";

    int ret = vppMock().adminResult;
    vppMock().adminResult = 0;

    auto dot = name.rfind('.');

    if (ret == 0 && is_up && dot != std::string::npos && !vppMock().isAdminUp(name.substr(0, dot)))
    {
        ret = -1;
    }
    else if (ret == 0)
    {
        vppMock().adminUp[name] = is_up;
    }

    vppMock().calls.push_back({"interface_set_state", name, 0, (uint32_t)ret, is_up});
    return ret;
}

int __wrap_interface_get_state(const char *hwif_name, bool *link_is_up)
{
    SWSS_LOG_ENTER();
    auto it = vppMock().linkUp.find(hwif_name ? hwif_name : "");
    *link_is_up = it != vppMock().linkUp.end() && it->second;
    return 0;
}

int __wrap_interface_set_promiscuous(const char *hwif_name, bool enable) {
    SWSS_LOG_ENTER();
    return 0;
}

int __wrap_set_l2_interface_vlan_tag_rewrite(const char *hwif_name, uint32_t tag1, uint32_t tag2, uint32_t push_dot1q, uint32_t vtr_op) {
    SWSS_LOG_ENTER();
    return 0;
}
int __wrap_set_bridge_domain_flags(uint32_t bd_id, vpp_bd_flags_t flag, bool enable) {
    SWSS_LOG_ENTER();
    return 0;
}

int __wrap_bridge_domain_get_member_count(uint32_t bd_id, uint32_t *member_count)
{
    SWSS_LOG_ENTER();

    *member_count = 0;
    return 0;
}

int __wrap_vpp_bridge_domain_add_del(uint32_t bridge_id, bool is_add) {
    SWSS_LOG_ENTER();
    return 0;
}
uint32_t __wrap_vpp_get_swif_idx_by_name(const char *hwif_name)
{
    SWSS_LOG_ENTER();

    return vppMock().nextSwIfIndex++;
}

// no classify punt in these tests: its one-time setup fails, which the VLAN member path tolerates
int __wrap_vpp_add_node_next(const char *node_name, const char *next_name, uint32_t *next_index) {
    SWSS_LOG_ENTER();
    return -1;
}

int __wrap_vpp_classify_set_interface_l2_tables(const char *hwif_name, uint32_t ip4_table_index,
        uint32_t ip6_table_index, uint32_t other_table_index, bool is_input) {
    SWSS_LOG_ENTER();
    return 0;
}

/*
 * vpp_nat44_* calls. Recorded as name: interface, or the mapping for a static
 * mapping; id: table, or the session count; flag: is_add or enable.
 */
bool __wrap_vpp_nat44_ed_supported()
{
    SWSS_LOG_ENTER();

    return vppMock().natSupported;
}

int __wrap_vpp_nat44_ed_enable_disable(bool enable, uint32_t sessions)
{
    SWSS_LOG_ENTER();

    vppMock().calls.push_back({"vpp_nat44_ed_enable_disable", "", sessions, 0, enable});
    return 0;
}

int __wrap_vpp_nat44_forwarding_enable_disable(bool enable)
{
    SWSS_LOG_ENTER();

    vppMock().calls.push_back({"vpp_nat44_forwarding_enable_disable", "", 0, 0, enable});
    return 0;
}

int __wrap_vpp_nat44_ed_output_interface_add_del(const char *hwif_name, bool is_add)
{
    SWSS_LOG_ENTER();

    vppMock().calls.push_back({"vpp_nat44_ed_output_interface_add_del", hwif_name ? hwif_name : "", 0, 0, is_add});
    return 0;
}

int __wrap_vpp_nat44_static_mapping_add_del(const vpp_nat44_static_mapping_t *m, bool is_add)
{
    SWSS_LOG_ENTER();

    char l[INET_ADDRSTRLEN], e[INET_ADDRSTRLEN];

    inet_ntop(AF_INET, &m->local, l, sizeof(l));
    inet_ntop(AF_INET, &m->external, e, sizeof(e));

    // name: "local:port>external:port/protocol" (no ports for an address-only mapping); id: table
    std::string name = m->addr_only
        ? std::string(l) + ">" + e
        : std::string(l) + ":" + std::to_string(m->local_port) + ">" + e + ":" +
          std::to_string(m->external_port) + "/" + std::to_string(m->protocol);

    vppMock().calls.push_back({"vpp_nat44_static_mapping_add_del", name, m->vrf_id, 0, is_add});
    return 0;
}

int __wrap_vpp_nat44_user_sessions_dump(struct in_addr inside_addr, uint32_t vrf_id,
                                        vpp_nat44_session_cb_fn cb, void *ctx)
{
    SWSS_LOG_ENTER();

    for (auto &s: vppMock().natSessions[inside_addr.s_addr])
    {
        cb(&s, ctx);
    }

    return 0;
}

}
