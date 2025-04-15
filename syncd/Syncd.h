#pragma once

#include "CommandLineOptions.h"
#include "FlexCounterManager.h"
#include "VendorSai.h"
#include "AsicView.h"
#include "SaiSwitch.h"
#include "VirtualOidTranslator.h"
#include "RedisClient.h"
#include "NotificationHandler.h"
#include "NotificationProcessor.h"
#include "SwitchNotifications.h"
#include "ServiceMethodTable.h"
#include "RedisVidIndexGenerator.h"
#include "RequestShutdown.h"
#include "ContextConfig.h"
#include "BreakConfig.h"
#include "NotificationProducerBase.h"
#include "TimerWatchdog.h"
#include "MdioIpcServer.h"

#include "meta/SaiAttributeList.h"
#include "meta/SelectableChannel.h"

#include "swss/consumertable.h"
#include "swss/producertable.h"
#include "swss/notificationconsumer.h"

#include <memory>

namespace syncd
{
    class Syncd
    {
        private:

            Syncd(const Syncd&) = delete;
            Syncd& operator=(const Syncd&) = delete;

        public:

            Syncd(
                    _In_ std::shared_ptr<sairedis::SaiInterface> vendorSai,
                    _In_ std::shared_ptr<CommandLineOptions> cmd,
                    _In_ bool isWarmStart);

            virtual ~Syncd();

        public:

            bool getAsicInitViewMode() const;

            void setAsicInitViewMode(
                    _In_ bool enable);

            bool isInitViewMode() const;

            bool isVeryFirstRun();

            void onSyncdStart(
                    _In_ bool warmStart);

            void run();

        public: // TODO private

            void processEvent(
                    _In_ sairedis::SelectableChannel& consumer);

            sai_status_t processQuadEventInInitViewMode(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& strObjectId,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            void processFlexCounterGroupEvent(
                    _In_ swss::ConsumerTable &consumer);

            void processFlexCounterEvent(
                    _In_ swss::ConsumerTable &consumer);

            const char* profileGetValue(
                    _In_ sai_switch_profile_id_t profile_id,
                    _In_ const char* variable);

            int profileGetNextValue(
                    _In_ sai_switch_profile_id_t profile_id,
                    _Out_ const char** variable,
                    _Out_ const char** value);

            void performStartupLogic();

            void sendShutdownRequest(
                    _In_ sai_object_id_t switchVid);

            void sendShutdownRequestAfterException();

        public: // shutdown actions for all switches

            sai_status_t removeAllSwitches();

            sai_status_t setRestartWarmOnAllSwitches(
                    _In_ bool flag);

            sai_status_t setFastAPIEnableOnAllSwitches();

            sai_status_t setPreShutdownOnAllSwitches();

            sai_status_t setUninitDataPlaneOnRemovalOnAllSwitches();

        private:

            void loadProfileMap();

            void saiLoglevelNotify(
                    _In_ std::string strApi,
                    _In_ std::string strLogLevel);

            void setSaiApiLogLevel();

        private:

            sai_status_t processNotifySyncd(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processSingleEvent(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processAttrCapabilityQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processAttrEnumValuesCapabilityQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processObjectTypeGetAvailabilityQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processStatsCapabilityQuery(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processStatsStCapabilityQuery(
                _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processFdbFlush(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processClearStatsEvent(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processGetStatsEvent(
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processQuadEvent(
                    _In_ sai_common_api_t api,
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processBulkQuadEvent(
                    _In_ sai_common_api_t api,
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            sai_status_t processBulkOid(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string> &object_ids,
                    _In_ sai_common_api_t api,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>> &attributes,
                    _In_ const std::vector<std::vector<swss::FieldValueTuple>>& strAttributes);

            sai_status_t processBulkEntry(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string> &object_ids,
                    _In_ sai_common_api_t api,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>> &attributes,
                    _In_ const std::vector<std::vector<swss::FieldValueTuple>>& strAttributes);

            sai_status_t processBulkCreateEntry(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string>& objectIds,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>>& attributes,
                    _Out_ std::vector<sai_status_t>& statuses);

            sai_status_t processBulkRemoveEntry(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string>& objectIds,
                    _Out_ std::vector<sai_status_t>& statuses);

            sai_status_t processBulkSetEntry(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string>& objectIds,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>>& attributes,
                    _Out_ std::vector<sai_status_t>& statuses);

            sai_status_t processBulkQuadEventInInitViewMode(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string> &object_ids,
                    _In_ sai_common_api_t api,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>> &attributes,
                    _In_ const std::vector<std::vector<swss::FieldValueTuple>>& strAttributes);

            sai_status_t processOid(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string &strObjectId,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t processFlexCounterGroupEvent(
                    _In_ const std::string &key,
                    _In_ const std::string &op,
                    _In_ const std::vector<swss::FieldValueTuple> &values,
                    _In_ bool fromAsicChannel=true);

            sai_status_t processFlexCounterEvent(
                    _In_ const std::string &key,
                    _In_ const std::string &op,
                    _In_ const std::vector<swss::FieldValueTuple> &values,
                    _In_ bool fromAsicChannel=true);

        private: // process quad oid

            sai_status_t processOidCreate(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string &strObjectId,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t processOidRemove(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string &strObjectId);

            sai_status_t processOidSet(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string &strObjectId,
                    _In_ sai_attribute_t *attr);

            sai_status_t processOidGet(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string &strObjectId,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

        private: // process bulk oid

            sai_status_t processBulkOidCreate(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _In_ const std::vector<std::string>& objectIds,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>>& attributes,
                    _Out_ std::vector<sai_status_t>& statuses);

            sai_status_t processBulkOidSet(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _In_ const std::vector<std::string>& objectIds,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>>& attributes,
                    _Out_ std::vector<sai_status_t>& statuses);

            sai_status_t processBulkOidGet(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _In_ const std::vector<std::string>& objectIds,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>>& attributes,
                    _Out_ std::vector<sai_status_t>& statuses);

            sai_status_t processBulkOidRemove(
                    _In_ sai_object_type_t objectType,
                    _In_ sai_bulk_op_error_mode_t mode,
                    _In_ const std::vector<std::string>& objectIds,
                    _Out_ std::vector<sai_status_t>& statuses);

        private: // process quad in init view mode

            sai_status_t processQuadInInitViewModeCreate(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& strObjectId,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            sai_status_t processQuadInInitViewModeRemove(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& strObjectId);

            sai_status_t processQuadInInitViewModeSet(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& strObjectId,
                    _In_ sai_attribute_t *attr);

            sai_status_t processQuadInInitViewModeGet(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& strObjectId,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

        private:

            void syncUpdateRedisQuadEvent(
                    _In_ sai_status_t status,
                    _In_ sai_common_api_t api,
                    _In_ const swss::KeyOpFieldsValuesTuple &kco);

            void syncUpdateRedisBulkQuadEvent(
                    _In_ sai_common_api_t api,
                    _In_ const std::vector<sai_status_t>& statuses,
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string>& objectIds,
                    _In_ const std::vector<std::vector<swss::FieldValueTuple>>& strAttributes);

        public: // TODO to private

            sai_status_t processEntry(
                    _In_ sai_object_meta_key_t meta_key,
                    _In_ sai_common_api_t api,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            void syncProcessNotification(
                    _In_ const swss::KeyOpFieldsValuesTuple& item);

        private:

            void inspectAsic();

            void clearTempView();

            sai_status_t onApplyViewInFastFastBoot();

            sai_status_t applyView();

            void dumpComparisonLogicOutput(
                    _In_ const std::vector<std::shared_ptr<AsicView>>& currentViews);

            void updateRedisDatabase(
                    _In_ const std::vector<std::shared_ptr<AsicView>>& temporaryViews);

            std::map<sai_object_id_t, swss::TableDump> redisGetAsicView(
                    _In_ const std::string &tableName);

            void onSwitchCreateInInitViewMode(
                    _In_ sai_object_id_t switch_vid,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            void performWarmRestart();

            void performWarmRestartSingleSwitch(
                    _In_ const std::string& key);

            void startDiagShell(
                    _In_ sai_object_id_t switchRid);

            void diagShellThreadProc(
                    _In_ sai_object_id_t switchRid);

            syncd_restart_type_t handleRestartQuery(
                    _In_ swss::NotificationConsumer &restartQuery);

        private:

            /**
             * @brief Send api response.
             *
             * This function should be use to send response to sairedis for
             * create/remove/set API as well as their corresponding bulk versions.
             *
             * Should not be used on GET api.
             */
            void sendApiResponse(
                    _In_ sai_common_api_t api,
                    _In_ sai_status_t status,
                    _In_ uint32_t object_count = 0,
                    _In_ sai_status_t * object_statuses = NULL);

            void sendGetResponse(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& strObjectId,
                    _In_ sai_object_id_t switchVid,
                    _In_ sai_status_t status,
                    _In_ uint32_t attr_count,
                    _In_ sai_attribute_t *attr_list);

            void sendBulkGetResponse(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::vector<std::string>& strObjectIds,
                    _In_ sai_status_t status,
                    _In_ const std::vector<std::shared_ptr<saimeta::SaiAttributeList>>& attributes,
                    _In_ const std::vector<sai_status_t>& statuses);

            void sendNotifyResponse(
                    _In_ sai_status_t status);

        private: // snoop get response oids

            void snoopGetResponse(
                    _In_ sai_object_type_t object_type,
                    _In_ const std::string &strObjectId,
                    _In_ uint32_t attr_count,
                    _In_ const sai_attribute_t *attr_list);

            void snoopGetAttr(
                    _In_ sai_object_type_t objectType,
                    _In_ const std::string& strObjectId,
                    _In_ const std::string& attrId,
                    _In_ const std::string& attrValue);

            void snoopGetOid(
                    _In_ sai_object_id_t vid);

            void snoopGetOidList(
                    _In_ const sai_object_list_t& list);

            void snoopGetAttrValue(
                    _In_ const std::string& strObjectId,
                    _In_ const sai_attr_metadata_t *meta,
                    _In_ const sai_attribute_t& attr);

        private:

            std::shared_ptr<CommandLineOptions> m_commandLineOptions;

            bool m_isWarmStart;

            bool m_firstInitWasPerformed;

            SwitchNotifications m_sn;

            ServiceMethodTable m_smt;

            sai_service_method_table_t m_test_services;

        public: // TODO to private

            bool m_asicInitViewMode;

            std::shared_ptr<FlexCounterManager> m_manager;

            /**
             * @brief set of objects removed by user when we are in init view
             * mode. Those could be vlan members, bridge ports etc.
             *
             * We need this list to later on not put them back to temp view
             * mode when doing populate existing objects in apply view mode.
             *
             * Object ids here a VIDs.
             */
            std::set<sai_object_id_t> m_initViewRemovedVidSet;

            std::shared_ptr<sairedis::SaiInterface> m_vendorSai;

            /*
             * TODO: Those are hard coded values for mlnx integration for v1.0.1 they need
             * to be updated.
             *
             * Also DEVICE_MAC_ADDRESS is not present in saiswitch.h
             */
            std::map<std::string, std::string> m_profileMap;

            std::map<std::string, std::string>::iterator m_profileIter;


            /**
             * @brief Contains map of all created switches.
             *
             * This syncd implementation supports only one switch but it's
             * written in a way that could be extended to use multiple switches
             * in the future, some refactoring needs to be made in marked
             * places.
             *
             * To support multiple switches VIDTORID and RIDTOVID db entries
             * needs to be made per switch like HIDDEN and LANES. Best way is
             * to wrap vid/rid map to functions that will return right key.
             *
             * Key is switch VID.
             */
            std::map<sai_object_id_t, std::shared_ptr<syncd::SaiSwitch>> m_switches;

            bool m_veryFirstRun;

            std::shared_ptr<VirtualOidTranslator> m_translator;

            std::shared_ptr<RedisClient> m_client;

            std::shared_ptr<NotificationHandler> m_handler;

            std::shared_ptr<syncd::NotificationProcessor> m_processor;

            std::shared_ptr<sairedis::SelectableChannel> m_selectableChannel;

            std::shared_ptr<syncd::MdioIpcServer> m_mdioIpcServer;

            bool m_enableSyncMode;

        private:

            /**
             * @brief Syncd mutex for thread synchronization
             *
             * Purpose of this mutex is to synchronize multiple threads like
             * main thread, counters and notifications as well as all
             * operations which require multiple Redis DB access.
             *
             * For example: query DB for next VID id number, and then put map
             * RID and VID to Redis. From syncd point of view this entire
             * operation should be atomic and no other thread should access DB
             * or make assumption on previous information until entire
             * operation will finish.
             *
             * Mutex must be used in 4 places:
             *
             * - notification processing
             * - main event loop processing
             * - syncd hard init when switches are created
             *   (notifications could be sent during that)
             * - in case of exception when sending shutdown request
             *   (other notifications can still arrive at this point)
             *
             * * getting flex counter - here we skip using mutex
             */
            std::mutex m_mutex;

            std::shared_ptr<swss::DBConnector> m_dbAsic;

            std::shared_ptr<swss::NotificationConsumer> m_restartQuery;

            std::shared_ptr<swss::DBConnector> m_dbFlexCounter;
            std::shared_ptr<swss::ConsumerTable> m_flexCounter;
            std::shared_ptr<swss::ConsumerTable> m_flexCounterGroup;
            std::shared_ptr<swss::Table> m_flexCounterTable;
            std::shared_ptr<swss::Table> m_flexCounterGroupTable;

            std::shared_ptr<NotificationProducerBase> m_notifications;

            std::shared_ptr<sairedis::SwitchConfigContainer> m_switchConfigContainer;
            std::shared_ptr<sairedis::RedisVidIndexGenerator> m_redisVidIndexGenerator;
            std::shared_ptr<sairedis::VirtualObjectIdManager> m_virtualObjectIdManager;

            std::shared_ptr<sairedis::ContextConfig> m_contextConfig;

            std::shared_ptr<BreakConfig> m_breakConfig;

            TimerWatchdog m_timerWatchdog;

            std::set<sai_object_id_t> m_createdInInitView;
    };
}
