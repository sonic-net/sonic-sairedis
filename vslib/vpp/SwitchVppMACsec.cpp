#include "SwitchVpp.h"
#include "SwitchVppUtils.h"

#include "meta/sai_serialize.h"

#include "swss/logger.h"

#include "vppxlate/SaiVppXlate.h"

#include <cstring>
#include <tuple>

using namespace saivs;

namespace
{
    bool isXpnCipher(int cipher)
    {
        return cipher == SAI_MACSEC_CIPHER_SUITE_GCM_AES_XPN_128
            || cipher == SAI_MACSEC_CIPHER_SUITE_GCM_AES_XPN_256;
    }

    bool is256Cipher(int cipher)
    {
        return cipher == SAI_MACSEC_CIPHER_SUITE_GCM_AES_256
            || cipher == SAI_MACSEC_CIPHER_SUITE_GCM_AES_XPN_256;
    }

    /*
     * SAI stores a 32-byte SAK array. 128-bit suites occupy bytes 16..31
     * (same truncation as SwitchStateBase::loadMACsecAttrFromMACsecSA).
     * 256-bit suites use the full 32 bytes. Never log sak_out.
     */
    void fillSak(
            int cipher,
            const uint8_t sak_in[32],
            uint8_t sak_out[32],
            uint8_t &sak_len)
    {
        memset(sak_out, 0, 32);

        if (is256Cipher(cipher))
        {
            memcpy(sak_out, sak_in, 32);
            sak_len = 32;
        }
        else
        {
            memcpy(sak_out, sak_in + 16, 16);
            sak_len = 16;
        }
    }
}

sai_status_t SwitchVpp::resolveMacsecSwIfIndex(
        _In_ sai_object_id_t port_id,
        _Out_ uint32_t &sw_if_index)
{
    SWSS_LOG_ENTER();

    std::string hwif_name = m_ifaceRegistry.resolveHwIfName(port_id, 0);

    if (hwif_name.empty())
    {
        SWSS_LOG_ERROR("failed to resolve hwif for MACsec port %s",
                       sai_serialize_object_id(port_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    int idx = get_sw_if_idx(hwif_name.c_str());

    if (idx < 0)
    {
        SWSS_LOG_ERROR("failed to get sw_if_index for %s (port %s)",
                       hwif_name.c_str(),
                       sai_serialize_object_id(port_id).c_str());
        return SAI_STATUS_FAILURE;
    }

    sw_if_index = static_cast<uint32_t>(idx);
    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::bindMacsecScToPort(
        _Inout_ VppMacsecScState &sc)
{
    SWSS_LOG_ENTER();

    if (sc.sw_if_index != ~0u)
    {
        return SAI_STATUS_SUCCESS;
    }

    auto flow_it = m_vppMacsecFlows.find(sc.flow_id);

    if (flow_it != m_vppMacsecFlows.end() && flow_it->second.sw_if_index != ~0u)
    {
        sc.sw_if_index = flow_it->second.sw_if_index;
        return SAI_STATUS_SUCCESS;
    }

    sai_object_id_t port_id = SAI_NULL_OBJECT_ID;

    if (sc.flow_id != SAI_NULL_OBJECT_ID
            && findPortByMACsecFlow(sc.flow_id, port_id) == SAI_STATUS_SUCCESS)
    {
        return resolveMacsecSwIfIndex(port_id, sc.sw_if_index);
    }

    return SAI_STATUS_FAILURE;
}

sai_status_t SwitchVpp::programVppMacsecPortAndSc(
        _Inout_ VppMacsecScState &sc)
{
    SWSS_LOG_ENTER();

    if (sc.sw_if_index == ~0u)
    {
        return SAI_STATUS_FAILURE;
    }

    VppMacsecPortState *port = nullptr;

    for (auto &kv : m_vppMacsecPorts)
    {
        if (kv.second.sw_if_index == sc.sw_if_index
                && kv.second.direction == sc.direction)
        {
            port = &kv.second;
            break;
        }
    }

    if (port && !port->vpp_programmed)
    {
        int ret = vpp_macsec_port_add_del(
                sc.sw_if_index,
                sc.direction,
                sc.encrypt,
                sc.send_sci,
                sc.replay_protect,
                sc.replay_window,
                true);

        if (ret != 0)
        {
            SWSS_LOG_ERROR("vpp_macsec_port_add_del failed ret %d sw_if %u dir %u",
                           ret, sc.sw_if_index, sc.direction);
            return SAI_STATUS_FAILURE;
        }

        port->vpp_programmed = true;
    }
    else if (!port)
    {
        int ret = vpp_macsec_port_add_del(
                sc.sw_if_index,
                sc.direction,
                sc.encrypt,
                sc.send_sci,
                sc.replay_protect,
                sc.replay_window,
                true);

        if (ret != 0)
        {
            SWSS_LOG_ERROR("vpp_macsec_port_add_del failed ret %d sw_if %u dir %u",
                           ret, sc.sw_if_index, sc.direction);
            return SAI_STATUS_FAILURE;
        }
    }

    if (!sc.vpp_programmed)
    {
        int ret = vpp_macsec_sc_add_del(
                sc.sw_if_index,
                sc.direction,
                sc.sci,
                sc.cipher_suite,
                sc.send_sci,
                sc.conf_offset,
                true);

        if (ret != 0)
        {
            SWSS_LOG_ERROR("vpp_macsec_sc_add_del failed ret %d sw_if %u dir %u sci 0x%llx",
                           ret, sc.sw_if_index, sc.direction,
                           (unsigned long long)sc.sci);
            return SAI_STATUS_FAILURE;
        }

        sc.vpp_programmed = true;
    }

    return SAI_STATUS_SUCCESS;
}

sai_status_t SwitchVpp::createMACsecPort(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (!sonicExtFeatureEnabled("macsec"))
    {
        SWSS_LOG_ERROR("sonic-ext macsec feature is disabled");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    auto dir_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_PORT_ATTR_MACSEC_DIRECTION, attr_count, attr_list);
    auto port_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_PORT_ATTR_PORT_ID, attr_count, attr_list);

    if (dir_attr == nullptr || port_attr == nullptr)
    {
        SWSS_LOG_ERROR("MACsec port create missing DIRECTION or PORT_ID");
        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    VppMacsecPortState state;
    state.direction = static_cast<uint8_t>(dir_attr->value.s32);
    state.port_id = port_attr->value.oid;
    state.vpp_programmed = false;

    CHECK_STATUS(resolveMacsecSwIfIndex(state.port_id, state.sw_if_index));

    m_vppMacsecPorts[object_id] = state;

    SWSS_LOG_NOTICE("cached MACsec port %s sw_if %u dir %u (VPP program deferred to SC)",
                    sai_serialize_object_id(object_id).c_str(),
                    state.sw_if_index, state.direction);

    auto sid = sai_serialize_object_id(object_id);
    sai_status_t status = create_internal(
            SAI_OBJECT_TYPE_MACSEC_PORT, sid, switch_id, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        m_vppMacsecPorts.erase(object_id);
    }

    return status;
}

sai_status_t SwitchVpp::createMACsecFlow(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    auto dir_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_FLOW_ATTR_MACSEC_DIRECTION, attr_count, attr_list);

    if (dir_attr == nullptr)
    {
        SWSS_LOG_ERROR("MACsec FLOW create missing DIRECTION");
        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    const uint8_t dir = static_cast<uint8_t>(dir_attr->value.s32);

    /* Bind only when exactly one unmatched PORT of this direction exists.
     * Several unmatched: leave unbound; SC create uses findPortByMACsecFlow. */
    VppMacsecPortState *unique_port = nullptr;
    sai_object_id_t unique_port_oid = SAI_NULL_OBJECT_ID;
    size_t n_unmatched = 0;

    for (auto &kv : m_vppMacsecPorts)
    {
        if (kv.second.direction == dir
                && kv.second.flow_id == SAI_NULL_OBJECT_ID)
        {
            n_unmatched++;
            unique_port = &kv.second;
            unique_port_oid = kv.first;
        }
    }

    if (n_unmatched == 0)
    {
        SWSS_LOG_ERROR("MACsec FLOW %s dir %u has no unmatched PORT",
                       sai_serialize_object_id(object_id).c_str(), dir);
        return SAI_STATUS_FAILURE;
    }

    VppMacsecFlowState flow;
    flow.direction = dir;
    flow.sw_if_index = ~0u;
    flow.port_id = SAI_NULL_OBJECT_ID;

    if (n_unmatched == 1)
    {
        unique_port->flow_id = object_id;
        flow.sw_if_index = unique_port->sw_if_index;
        flow.port_id = unique_port->port_id;
        SWSS_LOG_NOTICE("bound MACsec FLOW %s to port %s sw_if %u dir %u",
                        sai_serialize_object_id(object_id).c_str(),
                        sai_serialize_object_id(unique_port_oid).c_str(),
                        flow.sw_if_index, dir);
    }
    else
    {
        SWSS_LOG_NOTICE("MACsec FLOW %s dir %u not bound yet (%zu unmatched PORTs)",
                        sai_serialize_object_id(object_id).c_str(), dir, n_unmatched);
    }

    m_vppMacsecFlows[object_id] = flow;

    auto sid = sai_serialize_object_id(object_id);
    sai_status_t status = create_internal(
            SAI_OBJECT_TYPE_MACSEC_FLOW, sid, switch_id, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        if (n_unmatched == 1)
        {
            unique_port->flow_id = SAI_NULL_OBJECT_ID;
        }

        m_vppMacsecFlows.erase(object_id);
    }

    return status;
}

sai_status_t SwitchVpp::createMACsecSC(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (!sonicExtFeatureEnabled("macsec"))
    {
        SWSS_LOG_ERROR("sonic-ext macsec feature is disabled");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    auto dir_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SC_ATTR_MACSEC_DIRECTION, attr_count, attr_list);
    auto flow_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SC_ATTR_FLOW_ID, attr_count, attr_list);
    auto sci_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SC_ATTR_MACSEC_SCI, attr_count, attr_list);
    auto cipher_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SC_ATTR_MACSEC_CIPHER_SUITE, attr_count, attr_list);

    if (dir_attr == nullptr || flow_attr == nullptr
            || sci_attr == nullptr || cipher_attr == nullptr)
    {
        SWSS_LOG_ERROR("MACsec SC create missing mandatory attribute");
        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    VppMacsecScState sc;
    sc.direction = static_cast<uint8_t>(dir_attr->value.s32);
    sc.flow_id = flow_attr->value.oid;
    sc.sci = sci_attr->value.u64;
    sc.cipher_suite = static_cast<uint8_t>(cipher_attr->value.s32);

    auto enc_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SC_ATTR_ENCRYPTION_ENABLE, attr_count, attr_list);
    sc.encrypt = (enc_attr == nullptr || enc_attr->value.booldata) ? 1 : 0;

    auto send_sci_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SC_ATTR_MACSEC_EXPLICIT_SCI_ENABLE, attr_count, attr_list);
    sc.send_sci = (send_sci_attr && send_sci_attr->value.booldata) ? 1 : 0;

    auto replay_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SC_ATTR_MACSEC_REPLAY_PROTECTION_ENABLE, attr_count, attr_list);
    auto window_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SC_ATTR_MACSEC_REPLAY_PROTECTION_WINDOW, attr_count, attr_list);

    /*
     * Omitted replay attrs mean off (orchagent does not send them; SAI
     * default is on). Honor the attrs when they are present.
     */
    sc.replay_protect = (replay_attr && replay_attr->value.booldata) ? 1 : 0;
    sc.replay_window = window_attr ? window_attr->value.u32 : 0;

    auto offset_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SC_ATTR_CONFIDENTIALITY_OFFSET, attr_count, attr_list);
    sc.conf_offset = offset_attr ? static_cast<uint8_t>(offset_attr->value.u32) : 0;

    auto macsec_port_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SC_ATTR_MACSEC_PORT_ID, attr_count, attr_list);

    if (macsec_port_attr && macsec_port_attr->value.oid != SAI_NULL_OBJECT_ID)
    {
        auto it = m_vppMacsecPorts.find(macsec_port_attr->value.oid);

        if (it != m_vppMacsecPorts.end())
        {
            sc.sw_if_index = it->second.sw_if_index;
        }
    }

    if (sc.sw_if_index == ~0u)
    {
        bindMacsecScToPort(sc);
    }

    if (sc.sw_if_index != ~0u)
    {
        CHECK_STATUS(programVppMacsecPortAndSc(sc));
    }
    else
    {
        SWSS_LOG_ERROR("MACsec SC %s port not resolved; VPP program deferred to SA create",
                       sai_serialize_object_id(object_id).c_str());
    }

    m_vppMacsecScs[object_id] = sc;

    auto sid = sai_serialize_object_id(object_id);
    sai_status_t status = create_internal(
            SAI_OBJECT_TYPE_MACSEC_SC, sid, switch_id, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        if (sc.vpp_programmed)
        {
            int ret = vpp_macsec_sc_add_del(
                    sc.sw_if_index,
                    sc.direction,
                    sc.sci,
                    sc.cipher_suite,
                    sc.send_sci,
                    sc.conf_offset,
                    false);

            if (ret != 0)
            {
                SWSS_LOG_WARN("vpp_macsec_sc_add_del(del) rollback failed ret %d sw_if %u sci 0x%llx",
                              ret, sc.sw_if_index, (unsigned long long)sc.sci);
            }
        }

        m_vppMacsecScs.erase(object_id);
    }

    return status;
}

void SwitchVpp::destroyVppMacsecSa(
        _In_ const VppMacsecSaState &sa)
{
    SWSS_LOG_ENTER();

    uint8_t sak[32];
    uint8_t salt[12];
    memset(sak, 0, sizeof(sak));
    memset(salt, 0, sizeof(salt));

    int ret = vpp_macsec_sa_add_del(
            sa.sw_if_index,
            sa.direction,
            sa.sci,
            sa.an,
            sak,
            0,
            salt,
            0,
            0,
            0,
            false,
            nullptr);

    if (ret != 0)
    {
        SWSS_LOG_WARN("vpp_macsec_sa_add_del(del) failed ret %d sw_if %u dir %u an %u",
                      ret, sa.sw_if_index, sa.direction, sa.an);
    }
}

void SwitchVpp::retireVppMacsecEgressSa(
        _In_ const VppMacsecSaState &sa)
{
    SWSS_LOG_ENTER();

    auto key = std::make_tuple(sa.sw_if_index, sa.direction, sa.sci);
    auto it = m_vppMacsecRetiredEgressSas.find(key);

    if (it != m_vppMacsecRetiredEgressSas.end())
    {
        if (it->second.an != sa.an)
        {
            destroyVppMacsecSa(it->second);
        }
        else
        {
            return;
        }
    }

    m_vppMacsecRetiredEgressSas[key] = sa;
    SWSS_LOG_NOTICE("retired egress MACsec SA sw_if %u sci 0x%llx an %u pending replacement",
                    sa.sw_if_index, (unsigned long long)sa.sci, sa.an);
}

void SwitchVpp::commitRetiredVppMacsecEgressSa(
        _In_ const VppMacsecSaState &new_sa)
{
    SWSS_LOG_ENTER();

    auto key = std::make_tuple(new_sa.sw_if_index, new_sa.direction, new_sa.sci);
    auto it = m_vppMacsecRetiredEgressSas.find(key);

    if (it == m_vppMacsecRetiredEgressSas.end())
    {
        return;
    }

    if (it->second.an != new_sa.an)
    {
        destroyVppMacsecSa(it->second);
    }

    m_vppMacsecRetiredEgressSas.erase(it);
}

void SwitchVpp::dropRetiredVppMacsecEgressSa(
        _In_ uint32_t sw_if_index,
        _In_ uint8_t direction,
        _In_ uint64_t sci)
{
    SWSS_LOG_ENTER();

    if (sci != 0)
    {
        m_vppMacsecRetiredEgressSas.erase(std::make_tuple(sw_if_index, direction, sci));
        return;
    }

    for (auto it = m_vppMacsecRetiredEgressSas.begin(); it != m_vppMacsecRetiredEgressSas.end(); )
    {
        if (std::get<0>(it->first) == sw_if_index && std::get<1>(it->first) == direction)
        {
            it = m_vppMacsecRetiredEgressSas.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

sai_status_t SwitchVpp::createMACsecSA(
        _In_ sai_object_id_t object_id,
        _In_ sai_object_id_t switch_id,
        _In_ uint32_t attr_count,
        _In_ const sai_attribute_t *attr_list)
{
    SWSS_LOG_ENTER();

    if (!sonicExtFeatureEnabled("macsec"))
    {
        SWSS_LOG_ERROR("sonic-ext macsec feature is disabled");
        return SAI_STATUS_NOT_SUPPORTED;
    }

    auto dir_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SA_ATTR_MACSEC_DIRECTION, attr_count, attr_list);
    auto sc_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SA_ATTR_SC_ID, attr_count, attr_list);
    auto an_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SA_ATTR_AN, attr_count, attr_list);
    auto sak_attr = sai_metadata_get_attr_by_id(
            SAI_MACSEC_SA_ATTR_SAK, attr_count, attr_list);

    if (dir_attr == nullptr || sc_attr == nullptr
            || an_attr == nullptr || sak_attr == nullptr)
    {
        SWSS_LOG_ERROR("MACsec SA create missing mandatory attribute");
        return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
    }

    auto sc_it = m_vppMacsecScs.find(sc_attr->value.oid);

    if (sc_it == m_vppMacsecScs.end())
    {
        SWSS_LOG_ERROR("MACsec SA %s references unknown SC %s",
                       sai_serialize_object_id(object_id).c_str(),
                       sai_serialize_object_id(sc_attr->value.oid).c_str());
        return SAI_STATUS_INVALID_OBJECT_ID;
    }

    VppMacsecScState &sc = sc_it->second;

    if (sc.sw_if_index == ~0u)
    {
        if (bindMacsecScToPort(sc) != SAI_STATUS_SUCCESS)
        {
            SWSS_LOG_ERROR("cannot resolve port for MACsec SC %s",
                           sai_serialize_object_id(sc_attr->value.oid).c_str());
            return SAI_STATUS_FAILURE;
        }
    }

    CHECK_STATUS(programVppMacsecPortAndSc(sc));

    struct KeyWipe
    {
        uint8_t sak[32];
        uint8_t salt[12];
        KeyWipe()
        {
            memset(sak, 0, sizeof(sak));
            memset(salt, 0, sizeof(salt));
        }
        ~KeyWipe()
        {
            memset(sak, 0, sizeof(sak));
            memset(salt, 0, sizeof(salt));
        }
    } keys;

    uint8_t sak_len = 0;
    fillSak(sc.cipher_suite, sak_attr->value.macsecsak, keys.sak, sak_len);

    uint32_t ssci = 0;
    uint8_t xpn_en = 0;

    if (isXpnCipher(sc.cipher_suite))
    {
        auto salt_attr = sai_metadata_get_attr_by_id(
                SAI_MACSEC_SA_ATTR_SALT, attr_count, attr_list);
        auto ssci_attr = sai_metadata_get_attr_by_id(
                SAI_MACSEC_SA_ATTR_MACSEC_SSCI, attr_count, attr_list);

        if (salt_attr == nullptr || ssci_attr == nullptr)
        {
            SWSS_LOG_ERROR("XPN MACsec SA missing SALT or SSCI");
            return SAI_STATUS_MANDATORY_ATTRIBUTE_MISSING;
        }

        memcpy(keys.salt, salt_attr->value.macsecsalt, sizeof(keys.salt));
        ssci = ssci_attr->value.u32;
        xpn_en = 1;
    }

    uint64_t init_xpn = 0;

    if (dir_attr->value.s32 == SAI_MACSEC_DIRECTION_EGRESS)
    {
        auto xpn_attr = sai_metadata_get_attr_by_id(
                SAI_MACSEC_SA_ATTR_CONFIGURED_EGRESS_XPN, attr_count, attr_list);
        init_xpn = xpn_attr ? xpn_attr->value.u64 : 0;
    }
    else
    {
        auto xpn_attr = sai_metadata_get_attr_by_id(
                SAI_MACSEC_SA_ATTR_MINIMUM_INGRESS_XPN, attr_count, attr_list);
        init_xpn = xpn_attr ? xpn_attr->value.u64 : 1;
    }

    if (init_xpn == 0)
    {
        init_xpn = 1;
    }

    if (sc.direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        auto key = std::make_tuple(sc.sw_if_index, sc.direction, sc.sci);
        auto rit = m_vppMacsecRetiredEgressSas.find(key);

        if (rit != m_vppMacsecRetiredEgressSas.end() && rit->second.an == an_attr->value.u8)
        {
            destroyVppMacsecSa(rit->second);
            m_vppMacsecRetiredEgressSas.erase(rit);
        }
    }

    uint32_t sa_index = ~0u;
    int ret = vpp_macsec_sa_add_del(
            sc.sw_if_index,
            sc.direction,
            sc.sci,
            an_attr->value.u8,
            keys.sak,
            sak_len,
            keys.salt,
            ssci,
            xpn_en,
            init_xpn,
            true,
            &sa_index);

    if (ret != 0)
    {
        SWSS_LOG_ERROR("vpp_macsec_sa_add_del failed ret %d sw_if %u dir %u an %u sak_len %u xpn_en %u",
                       ret, sc.sw_if_index, sc.direction, an_attr->value.u8, sak_len, xpn_en);
        return SAI_STATUS_FAILURE;
    }

    if (sc.direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        ret = vpp_macsec_sc_set_encoding_an(sc.sw_if_index, sc.sci, an_attr->value.u8);

        if (ret != 0)
        {
            SWSS_LOG_ERROR("vpp_macsec_sc_set_encoding_an failed ret %d sw_if %u sci 0x%llx an %u",
                           ret, sc.sw_if_index, (unsigned long long)sc.sci, an_attr->value.u8);
            memset(keys.sak, 0, sizeof(keys.sak));
            memset(keys.salt, 0, sizeof(keys.salt));
            (void)vpp_macsec_sa_add_del(
                    sc.sw_if_index,
                    sc.direction,
                    sc.sci,
                    an_attr->value.u8,
                    keys.sak,
                    0,
                    keys.salt,
                    0,
                    0,
                    0,
                    false,
                    nullptr);
            return SAI_STATUS_FAILURE;
        }
    }

    VppMacsecSaState sa;
    sa.sw_if_index = sc.sw_if_index;
    sa.direction = sc.direction;
    sa.sci = sc.sci;
    sa.an = an_attr->value.u8;
    sa.sa_index = sa_index;
    m_vppMacsecSas[object_id] = sa;

    if (sc.direction == SAI_MACSEC_DIRECTION_EGRESS)
    {
        commitRetiredVppMacsecEgressSa(sa);
    }

    SWSS_LOG_NOTICE("programmed MACsec SA %s sw_if %u dir %u an %u sak_len %u xpn_en %u",
                    sai_serialize_object_id(object_id).c_str(),
                    sa.sw_if_index, sa.direction, sa.an, sak_len, xpn_en);

    auto sid = sai_serialize_object_id(object_id);
    sai_status_t status = create_internal(SAI_OBJECT_TYPE_MACSEC_SA, sid, switch_id, attr_count, attr_list);

    if (status != SAI_STATUS_SUCCESS)
    {
        memset(keys.sak, 0, sizeof(keys.sak));
        memset(keys.salt, 0, sizeof(keys.salt));
        (void)vpp_macsec_sa_add_del(
                sc.sw_if_index,
                sc.direction,
                sc.sci,
                an_attr->value.u8,
                keys.sak,
                0,
                keys.salt,
                0,
                0,
                0,
                false,
                nullptr);
        m_vppMacsecSas.erase(object_id);
        return status;
    }

    sai_attribute_t xpn;
    xpn.id = SAI_MACSEC_SA_ATTR_CURRENT_XPN;
    xpn.value.u64 = init_xpn;

    return set_internal(SAI_OBJECT_TYPE_MACSEC_SA, sid, &xpn);
}

sai_status_t SwitchVpp::removeMACsecSA(
        _In_ sai_object_id_t macsec_sa_id)
{
    SWSS_LOG_ENTER();

    auto it = m_vppMacsecSas.find(macsec_sa_id);

    if (it != m_vppMacsecSas.end())
    {
        if (it->second.direction == SAI_MACSEC_DIRECTION_EGRESS)
        {
            retireVppMacsecEgressSa(it->second);
        }
        else
        {
            destroyVppMacsecSa(it->second);
        }

        m_vppMacsecSas.erase(it);
    }

    auto sid = sai_serialize_object_id(macsec_sa_id);
    return remove_internal(SAI_OBJECT_TYPE_MACSEC_SA, sid);
}

sai_status_t SwitchVpp::removeMACsecSC(
        _In_ sai_object_id_t macsec_sc_id)
{
    SWSS_LOG_ENTER();

    auto it = m_vppMacsecScs.find(macsec_sc_id);

    if (it != m_vppMacsecScs.end())
    {
        dropRetiredVppMacsecEgressSa(it->second.sw_if_index, it->second.direction, it->second.sci);

        if (it->second.vpp_programmed)
        {
            int ret = vpp_macsec_sc_add_del(
                    it->second.sw_if_index,
                    it->second.direction,
                    it->second.sci,
                    it->second.cipher_suite,
                    it->second.send_sci,
                    it->second.conf_offset,
                    false);

            if (ret != 0)
            {
                SWSS_LOG_WARN("vpp_macsec_sc_add_del(del) failed ret %d sw_if %u sci 0x%llx",
                              ret, it->second.sw_if_index,
                              (unsigned long long)it->second.sci);
            }
        }

        for (auto sa_it = m_vppMacsecSas.begin(); sa_it != m_vppMacsecSas.end(); )
        {
            if (sa_it->second.sw_if_index == it->second.sw_if_index
                    && sa_it->second.sci == it->second.sci
                    && sa_it->second.direction == it->second.direction)
            {
                sa_it = m_vppMacsecSas.erase(sa_it);
            }
            else
            {
                ++sa_it;
            }
        }

        m_vppMacsecScs.erase(it);
    }

    auto sid = sai_serialize_object_id(macsec_sc_id);
    return remove_internal(SAI_OBJECT_TYPE_MACSEC_SC, sid);
}

sai_status_t SwitchVpp::removeMACsecFlow(
        _In_ sai_object_id_t macsec_flow_id)
{
    SWSS_LOG_ENTER();

    auto it = m_vppMacsecFlows.find(macsec_flow_id);

    if (it != m_vppMacsecFlows.end())
    {
        for (auto &kv : m_vppMacsecPorts)
        {
            if (kv.second.flow_id == macsec_flow_id)
            {
                kv.second.flow_id = SAI_NULL_OBJECT_ID;
            }
        }

        m_vppMacsecFlows.erase(it);
    }

    auto sid = sai_serialize_object_id(macsec_flow_id);
    return remove_internal(SAI_OBJECT_TYPE_MACSEC_FLOW, sid);
}

sai_status_t SwitchVpp::removeMACsecPort(
        _In_ sai_object_id_t macsec_port_id)
{
    SWSS_LOG_ENTER();

    auto it = m_vppMacsecPorts.find(macsec_port_id);

    if (it != m_vppMacsecPorts.end())
    {
        if (it->second.vpp_programmed)
        {
            int ret = vpp_macsec_port_add_del(
                    it->second.sw_if_index,
                    it->second.direction,
                    0, 0, 0, 0,
                    false);

            if (ret != 0)
            {
                SWSS_LOG_WARN("vpp_macsec_port_add_del(del) failed ret %d sw_if %u dir %u",
                              ret, it->second.sw_if_index, it->second.direction);
            }
        }

        if (it->second.flow_id != SAI_NULL_OBJECT_ID)
        {
            m_vppMacsecFlows.erase(it->second.flow_id);
        }

        const uint32_t sw_if_index = it->second.sw_if_index;
        const uint8_t direction = it->second.direction;

        dropRetiredVppMacsecEgressSa(sw_if_index, direction, 0);

        for (auto flow_it = m_vppMacsecFlows.begin(); flow_it != m_vppMacsecFlows.end(); )
        {
            if (flow_it->second.sw_if_index == sw_if_index
                    && flow_it->second.direction == direction)
            {
                flow_it = m_vppMacsecFlows.erase(flow_it);
            }
            else
            {
                ++flow_it;
            }
        }

        for (auto sc_it = m_vppMacsecScs.begin(); sc_it != m_vppMacsecScs.end(); )
        {
            if (sc_it->second.sw_if_index == sw_if_index
                    && sc_it->second.direction == direction)
            {
                sc_it = m_vppMacsecScs.erase(sc_it);
            }
            else
            {
                ++sc_it;
            }
        }

        for (auto sa_it = m_vppMacsecSas.begin(); sa_it != m_vppMacsecSas.end(); )
        {
            if (sa_it->second.sw_if_index == sw_if_index
                    && sa_it->second.direction == direction)
            {
                sa_it = m_vppMacsecSas.erase(sa_it);
            }
            else
            {
                ++sa_it;
            }
        }

        m_vppMacsecPorts.erase(it);
    }

    auto sid = sai_serialize_object_id(macsec_port_id);
    return remove_internal(SAI_OBJECT_TYPE_MACSEC_PORT, sid);
}

sai_status_t SwitchVpp::setMACsecSA(
        _In_ sai_object_id_t macsec_sa_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (attr == nullptr)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr->id == SAI_MACSEC_SA_ATTR_CONFIGURED_EGRESS_XPN
            || attr->id == SAI_MACSEC_SA_ATTR_MINIMUM_INGRESS_XPN)
    {
        auto it = m_vppMacsecSas.find(macsec_sa_id);

        if (it == m_vppMacsecSas.end())
        {
            SWSS_LOG_ERROR("set XPN on unknown MACsec SA %s",
                           sai_serialize_object_id(macsec_sa_id).c_str());
            return SAI_STATUS_INVALID_OBJECT_ID;
        }

        int ret = vpp_macsec_sa_set_xpn(
                it->second.sw_if_index,
                it->second.direction,
                it->second.sci,
                it->second.an,
                attr->value.u64);

        if (ret != 0)
        {
            SWSS_LOG_ERROR("vpp_macsec_sa_set_xpn failed ret %d sw_if %u an %u",
                           ret, it->second.sw_if_index, it->second.an);
            return SAI_STATUS_FAILURE;
        }
    }

    auto sid = sai_serialize_object_id(macsec_sa_id);
    return set_internal(SAI_OBJECT_TYPE_MACSEC_SA, sid, attr);
}

sai_status_t SwitchVpp::setAclEntryMACsecFlowActive(
        _In_ sai_object_id_t entry_id,
        _In_ const sai_attribute_t *attr)
{
    SWSS_LOG_ENTER();

    if (attr == nullptr || entry_id == SAI_NULL_OBJECT_ID)
    {
        SWSS_LOG_ERROR("attr or entry_id is null");
        return SAI_STATUS_INVALID_PARAMETER;
    }

    if (attr->id != SAI_ACL_ENTRY_ATTR_ACTION_MACSEC_FLOW)
    {
        return SAI_STATUS_INVALID_PARAMETER;
    }

    auto sid = sai_serialize_object_id(entry_id);
    return set_internal(SAI_OBJECT_TYPE_ACL_ENTRY, sid, attr);
}
