#pragma once

extern "C" {
#include "sai.h"
}

#include "EventQueue.h"
#include "TrafficFilterPipes.h"
#include "TrafficForwarder.h"

#include "swss/selectableevent.h"

#include <memory>
#include <thread>
#include <string.h>

namespace saivs
{
    class HostInterfaceInfo :
        public TrafficForwarder
    {
        private:

            HostInterfaceInfo(const HostInterfaceInfo&) = delete;

        public:

            HostInterfaceInfo(
                    _In_ int ifindex,
                    _In_ int socket,
                    _In_ int tapfd,
                    _In_ const std::string& tapname,
                    _In_ sai_object_id_t portId,
                    _In_ std::shared_ptr<EventQueue> eventQueue);

            virtual ~HostInterfaceInfo();

        public:

            void async_process_packet_for_fdb_event(
                    _In_ const uint8_t *buffer,
                    _In_ size_t size) const;

            bool installEth2TapFilter(
                    _In_ int priority,
                    _In_ std::shared_ptr<TrafficFilter> filter);

            bool uninstallEth2TapFilter(
                    _In_ std::shared_ptr<TrafficFilter> filter);

            bool installTap2EthFilter(
                    _In_ int priority,
                    _In_ std::shared_ptr<TrafficFilter> filter);

            bool uninstallTap2EthFilter(
                    _In_ std::shared_ptr<TrafficFilter> filter);

        private:

            void veth2tap_fun();

            void tap2veth_fun();

        public: // TODO to private

            int m_ifindex;

            int m_packet_socket;

            std::string m_name;

            sai_object_id_t m_portId;

            bool m_run_thread;

            std::shared_ptr<EventQueue> m_eventQueue;

            int m_tapfd;

        private:

            std::shared_ptr<std::thread> m_e2t;
            std::shared_ptr<std::thread> m_t2e;

            TrafficFilterPipes m_e2tFilters;
            TrafficFilterPipes m_t2eFilters;

            swss::SelectableEvent m_e2tEvent;
            swss::SelectableEvent m_t2eEvent;
    };
}
