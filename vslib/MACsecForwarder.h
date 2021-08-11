#pragma once

#include "HostInterfaceInfo.h"
#include "TrafficForwarder.h"

#include "swss/sal.h"
#include "swss/selectableevent.h"

#include <string>
#include <memory>
#include <thread>

namespace saivs
{
    class MACsecForwarder :
        public TrafficForwarder
    {
        public:

            MACsecForwarder(
                    _In_ const std::string &macsecInterfaceName,
                    _In_ std::shared_ptr<HostInterfaceInfo> info);

            virtual ~MACsecForwarder();

            int get_macsecfd() const;

            void forward();

        private:

            int m_macsecfd;

            const std::string m_macsecInterfaceName;

            bool m_runThread;

            swss::SelectableEvent m_exitEvent;

            std::shared_ptr<std::thread> m_forwardThread;

            std::shared_ptr<HostInterfaceInfo> m_info;
    };
}
