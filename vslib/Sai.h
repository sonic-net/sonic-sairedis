#pragma once

#include "VirtualSwitchSaiInterface.h"
#include "LaneMapContainer.h"
#include "EventQueue.h"
#include "EventPayloadNotification.h"
#include "ResourceLimiterContainer.h"
#include "CorePortIndexMapContainer.h"
#include "Context.h"

#include "meta/Meta.h"

#include "swss/selectableevent.h"
#include "swss/dbconnector.h"
#include "swss/notificationconsumer.h"

#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace saivs
{
    class Sai:
        public sairedis::SaiInterface
    {
        public:

            Sai();

            virtual ~Sai();

        public:

            sai_status_t apiInitialize(
                    _In_ uint64_t flags,
                    _In_ const sai_service_method_table_t *service_method_table) override;

            sai_status_t apiUninitialize(void) override;

        public: // SAI interface overrides

            virtual sai_status_t create(
                    _In_ sai_object_type_t objectType,
                    _Out_ sai_object_id_t* objectId,
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list) override;

            virtual sai_status_t remove(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId) override;

            virtual sai_status_t set(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t *attr) override;

            virtual sai_status_t get(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list) override;

        public: // QUAD ENTRY and BULK QUAD ENTRY

            SAIREDIS_DECLARE_EVERY_ENTRY(SAIREDIS_SAIINTERFACE_DECLARE_QUAD_ENTRY_OVERRIDE);
            SAIREDIS_DECLARE_EVERY_BULK_ENTRY(SAIREDIS_SAIINTERFACE_DECLARE_BULK_ENTRY_OVERRIDE);

        public: // bulk QUAD oid

            virtual sai_status_t bulkCreate(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t switch_id,
                    _In_ uint32_t object_count,
                    _In_ const uint32_t *attr_count,
                    _In_ const sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_object_id_t *object_id,
                    _Out_ sai_status_t *object_statuses) override;

            virtual sai_status_t bulkRemove(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t *object_id,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) override;

            virtual sai_status_t bulkSet(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t *object_id,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) override;

            virtual sai_status_t bulkGet(
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_id_t *object_id,
                    _In_ const uint32_t *attr_count,
                    _Inout_ sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses) override;

        public: // stats API

            virtual sai_status_t getStats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _Out_ uint64_t *counters) override;

            virtual sai_status_t queryStatsCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _Inout_ sai_stat_capability_list_t *stats_capability) override;

            virtual sai_status_t queryStatsStCapability(
                _In_ sai_object_id_t switch_id,
                _In_ sai_object_type_t object_type,
                _Inout_ sai_stat_st_capability_list_t *stats_capability) override;

            virtual sai_status_t getStatsExt(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _In_ sai_stats_mode_t mode,
                    _Out_ uint64_t *counters) override;

            virtual sai_status_t clearStats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids) override;

            virtual sai_status_t bulkGetStats(
                    _In_ sai_object_id_t switchId,
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_key_t *object_key,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _In_ sai_stats_mode_t mode,
                    _Inout_ sai_status_t *object_statuses,
                    _Out_ uint64_t *counters) override;

            virtual sai_status_t bulkClearStats(
                    _In_ sai_object_id_t switchId,
                    _In_ sai_object_type_t object_type,
                    _In_ uint32_t object_count,
                    _In_ const sai_object_key_t *object_key,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _In_ sai_stats_mode_t mode,
                    _Inout_ sai_status_t *object_statuses) override;

        public: // non QUAD API

            virtual sai_status_t flushFdbEntries(
                    _In_ sai_object_id_t switchId,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t *attrList) override;

        public: // SAI API

            virtual sai_status_t objectTypeGetAvailability(
                    _In_ sai_object_id_t switchId,
                    _In_ sai_object_type_t objectType,
                    _In_ uint32_t attrCount,
                    _In_ const sai_attribute_t *attrList,
                    _Out_ uint64_t *count) override;

            virtual sai_status_t queryAttributeCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id,
                    _Out_ sai_attr_capability_t *capability) override;

            virtual sai_status_t queryAttributeEnumValuesCapability(
                    _In_ sai_object_id_t switch_id,
                    _In_ sai_object_type_t object_type,
                    _In_ sai_attr_id_t attr_id,
                    _Inout_ sai_s32_list_t *enum_values_capability) override;

            virtual sai_object_type_t objectTypeQuery(
                    _In_ sai_object_id_t objectId) override;

            virtual sai_object_id_t switchIdQuery(
                    _In_ sai_object_id_t objectId) override;

            virtual sai_status_t logSet(
                    _In_ sai_api_t api,
                    _In_ sai_log_level_t log_level) override;

            virtual sai_status_t queryApiVersion(
                    _Out_ sai_api_version_t *version) override;

        private: // QUAD pre

            sai_status_t preSet(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t *attr);

            sai_status_t preSetPort(
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t *attr);

        protected: // unittests

            void startUnittestThread();

            void stopUnittestThread();

            void unittestChannelThreadProc();

            void channelOpEnableUnittest(
                    _In_ const std::string &key,
                    _In_ const std::vector<swss::FieldValueTuple> &values);

            void channelOpSetReadOnlyAttribute(
                    _In_ const std::string &key,
                    _In_ const std::vector<swss::FieldValueTuple> &values);

            void channelOpSetStats(
                    _In_ const std::string &key,
                    _In_ const std::vector<swss::FieldValueTuple> &values);

            void handleUnittestChannelOp(
                    _In_ const std::string &op,
                    _In_ const std::string &key,
                    _In_ const std::vector<swss::FieldValueTuple> &values);

        private: // FDB aging

            void processFdbEntriesForAging();

            void fdbAgingThreadProc();

            void startFdbAgingThread();

            void stopFdbAgingThread();

        private: // event queue

            void startEventQueueThread();

            void stopEventQueueThread();

            void eventQueueThreadProc();

            void processQueueEvent(
                    _In_ std::shared_ptr<Event> event);

            void syncProcessEventPacket(
                    _In_ std::shared_ptr<EventPayloadPacket> payload);

            void syncProcessEventNetLinkMsg(
                    _In_ std::shared_ptr<EventPayloadNetLinkMsg> payload);

            void asyncProcessEventNotification(
                    _In_ std::shared_ptr<EventPayloadNotification> payload);

        private: // unittests

            bool m_unittestChannelRun;

            std::shared_ptr<swss::SelectableEvent> m_unittestChannelThreadEvent;

            std::shared_ptr<std::thread> m_unittestChannelThread;

            std::shared_ptr<swss::NotificationConsumer> m_unittestChannelNotificationConsumer;

            std::shared_ptr<swss::DBConnector> m_dbNtf;

        private: // FDB aging thread

            bool m_fdbAgingThreadRun;

            std::shared_ptr<swss::SelectableEvent> m_fdbAgingThreadEvent;

            std::shared_ptr<std::thread> m_fdbAgingThread;

        private: // event queue

            bool m_eventQueueThreadRun;

            std::shared_ptr<std::thread> m_eventQueueThread;

            std::shared_ptr<EventQueue> m_eventQueue;

            std::shared_ptr<Signal> m_signal;

        private:

            std::shared_ptr<Context> getContext(
                    _In_ uint32_t globalContext) const;

        private:

            bool m_apiInitialized;

            std::recursive_mutex m_apimutex;

            std::shared_ptr<saimeta::Meta> m_meta;

            std::shared_ptr<VirtualSwitchSaiInterface> m_vsSai;

            sai_service_method_table_t m_service_method_table;

            const char *m_warm_boot_read_file;

            const char *m_warm_boot_write_file;

            std::shared_ptr<LaneMapContainer> m_laneMapContainer;

            std::shared_ptr<LaneMapContainer> m_fabricLaneMapContainer;

            std::shared_ptr<ResourceLimiterContainer> m_resourceLimiterContainer;

            std::shared_ptr<CorePortIndexMapContainer> m_corePortIndexMapContainer;

            uint32_t m_globalContext;

            std::map<uint32_t, std::shared_ptr<Context>> m_contextMap;
    };
}
