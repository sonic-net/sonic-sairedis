#pragma once

#include "MACsecAttr.h"
#include "MACsecFilter.h"
#include "MACsecForwarder.h"

namespace saivs
{

    class MACsecManager
    {
    public:
        MACsecManager();
        ~MACsecManager();

        bool create_macsec_port(
            _In_ const MACsecAttr &attr);

        bool create_macsec_egress_sa(
            _In_ const MACsecAttr &attr);

        bool create_macsec_sc(
            _In_ const MACsecAttr &attr);

        bool create_macsec_ingress_sc(
            _In_ const MACsecAttr &attr);

        bool create_macsec_ingress_sa(
            _In_ const MACsecAttr &attr);

        bool create_macsec_sa(
            _In_ const MACsecAttr &attr);

        bool delete_macsec_port(
            _In_ const MACsecAttr &attr);

        bool delete_macsec_egress_sa(
            _In_ const MACsecAttr &attr);

        bool delete_macsec_ingress_sc(
            _In_ const MACsecAttr &attr);

        bool delete_macsec_ingress_sa(
            _In_ const MACsecAttr &attr);

        bool get_macsec_sa_pn(
            _In_ const MACsecAttr &attr,
            _Out_ sai_uint64_t &pn) const;

    private:

        bool add_macsec_manager(
            _In_ const std::string &macsec_interface,
            _In_ std::shared_ptr<HostInterfaceInfo> info);

        bool delete_macsec_traffic_manager(
            _In_ const std::string &macsec_interface);

        bool get_macsec_device_info(
            _In_ const std::string &macsec_device,
            _Out_ std::string &info) const;

        bool is_macsec_device_existing(
            _In_ const std::string &macsec_device) const;

        bool get_macsec_sc_info(
            _In_ const std::string &macsec_device,
            _In_ sai_int32_t direction,
            _In_ const std::string &sci,
            _Out_ std::string &info) const;

        bool is_macsec_sc_existing(
            _In_ const std::string &macsec_device,
            _In_ sai_int32_t direction,
            _In_ const std::string &sci) const;

        bool get_macsec_sa_info(
            _In_ const std::string &macsec_device,
            _In_ sai_int32_t direction,
            _In_ const std::string &sci,
            _In_ macsec_an_t an,
            _Out_ std::string &info) const;

        bool is_macsec_sa_existing(
            _In_ const std::string &macsec_device,
            _In_ sai_int32_t direction,
            _In_ const std::string &sci,
            _In_ macsec_an_t an) const;

        void cleanup_macsec_device() const;

        std::string shellquote(
            _In_ const std::string &str) const;

        bool exec(
            _In_ const std::string &command,
            _Out_ std::string &output) const;

        bool exec(
            _In_ const std::string &command) const;

        struct MACsecTrafficManager
        {
            std::shared_ptr<HostInterfaceInfo> m_info;
            std::shared_ptr<MACsecFilter> m_ingress_filter;
            std::shared_ptr<MACsecFilter> m_egress_filter;
            std::shared_ptr<MACsecForwarder> m_forwarder;

            MACsecTrafficManager() = default;

            ~MACsecTrafficManager() = default;
        };

        std::map<std::string, MACsecTrafficManager> m_macsec_traffic_managers;

    };

}  // namespace saivs
