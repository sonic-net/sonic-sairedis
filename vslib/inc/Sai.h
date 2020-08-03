#pragma once

#include "VirtualSwitchSaiInterface.h"
#include "LaneMapContainer.h"
#include "EventQueue.h"
#include "EventPayloadNotification.h"
#include "ResourceLimiterContainer.h"

#include "meta/Meta.h"

#include "swss/selectableevent.h"
#include "swss/dbconnector.h"
#include "swss/notificationconsumer.h"

#include <string>
#include <vector>
#include <memory>
#include <mutex>

#define SAIVS_SAI_DECLARE_REMOVE_ENTRY(ot)                          \
    virtual sai_status_t remove(                                    \
            _In_ const sai_ ## ot ## _t* ot) override;

#define SAIVS_SAI_DECLARE_CREATE_ENTRY(ot)                          \
    virtual sai_status_t create(                                    \
            _In_ const sai_ ## ot ## _t* ot,                        \
            _In_ uint32_t attr_count,                               \
            _In_ const sai_attribute_t *attr_list) override;

#define SAIVS_SAI_DECLARE_SET_ENTRY(ot)                             \
    virtual sai_status_t set(                                       \
            _In_ const sai_ ## ot ## _t* ot,                        \
            _In_ const sai_attribute_t *attr) override;

#define SAIVS_SAI_DECLARE_GET_ENTRY(ot)                             \
    virtual sai_status_t get(                                       \
            _In_ const sai_ ## ot ## _t* ot,                        \
            _In_ uint32_t attr_count,                               \
            _Out_ sai_attribute_t *attr_list) override;

#define SAIVS_SAI_DECLARE_BULK_CREATE_ENTRY(ot)                     \
    virtual sai_status_t bulkCreate(                                \
            _In_ uint32_t object_count,                             \
            _In_ const sai_ ## ot ## _t *ot,                        \
            _In_ const uint32_t *attr_count,                        \
            _In_ const sai_attribute_t **attr_list,                 \
            _In_ sai_bulk_op_error_mode_t mode,                     \
            _Out_ sai_status_t *object_statuses) override;

#define SAIVS_SAI_DECLARE_BULK_REMOVE_ENTRY(ot)                     \
    virtual sai_status_t bulkRemove(                                \
            _In_ uint32_t object_count,                             \
            _In_ const sai_ ## ot ## _t *ot,                        \
            _In_ sai_bulk_op_error_mode_t mode,                     \
            _Out_ sai_status_t *object_statuses) override;

#define SAIVS_SAI_DECLARE_BULK_SET_ENTRY(ot)                        \
    virtual sai_status_t bulkSet(                                   \
            _In_ uint32_t object_count,                             \
            _In_ const sai_ ## ot ## _t *ot,                        \
            _In_ const sai_attribute_t *attr_list,                  \
            _In_ sai_bulk_op_error_mode_t mode,                     \
            _Out_ sai_status_t *object_statuses) override;

namespace saivs
{
    class Sai:
        public sairedis::SaiInterface
    {
        public:

            Sai();

            virtual ~Sai();

        public:

            sai_status_t initialize(
                    _In_ uint64_t flags,
                    _In_ const sai_service_method_table_t *service_method_table) override;

            sai_status_t uninitialize(void) override;

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

        public: // create ENTRY

            SAIVS_SAI_DECLARE_CREATE_ENTRY(fdb_entry);
            SAIVS_SAI_DECLARE_CREATE_ENTRY(inseg_entry);
            SAIVS_SAI_DECLARE_CREATE_ENTRY(ipmc_entry);
            SAIVS_SAI_DECLARE_CREATE_ENTRY(l2mc_entry);
            SAIVS_SAI_DECLARE_CREATE_ENTRY(mcast_fdb_entry);
            SAIVS_SAI_DECLARE_CREATE_ENTRY(neighbor_entry);
            SAIVS_SAI_DECLARE_CREATE_ENTRY(route_entry);
            SAIVS_SAI_DECLARE_CREATE_ENTRY(nat_entry);

        public: // remove ENTRY

            SAIVS_SAI_DECLARE_REMOVE_ENTRY(fdb_entry);
            SAIVS_SAI_DECLARE_REMOVE_ENTRY(inseg_entry);
            SAIVS_SAI_DECLARE_REMOVE_ENTRY(ipmc_entry);
            SAIVS_SAI_DECLARE_REMOVE_ENTRY(l2mc_entry);
            SAIVS_SAI_DECLARE_REMOVE_ENTRY(mcast_fdb_entry);
            SAIVS_SAI_DECLARE_REMOVE_ENTRY(neighbor_entry);
            SAIVS_SAI_DECLARE_REMOVE_ENTRY(route_entry);
            SAIVS_SAI_DECLARE_REMOVE_ENTRY(nat_entry);

        public: // set ENTRY

            SAIVS_SAI_DECLARE_SET_ENTRY(fdb_entry);
            SAIVS_SAI_DECLARE_SET_ENTRY(inseg_entry);
            SAIVS_SAI_DECLARE_SET_ENTRY(ipmc_entry);
            SAIVS_SAI_DECLARE_SET_ENTRY(l2mc_entry);
            SAIVS_SAI_DECLARE_SET_ENTRY(mcast_fdb_entry);
            SAIVS_SAI_DECLARE_SET_ENTRY(neighbor_entry);
            SAIVS_SAI_DECLARE_SET_ENTRY(route_entry);
            SAIVS_SAI_DECLARE_SET_ENTRY(nat_entry);

        public: // get ENTRY

            SAIVS_SAI_DECLARE_GET_ENTRY(fdb_entry);
            SAIVS_SAI_DECLARE_GET_ENTRY(inseg_entry);
            SAIVS_SAI_DECLARE_GET_ENTRY(ipmc_entry);
            SAIVS_SAI_DECLARE_GET_ENTRY(l2mc_entry);
            SAIVS_SAI_DECLARE_GET_ENTRY(mcast_fdb_entry);
            SAIVS_SAI_DECLARE_GET_ENTRY(neighbor_entry);
            SAIVS_SAI_DECLARE_GET_ENTRY(route_entry);
            SAIVS_SAI_DECLARE_GET_ENTRY(nat_entry);

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

        public: // bulk create ENTRY

            SAIVS_SAI_DECLARE_BULK_CREATE_ENTRY(fdb_entry);
            SAIVS_SAI_DECLARE_BULK_CREATE_ENTRY(nat_entry);
            SAIVS_SAI_DECLARE_BULK_CREATE_ENTRY(route_entry);

        public: // bulk remove ENTRY

            SAIVS_SAI_DECLARE_BULK_REMOVE_ENTRY(fdb_entry);
            SAIVS_SAI_DECLARE_BULK_REMOVE_ENTRY(nat_entry);
            SAIVS_SAI_DECLARE_BULK_REMOVE_ENTRY(route_entry);

        public: // bulk set ENTRY

            SAIVS_SAI_DECLARE_BULK_SET_ENTRY(fdb_entry);
            SAIVS_SAI_DECLARE_BULK_SET_ENTRY(nat_entry);
            SAIVS_SAI_DECLARE_BULK_SET_ENTRY(route_entry);

        public: // stats API

            virtual sai_status_t getStats(
                    _In_ sai_object_type_t object_type,
                    _In_ sai_object_id_t object_id,
                    _In_ uint32_t number_of_counters,
                    _In_ const sai_stat_id_t *counter_ids,
                    _Out_ uint64_t *counters) override;

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

            virtual sai_status_t queryAattributeEnumValuesCapability(
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

        private: // QUAD API helpers

            sai_status_t create(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& serializedObjectId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            sai_status_t remove(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& serializedObjectId);

            sai_status_t set(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& serializedObjectId,
                    _In_ const sai_attribute_t *attr);

            sai_status_t get(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& serializedObjectId,
                    _In_ uint32_t attr_count,
                    _Inout_ sai_attribute_t *attr_list);

        private: // bulk QUAD API helpers

            sai_status_t bulkCreate(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ const uint32_t *attr_count,
                    _In_ const sai_attribute_t **attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Inout_ sai_status_t *object_statuses);

            sai_status_t bulkRemove(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

            sai_status_t bulkSet(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::vector<std::string> &serialized_object_ids,
                    _In_ const sai_attribute_t *attr_list,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _Out_ sai_status_t *object_statuses);

        private: // QUAD pre

            sai_status_t preSet(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t *attr);

            sai_status_t preSetPort(
                    _In_ sai_object_id_t objectId,
                    _In_ const sai_attribute_t *attr);

        private: // unittests

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

            bool m_apiInitialized;

            std::recursive_mutex m_apimutex;

            std::shared_ptr<saimeta::Meta> m_meta;

            std::shared_ptr<VirtualSwitchSaiInterface> m_vsSai;

            sai_service_method_table_t m_service_method_table;

            const char *m_warm_boot_read_file;

            const char *m_warm_boot_write_file;

            std::shared_ptr<LaneMapContainer> m_laneMapContainer;

            std::shared_ptr<ResourceLimiterContainer> m_resourceLimiterContainer;
    };
}
