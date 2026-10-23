#include "MockVppXlate.h"

#include <algorithm>

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

int TestableSwitchVpp::vs_set_dev_admin_up(const char *, bool)
{
    SWSS_LOG_ENTER();
    return 0;
}

int TestableSwitchVpp::exec(const std::string&, std::string& output)
{
    SWSS_LOG_ENTER();
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
    *enabled = vppMock().queriedFeature == "iface-loopback" && vppMock().loopbackEnabled;
    return vppMock().featureResult;
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

extern "C" int __wrap_set_sw_interface_l2_bridge(const char *name, uint32_t bd_id, bool add, uint32_t)
{
    SWSS_LOG_ENTER();
    if (add)
    {
        vppMock().bridgePorts.insert(name);
        vppMock().bdMembers[bd_id].insert(name);
    }
    else
    {
        vppMock().bridgePorts.erase(name);
        vppMock().bdMembers[bd_id].erase(name);
    }
    return 0;
}

extern "C" int __wrap_refresh_interfaces_list() {
    SWSS_LOG_ENTER();
    return 0;
}
extern "C" int __wrap_vpp_want_l2_macs_events2(bool, vpp_mac_event_cb_fn, void *) {
    SWSS_LOG_ENTER();
    return 0;
}
extern "C" int __wrap_vpp_bridge_domain_set_mac_age(uint32_t bd_id, uint8_t mac_age)
{
    SWSS_LOG_ENTER();
    // VPP rejects a bridge domain it doesn't have
    bool fails = vppMock().macAgeFailures.count(bd_id) != 0 || vppMock().bdMembers.count(bd_id) == 0;
    vppMock().calls.push_back({"vpp_bridge_domain_set_mac_age", std::to_string(bd_id), bd_id, mac_age, !fails});
    if (fails)
    {
        return -1;
    }
    vppMock().bdMacAges[bd_id] = mac_age;
    return 0;
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
int __wrap_vpp_sync_for_events() {
    SWSS_LOG_ENTER();
    return 0;
}
vpp_event_info_t * __wrap_vpp_ev_dequeue() {
    SWSS_LOG_ENTER();
    return NULL;
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

    auto bd = vppMock().bdMembers.find(bd_id);

    *member_count = bd == vppMock().bdMembers.end() ? 0 : static_cast<uint32_t>(bd->second.size());
    return 0;
}

int __wrap_vpp_bridge_domain_add_del(uint32_t bridge_id, bool is_add) {
    SWSS_LOG_ENTER();
    if (!is_add)
    {
        vppMock().bdMembers.erase(bridge_id);
        vppMock().bdMacAges.erase(bridge_id);
    }
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

}
