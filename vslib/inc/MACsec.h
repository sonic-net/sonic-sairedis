#pragma once

#include "TrafficFilter.h"
#include "HostInterfaceInfo.h"

#include <swss/selectableevent.h>
#include <saitypes.h>

#include <string>
#include <thread>
#include <cinttypes>

namespace saivs
{

using macsec_sci_t = std::string;
using macsec_an_t = std::uint16_t;
using macsec_pn_t = std::uint64_t;

struct MACsecAttr
{
    std::string m_veth_name;
    std::string m_macsec_name;
    std::string m_sak;
    std::string m_sci;
    macsec_an_t m_an;
    macsec_pn_t m_pn;
    bool m_encryption_enable;
    sai_int32_t m_direction;
    std::shared_ptr<HostInterfaceInfo> m_info;

    ~MACsecAttr()
    {};
};

class MACsecForwarder
{
 public:
    MACsecForwarder(
        const std::string &macsec_interface_name,
        int tapfd,
        int vethfd);

    virtual ~MACsecForwarder();

    int get_macsecfd() const;

    void forward();
 private:
    int m_tapfd;
    int m_vethfd;
    int m_macsecfd;
    const std::string m_macsec_interface_name;
    bool m_run_thread;
    swss::SelectableEvent m_exit_event;
    std::shared_ptr<std::thread> m_forward_thread;
};

class MACsecFilter
    : public TrafficFilter
{
 public:
    MACsecFilter(
        const std::string &macsec_interface_name,
        int macsecfd);

    virtual ~MACsecFilter() = default;

    FilterStatus execute(
        void *buffer,
        ssize_t &length) override;

 protected:
    int m_macsecfd;
    const std::string m_macsec_interface_name;
    virtual FilterStatus forward(
        void *buffer,
        ssize_t &length) = 0;
};

class MACsecEgressFilter
    : public MACsecFilter
{
 public:
    MACsecEgressFilter(
        const std::string &macsec_interface_name,
        int macsecfd);

 protected:
    FilterStatus forward(
        void *buffer,
        ssize_t &length) override;
};

class MACsecIngressFilter
    : public MACsecFilter
{
 public:
    MACsecIngressFilter(
        const std::string &macsec_interface_name,
        int macsecfd);

 protected:
    FilterStatus forward(
        void *buffer,
        ssize_t &length) override;
};

class MACsecManager
{
 public:
    MACsecManager() = default;
    ~MACsecManager() = default;

    bool create_macsec_port(const MACsecAttr &attr);
    bool create_macsec_egress_sa(const MACsecAttr &attr);
    bool create_macsec_ingress_sc(const MACsecAttr &attr);
    bool create_macsec_ingress_sa(const MACsecAttr &attr);
    bool enable_macsec(const MACsecAttr &attr);
    bool delete_macsec_port(const MACsecAttr &attr);
    bool delete_macsec_egress_sa(const MACsecAttr &attr);
    bool delete_macsec_ingress_sc(const MACsecAttr &attr);
    bool delete_macsec_ingress_sa(const MACsecAttr &attr);

 private:

    bool add_macsec_manager(
        const std::string &macsec_interface,
        std::shared_ptr<HostInterfaceInfo> info);

    bool delete_macsec_manager(const std::string &macsec_interface);

    struct MACsecTrafficManager
    {
        std::shared_ptr<HostInterfaceInfo> m_info;
        std::shared_ptr<MACsecFilter> m_ingress_filter;
        std::shared_ptr<MACsecFilter> m_egress_filter;
        std::shared_ptr<MACsecForwarder> m_forwarder;

        MACsecTrafficManager()
        {};
        ~MACsecTrafficManager()
        {};
    };
    std::map<std::string, MACsecTrafficManager> m_managers;

    std::string shellquote(const std::string &str);
    bool exec(const std::string &command);

};

}  // namespace saivs
