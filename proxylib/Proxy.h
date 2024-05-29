#pragma once

#include "swss/selectableevent.h"

#include "meta/SaiInterface.h"
#include "meta/SelectableChannel.h"

#include "syncd/ServiceMethodTable.h"
#include "syncd/NotificationProducerBase.h"

#include <map>
#include <memory>
#include <thread>
#include <string>

namespace saiproxy
{
    class Proxy
    {
        public:

            Proxy(
                    _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai);

            virtual ~Proxy();

        public:

            void run();

            void stop();

        private:

            const char* profileGetValue(
                    _In_ sai_switch_profile_id_t profile_id,
                    _In_ const char* variable);

            int profileGetNextValue(
                    _In_ sai_switch_profile_id_t profile_id,
                    _Out_ const char** variable,
                    _Out_ const char** value);

            void processEvent(
                    _In_ sairedis::SelectableChannel& consumer);

            void processSingleEvent(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

        private: // api process methods

            // TODO implement bulk apis

            void processCreate(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processRemove(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processSet(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processGet(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processCreateEntry(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processFlushFdbEntries(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processObjectTypeGetAvailability(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processQueryAttributeCapability(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processQueryAttributeEnumValuesCapability(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processObjectTypeQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processSwitchIdQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processQueryApiVersion(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processLogSet(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processGetStats(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processGetStatsExt(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void processClearStats(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);
        private:

            syncd::ServiceMethodTable m_smt;

            sai_service_method_table_t m_test_services;

            std::shared_ptr<sairedis::SaiInterface> m_vendorSai;

            swss::SelectableEvent m_stopEvent;

            std::map<std::string, std::string> m_profileMap;

            std::map<std::string, std::string>::iterator m_profileIter;

            std::shared_ptr<sairedis::SelectableChannel> m_selectableChannel;

            std::shared_ptr<syncd::NotificationProducerBase> m_notifications;

            /**
             * @brief Mutex for synchronizing api execution and notifications
             */
            std::mutex m_mutex;

            bool m_apiInitialized;
    };
}
