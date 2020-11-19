#pragma once

#include "swss/sal.h"
#include "swss/selectableevent.h"

#include <string>
#include <memory>
#include <thread>

namespace saivs
{
    class MACsecForwarder
    {
    public:
        MACsecForwarder(
            _In_ const std::string &macsecInterfaceName,
            _In_ int tapfd);

        virtual ~MACsecForwarder();

        int get_macsecfd() const;

        void forward();

    private:
        int m_tapfd;
        int m_macsecfd;

        const std::string m_macsec_interface_name;

        bool m_run_thread;

        swss::SelectableEvent m_exit_event;

        std::shared_ptr<std::thread> m_forward_thread;
    };
}
